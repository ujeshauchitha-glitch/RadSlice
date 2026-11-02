# Running the core on the DE2-115

Status: the design is verified in simulation only (Icarus Verilog): the core matches the golden emulator on two
programs, and the whole board wrapper passes a simulated session (`bash scripts/run-board-sim.sh`). It has **not** been
built with Quartus, has **not** been timed, and has **not** been run on a board. Whether it fits the FPGA and meets
timing is unknown until Quartus reports it (see G18 in `gaps.md`; the register file was the large part in the
generic synthesis estimate).

## Build
1. Install Quartus Prime Lite (free) with Cyclone IV device support, and the USB-Blaster driver.
2. Open `rtl/de2_115/de2_115.qpf` (project `de2_115`). `de2_115.qsf` holds the device, files and pins; `de2_115.sdc` the clock.
3. If you change the demo program: `bash scripts/build-demo.sh` rewrites `rtl/de2_115/demo.hex`.
4. Compile, then program the board over USB-Blaster (Tools, Programmer, `output_files/de2_115.sof`).

Pin locations come from a published DE2-115 pin table, not from Terasic's manual: compare a few against the manual.
Before programming, check that jumpers **JP6 and JP7** are in the 3.3 V position; the 3.3-V LVTTL assignments assume it.

## Using it
- The counter on the four seven-segment displays is register x6, held in the slice-serial register file.
- Red LEDs mirror the 18 switches. Green LEDs[7:0] count parity errors seen by the register file; LEDG[8] lights if the core halts.
- KEY0 resets. KEY1 flips one stored bit: set SW[4:0] to the register (6 for the counter) and SW[10:5] to the bit, then press.
  The counter jumps by that bit and the green LEDs count a parity error. The error is detected, not repaired.
- KEY2 sets a stuck-bit defect on a physical slice: SW[8:0] is the slice (register x6's first byte is slice 48), SW[11:9] the bit,
  SW[12] the stuck value. With SW[17] = 1 it clears that slice's defect instead.
- Not wired yet: the boot self-test and remap that would repair a defective slice (the register file has the remap port).

## Verified so far
`run-board-sim.sh` drives the switches and keys in simulation: the counter counts, a flipped bit moves the counter by that bit and
raises the parity count, and a stuck bit raises it. That is the design behaving as designed, not a board measurement.
