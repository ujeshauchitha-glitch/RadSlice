# A fixed ALU-heavy loop (adds, logic, shifts, compares, W forms, one load and one store
# per iteration, a data-dependent branch). Used only to put a number on the cycle
# model for a compute-bound instruction mix; it has no meaning beyond that.
    .section .text.init
    .globl _start
_start:
    li   s0, 0x80100000        # scratch buffer
    li   t0, 12345
    li   t1, 6789
    li   t2, 1
1:
    add  t3, t0, t1
    sub  t4, t3, t2
    xor  t0, t3, t4
    slli t1, t0, 3
    srli t5, t1, 5
    srai t6, t1, 7
    slt  a0, t5, t6
    sltu a1, t6, t5
    addw a2, t3, t4
    addiw a3, a2, 17
    and  a4, a2, a3
    or   a5, a4, a0
    sd   a5, 0(s0)
    ld   a6, 0(s0)
    add  t1, t1, a6
    addi t2, t2, 1
    andi a7, t2, 3
    beq  a7, zero, 2f
    sllw t0, t0, a7
2:
    bltu t2, t1, 1b
    j    1b
