#pragma once

#include <stdio.h>
#include <unistd.h>

// import libft here ?

#define eprintf(...) fprintf (stderr, __VA_ARGS__) // used to print on stderr

void chacha20(char *str);
