# G19 step 3: yield of the spare-and-remap MAC array

## The closed form against the chips the testbench used

Each element defective with probability q (a stuck bit); a chip is accepted if at most S elements are defective. `accepted` is read from the chip file whose accept/reject decisions the Verilog self-test reproduced exactly (0 decision mismatches in `run-mac-selftest.sh`).

| P | S | q | chips | accepted | closed form | z |
|---|---|---|---|---|---|---|
| 8 | 4 | 0.25 | 150 | 121 | 126.4 (0.842) | -1.20 |
| 8 | 2 | 0.15 | 150 | 122 | 123.0 (0.820) | -0.22 |
| 4 | 4 | 0.3 | 150 | 140 | 141.3 (0.942) | -0.46 |

## Spares needed for a 99% yield (closed form)

| per-element good probability g | P = 8 | P = 32 | P = 128 | P = 512 |
|---|---|---|---|---|
| 0.99 | 1 (1.12x elements) | 2 (1.06x elements) | 5 (1.04x elements) | 11 (1.02x elements) |
| 0.95 | 3 (1.38x elements) | 5 (1.16x elements) | 14 (1.11x elements) | 40 (1.08x elements) |
| 0.9 | 4 (1.50x elements) | 9 (1.28x elements) | 24 (1.19x elements) | 76 (1.15x elements) |
| 0.8 | 7 (1.88x elements) | 16 (1.50x elements) | 48 (1.38x elements) | 159 (1.31x elements) |
| 0.7 | 10 (2.25x elements) | 25 (1.78x elements) | 77 (1.60x elements) | 262 (1.51x elements) |

All g are tier C. Reading the table: the spare overhead FALLS as the array gets wider (more elements average out the defects) and RISES as g drops, from about 2% to 12% extra elements at g = 0.99 to about 50% to 125% at g = 0.7. The per-element defect rate, which is not measured here, therefore decides whether sparing alone is cheap or not.
