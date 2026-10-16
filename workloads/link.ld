/* Bare-metal M-mode workloads for the fault-injection campaigns.
 *
 * Code at the start of DRAM, where the emulator begins. Results are written to a
 * fixed 256-byte window at 0x80100000 (RESULT_BASE in common.h) so a harness can
 * compare them with the golden run without symbol tables. The stack sits at 0x80180000. */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
  . = 0x80000000;
  .text.init : { *(.text.init) }

  . = ALIGN(0x1000);
  .tohost : { *(.tohost) }

  . = ALIGN(0x1000);
  .text   : { *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) *(.srodata .srodata.*) }
  .data   : { *(.data .data.*) *(.sdata .sdata.*) }
  .bss    : { *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) }
  . = ALIGN(16);
  _end = .;
}
