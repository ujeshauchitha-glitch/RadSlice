# Start-up and exit for the workloads.
#
# Exit protocol (HTIF, as riscv-tests): writing (code << 1) | 1 to `tohost` stops the
# run. main() returning means success: code 0. A workload that detects an unexpected
# trap writes code 0x100 (see crash() in common.h), which the harness reads as a crash.

    .section .text.init
    .globl _start
_start:
    li      sp, 0x80180000
    call    main
    la      t0, tohost
    li      t1, 1
    sd      t1, 0(t0)
1:  j       1b

    .section .tohost,"aw",@progbits
    .align 6
    .globl tohost
tohost:   .dword 0
    .globl fromhost
fromhost: .dword 0
