#include "../include/woody_woodpacker.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        eprintf("./woody_woodpacker <elf64 binary file>");
        return (1);
    }

    return (0);
}