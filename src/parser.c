#include "woody_woodpacker.h"

size_t get_file_size(int fd) {
    if (fd < 0) {
        return -1;
    }

    struct stat st;
    if (syscall(SYS_fstat, fd, &st) == 0) {
        return st.st_size;
    }

    close(fd);
    return -1;
}

unsigned char *map_file(int fd, size_t *out_len) {
    unsigned char* data = NULL;
    *out_len = get_file_size(fd);

    data = mmap(NULL, *out_len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("open/mmap");
        return NULL;
    }

    return data;
}

static bool is_elf64(unsigned char *buffer, size_t len) {
    if (buffer == NULL) {
        printf("buffer is null\n");
        return false;
    }

    if (len < sizeof(Elf64_Ehdr)) {
        return 0;
    }

    if (buffer[EI_MAG0] != ELFMAG0 || buffer[EI_MAG1] != ELFMAG1 ||
        buffer[EI_MAG2] != ELFMAG2 || buffer[EI_MAG3] != ELFMAG3)
        return 0;

    if (buffer[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "error: not a valid 64-bit ELF file\n");
        return 0;
    }

    return 1;
}

unsigned char* parse_elf(const char *path, size_t *file_size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        return NULL;
    }

    unsigned char *mapped_data = map_file(fd, file_size);
    if (mapped_data == MAP_FAILED) {
        printf("mmap failed\n");
        return NULL;
    }

    if (is_elf64(mapped_data, *file_size) == false) {
        printf("Yikes! It is not an elf\n");
        return NULL;
    }

    return mapped_data;
}
