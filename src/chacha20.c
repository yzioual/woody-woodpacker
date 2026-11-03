#include "../include/woody_woodpacker.h"
#define KEY_SIZE (256 / 8)

char    *chacha20(char *str) {
    // first create a random key with syscall
    char rand_key[KEY_SIZE];
    syscall(318, rand_key, KEY_SIZE, 0);
    printf("rand_key : |%s|\n", rand_key); // used for test

    return (NULL);
}
