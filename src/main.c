#include "../include/woody_woodpacker.h"

int main(int argc, char **argv) {
	if (argc != 2) {
		eprintf("./woody_woodpacker <elf64 binary file>\n");
		return (1);
	}

  unsigned char *data = parse_elf(argv[1]);
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

  unsigned char *target = data + target_ph->p_offset;
  size_t target_len = target_ph->p_filesz;

  printf("found target segment: offset=0x%lx vaddr=0x%lx filesz=%lu\n",
          target_ph->p_offset, target_ph->p_vaddr, target_len);

  (void) target;
  return (0);
}
