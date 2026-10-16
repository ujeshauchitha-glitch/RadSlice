/* A small CoreMark-style mix: linked-list traversal, a matrix multiply, a CRC, and a
 * byte-stream state machine. It is NOT CoreMark and its score means nothing; it
 * exists to give the fault campaigns a pointer-chasing, arithmetic-heavy and
 * branchy integer workload. Multiplies are done by shift-and-add so the program needs
 * no libgcc on RV64I. */
#include "common.h"

#define NODES 64
#define DIM 6
#define BUF 256

static u32 seed = 0x2545F491u;
static u16 next_idx[NODES];
static u16 value[NODES];
static u16 ma[DIM][DIM], mb[DIM][DIM];
static u8  buf[BUF];

static u32 mul16(u32 a, u32 b) {          /* 16-bit x 16-bit, shift-and-add */
    u32 r = 0;
    for (int i = 0; i < 16; ++i) {
        if (b & 1) r += a;
        a <<= 1;
        b >>= 1;
    }
    return r;
}

static u32 list_phase(void) {
    /* a single cycle through all nodes, in a pseudo-random order */
    /* i -> 37*i + 5 (mod 64) is a permutation because 37 is odd; 37*i is shifts and adds */
    u16 order[NODES];
    for (int i = 0; i < NODES; ++i) order[i] = (u16)(((i << 5) + (i << 2) + i + 5) & (NODES - 1));
    for (int i = 0; i < NODES; ++i) {
        next_idx[order[i]] = order[(i + 1) & (NODES - 1)];
        value[order[i]] = (u16)xorshift32(&seed);
    }
    u32 acc = 0;
    u16 p = order[0];
    for (int pass = 0; pass < 4; ++pass)
        for (int i = 0; i < NODES; ++i) {
            acc = ((acc << 3) | (acc >> 29)) ^ value[p];
            p = next_idx[p];
        }
    return acc;
}

static u32 matrix_phase(void) {
    for (int i = 0; i < DIM; ++i)
        for (int j = 0; j < DIM; ++j) {
            ma[i][j] = (u16)(xorshift32(&seed) & 0x7fff);
            mb[i][j] = (u16)(xorshift32(&seed) & 0x7fff);
        }
    u32 sum = 0;
    for (int i = 0; i < DIM; ++i)
        for (int j = 0; j < DIM; ++j) {
            u32 c = 0;
            for (int k = 0; k < DIM; ++k) c += mul16(ma[i][k], mb[k][j]);
            sum ^= c + (u32)(i * DIM + j);
        }
    return sum;
}

static u16 crc16(const u8 *p, int n) {     /* CRC-16/CCITT, bitwise */
    u16 crc = 0xffff;
    for (int i = 0; i < n; ++i) {
        crc ^= (u16)((u16)p[i] << 8);
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? (u16)((crc << 1) ^ 0x1021) : (u16)(crc << 1);
    }
    return crc;
}

static u32 state_phase(void) {
    /* count tokens in a byte stream: runs of digits, runs of letters, separators */
    enum { S_IDLE, S_NUM, S_ALPHA } st = S_IDLE;
    u32 nums = 0, words = 0, seps = 0;
    for (int i = 0; i < BUF; ++i) {
        u8 c = buf[i] & 0x3f;
        int cls = c < 16 ? 0 : (c < 40 ? 1 : 2);
        switch (st) {
            case S_IDLE:  if (cls == 1) { st = S_NUM; ++nums; } else if (cls == 2) { st = S_ALPHA; ++words; } else ++seps; break;
            case S_NUM:   if (cls == 0) { st = S_IDLE; ++seps; } else if (cls == 2) { st = S_ALPHA; ++words; } break;
            case S_ALPHA: if (cls == 0) { st = S_IDLE; ++seps; } else if (cls == 1) { st = S_NUM; ++nums; } break;
        }
    }
    return (nums << 20) ^ (words << 10) ^ seps;
}

int main(void) {
    u32 a = list_phase();
    u32 b = matrix_phase();
    for (int i = 0; i < BUF; ++i) buf[i] = (u8)xorshift32(&seed);
    u32 c = crc16(buf, BUF);
    u32 d = state_phase();

    RESULT[0] = a;
    RESULT[1] = b;
    RESULT[2] = c;
    RESULT[3] = d;
    RESULT[4] = ((u64)a << 32) ^ b ^ ((u64)c << 16) ^ d;
    return 0;
}
