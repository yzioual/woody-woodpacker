#include "../include/woody_woodpacker.h"
#include <sys/random.h>
#include <string.h>


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

void    print_key(const unsigned char *str, size_t size) {
    printf("key :");
    for (size_t i = 0; i < size; i++) {
        printf("%02X", str[i]);
    }
    printf("\n");
}


// function not mine -> wikipedia page
void    chacha_block(uint32_t out[16], uint32_t const in[16])
{
	int         i;
	uint32_t    x[16];

	for (i = 0; i < 16; ++i)
		x[i] = in[i];
	// 10 loops × 2 rounds/loop = 20 rounds
	for (i = 0; i < ROUNDS; i += 2) {
		// Odd round
		QR(x[0], x[4], x[ 8], x[12]); // column 1
		QR(x[1], x[5], x[ 9], x[13]); // column 2
		QR(x[2], x[6], x[10], x[14]); // column 3
		QR(x[3], x[7], x[11], x[15]); // column 4
		// Even round
		QR(x[0], x[5], x[10], x[15]); // diagonal 1 (main diagonal)
		QR(x[1], x[6], x[11], x[12]); // diagonal 2
		QR(x[2], x[7], x[ 8], x[13]); // diagonal 3
		QR(x[3], x[4], x[ 9], x[14]); // diagonal 4
	}
	for (i = 0; i < 16; ++i)
		out[i] = x[i] + in[i];
}

uint32_t    bytes_to_word_le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// maybe bugs in this one, don't have time to test
uint32_t    *to_words_chacha20(const unsigned char *rand_key, const unsigned char *rand_nonce, uint32_t *out) {
    const unsigned char constant_initial_state[16] = "expand 32-byte k"; // yes it's the real constant for the algorithm.
    size_t i = 0; // to iterate throug out;

    for (int index = 0; index < 16; index += 4) out[i++] = bytes_to_word_le(constant_initial_state + index);
    for (int index = 0; index < KEY_SIZE; index += 4) out[i++] = bytes_to_word_le(rand_key + index);
    out[i++] = COUNTER_START;
    for (int index = 0; index < NONCE_SIZE; index += 4) out[i++] = bytes_to_word_le(rand_nonce + index);

    return (out);
}

void    words_to_bytes(uint32_t *ribbon, unsigned char *out) {
    size_t  index_ribbon = 0;
    for (int index_out = 0; index_out < 64; index_out += 4) {
        out[index_out] = ribbon[index_ribbon];
        out[index_out + 1] = (ribbon[index_ribbon] >> 8) & 0xFF;
        out[index_out + 2] = (ribbon[index_ribbon] >> 16) & 0xFF;
        out[index_out + 3] = (ribbon[index_ribbon] >> 24) & 0xFF;
        index_ribbon++;
    }
}

/*
 * initial state of the algorithm = 512bits -> 16 words of 32 bits (4*4 matrix)
 * -> 4 words of constant
 * -> 8 words for the key
 * -> 1 for counter
 * -> 3 for nonce
 * uint32_t x[16];
*/

// ADD RAND_KEY TO ARG, DONT FORGET
void    chacha20(unsigned char *data, size_t len, unsigned char *key, size_t key_len) {
    unsigned char   rand_nonce[NONCE_SIZE + 1] = "nonce propre"; // can be random or public, in our case he's public

    // syscall 318 (getrandom) fills a buffer pointed with random values between 0 to 255
    print_key(key, key_len);

    uint32_t        words[16];
    uint32_t        ribbon[16];
    size_t          index = 0;
    unsigned char   ribbon_bytes[64];

    to_words_chacha20(key, rand_nonce, words); // turn all of our initial state into WORDS of 32 bits
    while (index < len) {
        chacha_block(ribbon, words);
        words_to_bytes(ribbon, ribbon_bytes); 
        words[12]++;
        for (int j = 0; j < 64 && index < len; j++) {
            data[index] ^= ribbon_bytes[j];
            index++;
        }
    }
    printf("voila : \n");
    print_key(data, len);
}
