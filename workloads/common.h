#pragma once
/* Shared definitions for the bare-metal workloads (RV64I, M-mode, no libc). */

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

/* A fixed window the harness compares against the golden run. */
#define RESULT ((volatile u64 *)0x80100000UL)

extern volatile u64 tohost;

/* Stop the run reporting failure code `code` (non-zero). */
static inline void fail_exit(u64 code) {
    tohost = (code << 1) | 1;
    for (;;) {}
}

/* A workload that finds itself in an impossible state exits with this code; the
 * harness classifies it as a crash. */
#define CRASH_CODE 0x100
static inline void crash(void) { fail_exit(CRASH_CODE); }

/* A small deterministic generator (xorshift32): no multiply, so no libgcc. */
static inline u32 xorshift32(u32 *s) {
    u32 x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
