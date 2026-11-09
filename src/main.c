#include "../include/woody_woodpacker.h"
#include <elf.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#define PAGE_SIZE 0x1000

#define KEY_SIZE (256 / 8)

static  size_t find_placeholder(unsigned char *buf, size_t len, uint64_t marker) {
    for (size_t i = 0; i + 8 <= len; i++) {
        if (memcmp(buf + i, &marker, 8) == 0)
            return i;
    }
    return (size_t)-1;
}

int write_woody(unsigned char *data, size_t len) {
    int fd = open("woody", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd < 0) {
        perror("open woody");
        return -1;
    }

    ssize_t written = write(fd, data, len);
    if (written < 0 || (size_t)written != len) {
        perror("write woody");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static void patch_u64(unsigned char *buf, size_t offset, uint64_t value) {
    memcpy(buf + offset, &value, sizeof(uint64_t));
}

int main(int argc, char **argv) {
    if (argc != 2) {
        eprintf("./woody_woodpacker <elf64 binary file>\n");
        return (1);
    }

    int stub_fd = open("stub.bin", O_RDWR);
    if (stub_fd < 0) {
        perror("open failed");
        return (1);
    }

    size_t stub_file_size;
    unsigned char *stub = map_file(stub_fd, &stub_file_size);

    long off_target     = find_placeholder(stub, stub_file_size, 0x1111111111111111ULL);
    long off_target_len = find_placeholder(stub, stub_file_size, 0x2222222222222222ULL);
    long off_key        = find_placeholder(stub, stub_file_size, 0x3333333333333333ULL);
    long off_key_len    = find_placeholder(stub, stub_file_size, 0x4444444444444444ULL);
    long off_orig_entry = find_placeholder(stub, stub_file_size, 0x5555555555555555ULL);
    long off_page_addr = find_placeholder(stub, stub_file_size, 0x6666666666666666ULL);
    long off_page_len  = find_placeholder(stub, stub_file_size, 0x7777777777777777ULL);

    if (off_page_addr < 0 || off_page_len < 0) {
        fprintf(stderr, "failed to locate mprotect placeholders in stub\n");
        exit(EXIT_FAILURE);
    }

    if (off_target < 0 || off_target_len < 0 || off_key < 0 ||
            off_key_len < 0 || off_orig_entry < 0) {
        fprintf(stderr, "failed to locate one or more placeholders in stub\n");
        exit(EXIT_FAILURE);
    }

    size_t filesz;
    unsigned char *data = parse_elf(argv[1], &filesz);
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *) data;
    Elf64_Phdr *phdrs = (Elf64_Phdr *) (data + ehdr->e_phoff);
    Elf64_Phdr *target_ph = NULL;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *ph = &phdrs[i];

        if (ph->p_type != PT_LOAD)
            continue;
        if (!(ph->p_flags & PF_X))
            continue;
        if (ehdr->e_entry < ph->p_vaddr ||
                ehdr->e_entry >= ph->p_vaddr + ph->p_memsz)
            continue;

        target_ph = ph;
        break;
    }

    if (!target_ph) {
        eprintf("could not find PT_LOAD segment containing entry point\n");
        return (1);
    }

    // compute where we want to inject the stub into memory
    uint64_t highest_end = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
            if (end > highest_end)
                highest_end = end;
        }
    }
    uint64_t stub_vaddr = (highest_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t stub_entry_vaddr = stub_vaddr; // _start is at offset 0 (first function in stub.c)


    uint64_t entry_offset_in_segment = ehdr->e_entry - target_ph->p_vaddr;
    uint64_t runtime_target_addr = target_ph->p_vaddr + entry_offset_in_segment;

    unsigned char *target = data + target_ph->p_offset + entry_offset_in_segment;
    size_t target_len = target_ph->p_filesz - entry_offset_in_segment;

    unsigned char key[KEY_SIZE + 1] = {0};
    syscall(SYS_getrandom, key, KEY_SIZE, 0);

    size_t key_len = sizeof(key) - 1;

    chacha20(target, target_len, key);

    uint64_t page_addr = runtime_target_addr & ~((uint64_t)PAGE_SIZE - 1);
    uint64_t page_end   = (runtime_target_addr + target_len + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);
    uint64_t page_len   = page_end - page_addr;

    patch_u64(stub, off_page_addr, page_addr);
    patch_u64(stub, off_page_len,  page_len);

    patch_u64(stub, off_target, runtime_target_addr);
    patch_u64(stub, off_target_len, target_len);

    uint64_t key_rtime_addr = stub_vaddr + stub_file_size;
    patch_u64(stub, off_key,        key_rtime_addr);
    patch_u64(stub, off_key_len,    key_len);
    patch_u64(stub, off_orig_entry, ehdr->e_entry);

    size_t stub_blob_len = stub_file_size + key_len;
    uint64_t stub_file_offset = (filesz + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);

    int old_phnum = ehdr->e_phnum;
    int new_phnum = old_phnum + 1;

    size_t phdr_table_size = new_phnum * sizeof(Elf64_Phdr);
    uint64_t new_phdr_offset = stub_file_offset + stub_blob_len;
    size_t final_size = stub_file_offset + stub_blob_len + phdr_table_size;

    // ALLOCATE MEMORY for the new final buffer
    unsigned char* final_piece = malloc(final_size);
    if (final_piece == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // 1. copy original file content (headers + all original segments, with encrypted code)
    memcpy(final_piece, data, filesz);
    memset(final_piece + filesz, 0, stub_file_offset - filesz);

    // 2. copy the patched stub right after it
    memcpy(final_piece + stub_file_offset, stub, stub_file_size);
    // 3. copy the key right after the stub
    memcpy(final_piece + stub_file_offset + stub_file_size, key, key_len);

    Elf64_Phdr *old_phdrs_in_final = (Elf64_Phdr *)(final_piece + ehdr->e_phoff);
    Elf64_Phdr *new_phdr_table = (Elf64_Phdr *)(final_piece + new_phdr_offset);

    // copy existing entries over, unchanged
    memcpy(new_phdr_table, old_phdrs_in_final, old_phnum * sizeof(Elf64_Phdr));

    // build the new entry for our stub+key segment
    Elf64_Phdr *stub_phdr = &new_phdr_table[old_phnum];
    stub_phdr->p_type   = PT_LOAD;
    stub_phdr->p_offset = stub_file_offset;
    stub_phdr->p_vaddr  = stub_vaddr;
    size_t total_appended_len = stub_blob_len + phdr_table_size;
    stub_phdr->p_filesz = total_appended_len;
    stub_phdr->p_memsz  = total_appended_len;
    stub_phdr->p_flags  = PF_R | PF_X;
    stub_phdr->p_align  = PAGE_SIZE;

    Elf64_Ehdr *final_ehdr = (Elf64_Ehdr *)final_piece;

    final_ehdr->e_phoff = new_phdr_offset;
    final_ehdr->e_phnum = new_phnum;
    final_ehdr->e_entry = stub_entry_vaddr;

    write_woody(final_piece, final_size);
    return (0);
}
