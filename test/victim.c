static long my_write(int fd, const void *buf, unsigned long count) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(1), "D"(fd), "S"(buf), "d"(count) : "rcx", "r11","memory");
    return ret;
}

static void my_exit(int code) {
    __asm__ volatile ("syscall" : : "a"(60), "D"(code) : "memory");
    __builtin_unreachable();
}

void _start(void) {
    my_write(1, "it works!\n", 10);
    my_exit(0);
}
