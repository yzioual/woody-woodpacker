static long my_write(int fd, const void *buf, unsigned long count);
static void my_exit(int code);
static void xor_crypt(unsigned char *data, unsigned long len, const unsigned char *key, unsigned long key_len);
static void jump_to(unsigned long addr, unsigned long orig_rsp, unsigned long orig_rdx);
static long my_mprotect(unsigned long addr, unsigned long len, long prot);
static void chacha20(unsigned char *data, unsigned int len, unsigned char *key);
static void lzss_decompress(const unsigned char *src, unsigned long src_len,
                            unsigned char *dst, unsigned long dst_len);

#include <stdint.h>
#define KEY_SIZE (256 / 8)
#define NONCE_SIZE (96 / 8)
#define COUNTER_START 1 // can also start at 0 but for our case it's 1
#define ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define QR(a, b, c, d) (             \
	a += b, d ^= a, d = ROTL(d, 16), \
	c += d, b ^= c, b = ROTL(b, 12), \
	a += b, d ^= a, d = ROTL(d,  8), \
	c += d, b ^= c, b = ROTL(b,  7))
#define ROUNDS 20

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
    volatile unsigned long page_len   = 0x7777777777777777UL;  //
    volatile unsigned long orig_target_len = 0x8888888888888888UL; // [PATCH] original uncompressed len
                                                               //
    my_mprotect(page_addr, page_len, 7);

    if (orig_target_len > target_len) {
        /* compressed path: blob lives in stub segment right after the key */
        unsigned long compressed_addr = key_addr + key_len;
        chacha20((unsigned char *) compressed_addr, target_len, (unsigned char *) key_addr);
        lzss_decompress((unsigned char *) compressed_addr, target_len,
                        (unsigned char *) target_addr, orig_target_len);
    } else {
        /* uncompressed path: encrypted data is in .text, decrypt in place */
        chacha20((unsigned char *) target_addr, target_len, (unsigned char *) key_addr);
    }

    jump_to(orig_entry, orig_rsp, orig_rdx);
}

static void    chacha_block(uint32_t out[16], uint32_t const in[16])
{
	int         i;
	uint32_t    x[16];

	for (i = 0; i < 16; ++i)
		x[i] = in[i];
	for (i = 0; i < ROUNDS; i += 2) {
		QR(x[0], x[4], x[ 8], x[12]); // column 1
		QR(x[1], x[5], x[ 9], x[13]); // column 2
		QR(x[2], x[6], x[10], x[14]); // column 3
		QR(x[3], x[7], x[11], x[15]); // column 4
		QR(x[0], x[5], x[10], x[15]); // diagonal 1 (main diagonal)
		QR(x[1], x[6], x[11], x[12]); // diagonal 2
		QR(x[2], x[7], x[ 8], x[13]); // diagonal 3
		QR(x[3], x[4], x[ 9], x[14]); // diagonal 4
	}
	for (i = 0; i < 16; ++i)
		out[i] = x[i] + in[i];
}

static uint32_t    bytes_to_word_le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t    *to_words_chacha20(const unsigned char *rand_key, const unsigned char *rand_nonce, uint32_t *out) {
    const unsigned char constant_initial_state[17] = "expand 32-byte k";
    unsigned int i = 0; 

    for (int index = 0; index < 16; index += 4) out[i++] = bytes_to_word_le(constant_initial_state + index);
    for (int index = 0; index < KEY_SIZE; index += 4) out[i++] = bytes_to_word_le(rand_key + index);
    out[i++] = COUNTER_START;
    for (int index = 0; index < NONCE_SIZE; index += 4) out[i++] = bytes_to_word_le(rand_nonce + index);

    return (out);
}

static void    words_to_bytes(uint32_t *ribbon, unsigned char *out) {
    unsigned int  index_ribbon = 0;
    for (int index_out = 0; index_out < 64; index_out += 4) {
        out[index_out] = ribbon[index_ribbon];
        out[index_out + 1] = (ribbon[index_ribbon] >> 8) & 0xFF;
        out[index_out + 2] = (ribbon[index_ribbon] >> 16) & 0xFF;
        out[index_out + 3] = (ribbon[index_ribbon] >> 24) & 0xFF;
        index_ribbon++;
    }
}

static void    chacha20(unsigned char *data, unsigned int len, unsigned char *key) {
    unsigned char   rand_nonce[NONCE_SIZE + 1] = "nonce propre";

    uint32_t        words[16];
    uint32_t        ribbon[16];
    unsigned int          index = 0;
    unsigned char   ribbon_bytes[64];

    to_words_chacha20(key, rand_nonce, words);
    while (index < len) {
        chacha_block(ribbon, words);
        words_to_bytes(ribbon, ribbon_bytes); 
        words[12]++;
        for (int j = 0; j < 64 && index < len; j++) {
            data[index] ^= ribbon_bytes[j];
            index++;
        }
    }
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

/*
 * LZSS Decompressor — mirrors the encoder in src/lzss.c
 *
 *   flag byte  →  8 items
 *     bit = 1  →  1 literal byte
 *     bit = 0  →  2 bytes  (12-bit offset | 4-bit length+3)
 */
#define LZSS_MIN_MATCH 3

static void lzss_decompress(const unsigned char *src, unsigned long src_len,
                             unsigned char *dst, unsigned long dst_len)
{
    unsigned long si = 0, di = 0;

    while (si < src_len && di < dst_len) {
        unsigned char flags = src[si++];

        for (int bit = 0; bit < 8 && si < src_len && di < dst_len; bit++) {
            if (flags & (1 << bit)) {
                /* literal */
                dst[di++] = src[si++];
            } else {
                if (si + 1 >= src_len)
                    return;
                unsigned int b0 = src[si++];
                unsigned int b1 = src[si++];
                unsigned int offset = ((b0 << 4) | (b1 >> 4)) + 1;
                unsigned int length = (b1 & 0x0F) + LZSS_MIN_MATCH;

                for (unsigned int k = 0; k < length && di < dst_len; k++) {
                    dst[di] = dst[di - offset];
                    di++;
                }
            }
        }
    }
}
