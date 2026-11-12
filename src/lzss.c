#include "woody_woodpacker.h"

/*
 * LZSS Compression
 *
 * Format: stream of flag bytes, each followed by up to 8 items.
 *   flag bit = 1 → literal byte
 *   flag bit = 0 → 2-byte (offset, length) match pair
 *       offset: 12 bits  (1..4096 lookback)
 *       length:  4 bits  (3..18  match length)
 *
 * Returns malloc'd buffer; caller frees.  *out_len = compressed size.
 */

#define LZSS_WINDOW   4096
#define LZSS_MIN_MATCH   3
#define LZSS_MAX_MATCH  18

unsigned char *lzss_compress(const unsigned char *src, unsigned long src_len,
                             unsigned long *out_len)
{
    /* worst case: every byte is a literal → 1 flag byte per 8 literals */
    unsigned long max_out = src_len + (src_len / 8) + 16;
    unsigned char *dst = malloc(max_out);
    if (!dst)
        return NULL;

    unsigned long si = 0, di = 0;

    while (si < src_len) {
        unsigned long flag_pos = di++;      /* reserve space for flag byte */
        unsigned char flags = 0;

        for (int bit = 0; bit < 8 && si < src_len; bit++) {
            /* search for the longest match in the sliding window */
            unsigned long best_off = 0, best_len = 0;
            unsigned long win_start = (si > LZSS_WINDOW) ? si - LZSS_WINDOW : 0;

            for (unsigned long j = win_start; j < si; j++) {
                unsigned long mlen = 0;
                while (mlen < LZSS_MAX_MATCH &&
                       si + mlen < src_len   &&
                       src[j + mlen] == src[si + mlen])
                    mlen++;
                if (mlen > best_len) {
                    best_len = mlen;
                    best_off = si - j;
                }
            }

            if (best_len >= LZSS_MIN_MATCH) {
                /* match → flag bit stays 0 */
                unsigned int off_enc = (unsigned int)(best_off - 1);   /* 0..4095 */
                unsigned int len_enc = (unsigned int)(best_len - LZSS_MIN_MATCH); /* 0..15 */
                dst[di++] = (off_enc >> 4) & 0xFF;
                dst[di++] = ((off_enc & 0x0F) << 4) | (len_enc & 0x0F);
                si += best_len;
            } else {
                /* literal → flag bit = 1 */
                flags |= (1 << bit);
                dst[di++] = src[si++];
            }
        }
        dst[flag_pos] = flags;
    }

    *out_len = di;
    return dst;
}
