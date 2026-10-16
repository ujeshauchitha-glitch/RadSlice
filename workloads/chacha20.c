/* ChaCha20 (RFC 8439) keystream generation. Pure add / xor / rotate on 32-bit words,
 * so on RV64I it is dominated by the W-form instructions (ADDW, XOR, SLLIW/SRLIW).
 * A crypto kernel is a good fault target: one flipped bit anywhere diffuses through
 * the whole output, so silent corruption cannot hide. The key and nonce are the
 * RFC's section 2.3.2 test inputs; the harness compares against the golden core, not
 * against a hard-coded vector. */
#include "common.h"

#define BLOCKS 24

static inline u32 rotl(u32 v, int c) { return (v << c) | (v >> (32 - c)); }

#define QR(a, b, c, d)                  \
    a += b; d ^= a; d = rotl(d, 16);    \
    c += d; b ^= c; b = rotl(b, 12);    \
    a += b; d ^= a; d = rotl(d, 8);     \
    c += d; b ^= c; b = rotl(b, 7);

static void chacha_block(u32 out[16], const u32 in[16]) {
    u32 x[16];
    for (int i = 0; i < 16; ++i) x[i] = in[i];
    for (int r = 0; r < 10; ++r) {
        QR(x[0], x[4], x[8],  x[12]);
        QR(x[1], x[5], x[9],  x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8],  x[13]);
        QR(x[3], x[4], x[9],  x[14]);
    }
    for (int i = 0; i < 16; ++i) out[i] = x[i] + in[i];
}

int main(void) {
    u32 st[16];
    st[0] = 0x61707865; st[1] = 0x3320646e; st[2] = 0x79622d32; st[3] = 0x6b206574;
    for (int i = 0; i < 8; ++i)             /* key bytes 00 01 02 ... 1f, little-endian words */
        st[4 + i] = (u32)(4 * i) | ((u32)(4 * i + 1) << 8) | ((u32)(4 * i + 2) << 16) | ((u32)(4 * i + 3) << 24);
    st[12] = 1;                              /* block counter */
    st[13] = 0x09000000; st[14] = 0x4a000000; st[15] = 0x00000000;   /* nonce */

    u32 out[16], fold[16];
    for (int i = 0; i < 16; ++i) fold[i] = 0;
    u32 sum = 0;
    for (int blk = 0; blk < BLOCKS; ++blk) {
        chacha_block(out, st);
        for (int i = 0; i < 16; ++i) { fold[i] ^= out[i]; sum += out[i]; }
        if (blk == 0)
            for (int i = 0; i < 8; ++i) RESULT[8 + i] = ((u64)out[2 * i + 1] << 32) | out[2 * i];
        st[12]++;
    }
    for (int i = 0; i < 8; ++i) RESULT[i] = ((u64)fold[2 * i + 1] << 32) | fold[2 * i];
    RESULT[16] = sum;
    return 0;
}
