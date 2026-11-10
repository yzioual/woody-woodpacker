#pragma once

#include <stdio.h>
#include <unistd.h>
#include <elf.h>
#include <stdbool.h>
#include <sys/syscall.h>
#include <stdint.h>

#define KEY_SIZE (256 / 8)

#define eprintf(...) fprintf (stderr, __VA_ARGS__) // used to print on stderr

void    chacha20(unsigned char *, unsigned int, unsigned char *);
size_t get_file_size(int);
unsigned char* parse_elf(const char *, size_t*);
unsigned char *map_file(int, size_t*);
