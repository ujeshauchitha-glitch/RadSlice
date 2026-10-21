# G10: the cost of the spare-routing crossbar

Exact models; p = 0.71 (tier B); every cost knob is tier C. Transistor count stands in for area, wiring and layout are ignored, so routing costs are more likely too low than too high. Default knobs: tagged spare array, 2 read ports, 1 write port, 9 T per tag bit per port for compare, ALU unit = 27 T per bit, nothing outside register file and ALU.

## E1. Routing silicon against the slices it protects

Ideal pool (1 region). Silicon in units of the 256-slice bare register file (36,864 T).

| spares | spare storage | tagged routing | per-position routing |
|---:|---:|---:|---:|
| 32 | 0.12 | 1.35 | 32.00 |
| 64 | 0.25 | 2.70 | 64.00 |
| 96 | 0.38 | 4.05 | 96.00 |
| 128 | 0.50 | 5.41 | 128.00 |
| 192 | 0.75 | 8.11 | 192.00 |
| 256 | 1.00 | 10.81 | 256.00 |

## E2. Cheapest register file per region count

Silicon per working register file, relative to the bare 256-slice file. No sparing at all yields 8.4e-39, so it is not a candidate.

Reference designs (independent defects, tagged routing, default costs):

| design | silicon rel. | yield | cost per good rel. | same, crossbar free |
|:--|---:|---:|---:|---:|
| C2 design: 128 spares, ideal pool | 6.91 | 0.972 | 7.11 | 1.54 |
| 160 spares, ideal pool | 8.38 | 1.000 | 8.38 | 1.63 |

The search stops at 420 spares (360 under clustering), so the 64-region rows are limited by that cap and are not minima.

### independent defects

| regions | topology | best spares | yield | silicon rel. | cost per good rel. | same, crossbar free |
|---:|:--|---:|---:|---:|---:|---:|
| 1 | tagged | 126 | 0.961 | 6.81 | 7.09 | 1.54 (S=132) |
| 1 | per_position | 125 | 0.954 | 126.49 | 132.64 | 1.54 (S=132) |
| 2 | tagged | 140 | 0.957 | 5.10 | 5.33 | 1.60 (S=146) |
| 2 | per_position | 138 | 0.945 | 70.54 | 74.62 | 1.60 (S=146) |
| 4 | tagged | 160 | 0.943 | 4.24 | 4.50 | 1.70 (S=168) |
| 4 | per_position | 156 | 0.921 | 40.61 | 44.07 | 1.70 (S=168) |
| 8 | tagged | 192 | 0.929 | 3.91 | 4.21 | 1.86 (S=208) |
| 8 | per_position | 192 | 0.929 | 25.75 | 27.73 | 1.86 (S=208) |
| 16 | tagged | 256 | 0.944 | 4.56 | 4.83 | 2.12 (S=256) |
| 16 | per_position | 240 | 0.898 | 16.94 | 18.86 | 2.12 (S=256) |
| 32 | tagged | 352 | 0.936 | 5.47 | 5.84 | 2.54 (S=352) |
| 32 | per_position | 320 | 0.864 | 12.25 | 14.18 | 2.54 (S=352) |
| 64 | tagged | 384 | 0.573 | 5.41 | 9.43 | 4.36 (S=384) |
| 64 | per_position | 384 | 0.573 | 8.50 | 14.82 | 4.36 (S=384) |

Cheapest design that reaches a yield target (tagged routing):

| target | regions | spares | yield | silicon rel. | cost per good rel. |
|---:|---:|---:|---:|---:|---:|
| 90% | 8 | 192 | 0.929 | 3.91 | 4.21 |
| 99% | 8 | 224 | 0.990 | 4.39 | 4.43 |

### die-level clustering, alpha = 9.4

| regions | topology | best spares | yield | silicon rel. | cost per good rel. | same, crossbar free |
|---:|:--|---:|---:|---:|---:|---:|
| 1 | tagged | 144 | 0.810 | 7.64 | 9.44 | 1.83 (S=178) |
| 1 | per_position | 138 | 0.779 | 139.54 | 179.21 | 1.83 (S=178) |
| 2 | tagged | 160 | 0.828 | 5.69 | 6.87 | 1.90 (S=192) |
| 2 | per_position | 148 | 0.771 | 75.58 | 98.06 | 1.90 (S=192) |
| 4 | tagged | 184 | 0.843 | 4.73 | 5.61 | 1.99 (S=208) |
| 4 | per_position | 168 | 0.778 | 43.66 | 56.14 | 1.99 (S=208) |
| 8 | tagged | 224 | 0.862 | 4.39 | 5.10 | 2.16 (S=240) |
| 8 | per_position | 208 | 0.813 | 27.81 | 34.22 | 2.16 (S=240) |
| 16 | tagged | 288 | 0.876 | 5.01 | 5.72 | 2.43 (S=288) |
| 16 | per_position | 256 | 0.798 | 18.00 | 22.56 | 2.43 (S=288) |
| 32 | tagged | 320 | 0.736 | 5.06 | 6.88 | 3.06 (S=320) |
| 32 | per_position | 320 | 0.736 | 12.25 | 16.64 | 3.06 (S=320) |
| 64 | tagged | 256 | 0.171 | 3.94 | 23.01 | 11.69 (S=256) |
| 64 | per_position | 256 | 0.171 | 6.00 | 35.07 | 11.69 (S=256) |

