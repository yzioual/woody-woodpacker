static long my_write(int fd, const void *buf, unsigned long count);
static void my_exit(int code);
static void jump_to(unsigned long addr, unsigned long orig_rsp, unsigned long orig_rdx);
static long my_mprotect(unsigned long addr, unsigned long len, long prot);
void    chacha20(unsigned char *, long unsigned int, unsigned char *);

void _start(void) {
    // Capture the original rsp and rdx before compiler-generated code clobbers them.
    // 1. rsp: After `push rbp; mov rsp, rbp`, rbp = orig_rsp - 8, so orig_rsp = rbp + 8.
    // 2. rdx: According to System V AMD64 ABI, rdx holds a pointer to a cleanup function
    //         registered with atexit (rtld_fini), or 0 if none.
    //         If rdx is not restored (or left holding a stack pointer), glibc's _start
    //         treats it as rtld_fini and calls it upon exit(), jumping into the stack and crashing!
    unsigned long orig_rsp;
    unsigned long orig_rdx;
    __asm__ volatile ("lea 8(%%rbp), %0" : "=r"(orig_rsp));
    __asm__ volatile ("mov %%rdx, %0" : "=r"(orig_rdx));

    volatile unsigned long target_addr = 0x1111111111111111UL;  // [PATCH] encrypted data addr
    volatile unsigned long target_len  = 0x2222222222222222UL;  // [PATCH] encrypted data len
    volatile unsigned long key_addr    = 0x3333333333333333UL;  // [PATCH] key addr
    volatile unsigned long key_len     = 0x4444444444444444UL;  // [PATCH] key len
    volatile unsigned long orig_entry  = 0x5555555555555555UL;  // [PATCH] original e_entry
    volatile unsigned long page_addr  = 0x6666666666666666UL;  // [PATCH] page-aligned addr
    volatile unsigned long page_len   = 0x7777777777777777UL;

    my_mprotect(page_addr, page_len, 7);
    chacha20((unsigned char*) target_addr, target_len, (unsigned char*) key_addr);
    jump_to(orig_entry, orig_rsp, orig_rdx);
}

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

static void jump_to(unsigned long addr, unsigned long orig_rsp, unsigned long orig_rdx)
{
    __asm__ volatile (
        "mov %1, %%rsp\n\t"  // restore the kernel-provided stack pointer
        "mov %2, %%rdx\n\t"  // restore the kernel/rtld-provided rdx
        "xor %%rbp, %%rbp\n\t" // clear rbp as required by ABI at _start
        "jmp *%0"
        :
        : "r"(addr), "r"(orig_rsp), "r"(orig_rdx)
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
