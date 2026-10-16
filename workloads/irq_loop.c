/* An interrupt-heavy loop: machine-timer interrupts every PERIOD ticks while main()
 * does checksum work. Each interrupt saves and restores sixteen registers through
 * irq_entry (crt0.S) and runs a C handler, so the register file is busy, CSRs are
 * exercised, and a corrupted return address or stack word shows up as a hang or crash
 * rather than a quietly wrong sum. The timer ticks once per retired instruction in
 * the harness, so a fault-free run is exactly reproducible.
 *
 * Needs Zicsr (csrr/csrw), so this one is built with -march=rv64i_zicsr. */
#include "common.h"

#define CLINT_MTIMECMP (*(volatile u64 *)0x02004000UL)
#define CLINT_MTIME    (*(volatile u64 *)0x0200BFF8UL)
#define PERIOD         400
#define TARGET_IRQS    60

extern void irq_entry(void);

static volatile u64 irq_count;
static volatile u64 irq_mix;
static volatile u64 main_sum;

static inline u64 read_mcause(void) {
    u64 v;
    __asm__ volatile("csrr %0, mcause" : "=r"(v));
    return v;
}

void irq_handler(void) {
    const u64 cause = read_mcause();
    /* bit 63 = interrupt, code 7 = machine timer. Anything else is not ours. */
    if (cause != ((1ull << 63) | 7)) crash();
    irq_count = irq_count + 1;
    irq_mix = (irq_mix << 5) ^ (irq_mix >> 59) ^ irq_count ^ (CLINT_MTIME & 0xff);
    CLINT_MTIMECMP = CLINT_MTIMECMP + PERIOD;
}

int main(void) {
    __asm__ volatile("csrw mtvec, %0" ::"r"((u64)irq_entry));
    CLINT_MTIMECMP = CLINT_MTIME + PERIOD;
    __asm__ volatile("csrs mie, %0" ::"r"(1u << 7));        /* MTIE */
    __asm__ volatile("csrs mstatus, %0" ::"r"(1u << 3));    /* MIE  */

    u32 seed = 0x9e3779b9u;
    u32 acc = 0;
    while (irq_count < TARGET_IRQS) {
        /* a burst of work between interrupts */
        for (int i = 0; i < 16; ++i) {
            u32 x = xorshift32(&seed);
            acc = ((acc << 7) | (acc >> 25)) + (x ^ (acc >> 3));
        }
        main_sum = acc;
    }
    __asm__ volatile("csrc mstatus, %0" ::"r"(1u << 3));    /* stop taking interrupts */

    RESULT[0] = irq_count;
    RESULT[1] = irq_mix;
    RESULT[2] = main_sum;
    RESULT[3] = ((u64)acc << 32) ^ seed;
    return 0;
}
