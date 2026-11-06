static long my_write(int fd, const void *buf, unsigned long count)
{
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(1), "D"(fd), "S"(buf), "d"(count)   // 1 = sys_write
        : "rcx", "r11", "memory"
    );
    return ret;
}

static void my_exit(int code)
{
    __asm__ volatile (
        "syscall"
        :
        : "a"(60), "D"(code)   // 60 = sys_exit
        : "memory"
    );
    __builtin_unreachable();
}

static void xor_crypt(unsigned char *data, unsigned long len, const unsigned char *key, unsigned long key_len)
{
    for (unsigned long i = 0; i < len; i++) {
        data[i] ^= key[i % key_len];
    }
}

static void jump_to(unsigned long addr)
{
    __asm__ volatile (
        "jmp *%0"
        :
        : "r"(addr)
        : "memory"
    );
}

static long my_mprotect(unsigned long addr, unsigned long len, long prot)
{
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(10), "D"(addr), "S"(len), "d"(prot)   // 10 = sys_mprotect
        : "rcx", "r11", "memory"
    );
    return ret;
}

void _start(void) {
    volatile unsigned long target_addr = 0x1111111111111111UL;  // [PATCH] encrypted data addr
    volatile unsigned long target_len  = 0x2222222222222222UL;  // [PATCH] encrypted data len
    volatile unsigned long key_addr    = 0x3333333333333333UL;  // [PATCH] key addr
    volatile unsigned long key_len     = 0x4444444444444444UL;  // [PATCH] key len
    volatile unsigned long orig_entry  = 0x5555555555555555UL;  // [PATCH] original e_entry
    volatile unsigned long page_addr  = 0x6666666666666666UL;  // [PATCH] page-aligned addr
    volatile unsigned long page_len   = 0x7777777777777777UL;  //
                                                               //
    my_mprotect(page_addr, page_len, 7);
    xor_crypt((unsigned char*) target_addr, target_len, (unsigned char*) key_addr, key_len);
    jump_to(orig_entry);
}