Cheapest design that reaches a yield target (tagged routing):

| target | regions | spares | yield | silicon rel. | cost per good rel. |
|---:|---:|---:|---:|---:|---:|
| 90% | 8 | 256 | 0.927 | 4.88 | 5.26 |
| 99% | 8 | 352 | 0.990 | 6.33 | 6.39 |

## E3. Chip cost against slice width K

Register file and ALU optimised jointly (independent defects, default costs). Silicon per good chip in units of the bare register file; cycles per instruction from the M2 beat model with the outside-the-ALU cost `base` = 2 beats; silicon x cycles normalised to the best row in the table.

### ALU organisation: replicated

| K | RF regions | RF spares | ALU spares | ALU yield | ALU silicon (T) | per good chip rel. | x cycles: alu_mix (compute l | x cycles: rv64ui+rv64mi (tes |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 8 | 200 | 6 | 0.973 | 32,616 | 5.286 | 15.60 | 14.48 |
| 2 | 8 | 200 | 7 | 0.987 | 25,731 | 5.009 | 7.63 | 7.12 |
| 4 | 8 | 200 | 7 | 0.980 | 19,809 | 4.876 | 3.95 | 3.71 |
| 8 | 8 | 200 | 8 | 0.979 | 19,224 | 4.861 | 2.20 | 2.10 |
| 16 | 8 | 200 | 10 | 0.973 | 22,158 | 4.978 | 1.37 | 1.33 |
| 32 | 8 | 200 | 15 | 0.954 | 32,913 | 5.401 | 1.00 | 1.00 |
| 64 | 8 | 200 | 29 | 0.865 | 68,805 | 7.132 | 1.03 | 1.05 |

### ALU organisation: single

| K | RF regions | RF spares | ALU spares | ALU yield | ALU silicon (T) | per good chip rel. | x cycles: alu_mix (compute l | x cycles: rv64ui+rv64mi (tes |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 8 | 192 | 2 | 1.000 | 117 | 4.210 | 14.16 | 13.14 |
| 2 | 8 | 192 | 2 | 0.999 | 216 | 4.215 | 7.32 | 6.82 |
| 4 | 8 | 192 | 3 | 0.999 | 567 | 4.226 | 3.90 | 3.67 |
| 8 | 8 | 192 | 4 | 0.998 | 1,404 | 4.256 | 2.20 | 2.10 |
| 16 | 8 | 192 | 6 | 0.993 | 3,942 | 4.354 | 1.36 | 1.33 |
| 32 | 8 | 200 | 12 | 0.978 | 14,796 | 4.741 | 1.00 | 1.00 |
| 64 | 8 | 200 | 29 | 0.865 | 68,805 | 7.132 | 1.17 | 1.19 |

## E4. Which cost assumption changes which answer

One knob moved at a time from the default (independent defects). `K*` is the K with the lowest silicon x cycles for the first workload at base = 2; `ALU/chip` is the ALU's share of chip silicon at the K* design.

| knob | value | RF regions | RF spares | RF cost per good rel. | K* replicated | K* single | ALU/chip (single K*) |
|:--|:--|---:|---:|---:|---:|---:|---:|
| default |  | 8 | 192 | 4.21 | 32 | 32 | 0.091 |
| topology | per_position | 32 | 320 | 14.18 | 64 | 64 | 0.167 |
| read_ports | 1 | 8 | 192 | 3.50 | 32 | 32 | 0.100 |
| read_ports | 3 | 8 | 192 | 4.91 | 32 | 32 | 0.081 |
| tag_compare_T | 0.0 | 8 | 200 | 3.30 | 32 | 32 | 0.105 |
| tag_compare_T | 27.0 | 8 | 192 | 6.02 | 64 | 32 | 0.072 |
| t_mux2 x | 0.5 | 8 | 192 | 3.80 | 32 | 32 | 0.088 |
| t_mux2 x | 2.0 | 8 | 192 | 5.01 | 64 | 32 | 0.097 |
| t_mux2 x | 4.0 | 8 | 192 | 6.63 | 64 | 32 | 0.094 |
| alu_factor | 4.0 | 8 | 192 | 4.21 | 32 | 32 | 0.184 |
| alu_factor | 16.0 | 8 | 192 | 4.21 | 32 | 16 | 0.159 |
| other_T | 6000.0 | 8 | 192 | 4.21 | 32 | 32 | 0.087 |
| other_T | 60000.0 | 8 | 192 | 4.21 | 64 | 32 | 0.071 |
| base_cycles | 1 | 8 | 192 | 4.21 | 64 | 32 | 0.091 |
| base_cycles | 4 | 8 | 192 | 4.21 | 32 | 32 | 0.091 |

