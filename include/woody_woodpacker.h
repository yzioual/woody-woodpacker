#pragma once

#include <stdio.h>
#include <unistd.h>
#include <elf.h>
#include <stdbool.h>

// import libft here ?

#define eprintf(...) fprintf (stderr, __VA_ARGS__) // used to print on stderr

char    *chacha20(char *str);
void    parse_elf(const char *);
