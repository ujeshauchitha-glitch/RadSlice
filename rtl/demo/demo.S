# Demo program for the DE2-115 wrapper. A counter lives in x6, which is held in the slice-serial register file.
# It is shown on the four seven-segment displays; the switches are mirrored on the red LEDs. The green LEDs are
# driven by hardware and count parity errors: flip a bit of x6 with KEY1 (set SW[4:0] to 6 first) and the counter
# jumps while the green LEDs count up.
#ifndef DELAY
#define DELAY 20000          /* about 2 x DELAY instructions between counts */
#endif
    .section .text.init
    .globl _start
_start:
    li      sp, 0x80002000
    li      x5, 0x10000000          # I/O base
    li      x6, 0                   # the counter: this is the register to attack
1:
    sd      x6, 16(x5)              # HEX3..HEX0 <- counter
    ld      x7, 24(x5)              # switches
    sd      x7, 0(x5)               # red LEDs <- switches
    addi    x6, x6, 1
    li      x8, DELAY               # delay
2:
    addi    x8, x8, -1
    bnez    x8, 2b
    j       1b
