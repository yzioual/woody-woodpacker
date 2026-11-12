#pragma once

#include <stdio.h>
#include <unistd.h>
#include <elf.h>
#include <stdbool.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 0x1000
#define KEY_SIZE (256 / 8)

#define eprintf(...) fprintf (stderr, __VA_ARGS__) // used to print on stderr
#define NONCE_SIZE (96 / 8)
#define COUNTER_START 1 // can also start at 0 but for our case it's 1
#define ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define QR(a, b, c, d) (             \
	a += b, d ^= a, d = ROTL(d, 16), \
	c += d, b ^= c, b = ROTL(b, 12), \
	a += b, d ^= a, d = ROTL(d,  8), \
	c += d, b ^= c, b = ROTL(b,  7))
#define ROUNDS 20

void    chacha20(unsigned char *, unsigned int, unsigned char *);
unsigned char *lzss_compress(const unsigned char *src, unsigned long src_len,
                             unsigned long *out_len);
size_t get_file_size(int);
unsigned char* parse_elf(const char *, size_t*);
unsigned char *map_file(int, size_t*);
