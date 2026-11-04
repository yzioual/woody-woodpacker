#pragma once

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdint.h>

// import libft here ?

#define eprintf(...) fprintf (stderr, __VA_ARGS__) // used to print on stderr

char    *chacha20(unsigned char *data, size_t len);
