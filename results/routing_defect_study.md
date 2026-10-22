# G14: do the spares and routing survive their own defects?

Exact models, independent defects, p = 0.71. q is the per-transistor yield of the routing: 0.99762 is implied by the register measurements (tier B), 0.9992 is the isolated-device figure (tier A), 1 is the fault-free routing of G10. Each spare carries private routing that fails with it; a shared block of 1152 transistors is fatal unless replicated. Costs are relative to the bare 256-slice register file.

## E1. The cheapest register file against q

Rows marked * found no design above 50% yield within the search range (up to 1,200 spares, 64 shared copies): the cost shown is the best of poor options and is not a design.

| q | topology | shared block | regions | spares | shared copies | yield | silicon rel. | cost per good rel. | vs q = 1 |
|---:|:--|:--|---:|---:|---:|---:|---:|---:|---:|
| 1 (G10) | tagged | none | 8 | 192 | 1 | 0.929 | 3.91 | 4.21 | 1.00x |
| 0.99999 | tagged | none | 8 | 192 | 1 | 0.925 | 3.91 | 4.23 | 1.00x |
| 0.9999 | tagged | none | 8 | 208 | 1 | 0.948 | 4.15 | 4.37 | 1.04x |
| 0.9996 | tagged | none | 8 | 232 | 1 | 0.919 | 4.51 | 4.91 | 1.17x |
| 0.9992 | tagged | none | 8 | 280 | 1 | 0.916 | 5.24 | 5.72 | 1.36x |
| 0.9985 | tagged | none | 8 | 384 | 1 | 0.908 | 6.81 | 7.50 | 1.78x |
| 0.99762 | tagged | none | 8 | 568 | 1 | 0.907 | 9.60 | 10.58 | 2.52x |
| 1 (G10) | tagged | 1152 T | 8 | 192 | 1 | 0.929 | 3.94 | 4.24 | 1.00x |
| 0.99999 | tagged | 1152 T | 8 | 192 | 2 | 0.924 | 3.97 | 4.29 | 1.01x |
| 0.9999 | tagged | 1152 T | 8 | 208 | 3 | 0.947 | 4.24 | 4.48 | 1.06x |
| 0.9996 | tagged | 1152 T | 8 | 240 | 6 | 0.941 | 4.82 | 5.12 | 1.21x |
| 0.9992 | tagged | 1152 T | 8 | 288 | 8 | 0.920 | 5.61 | 6.10 | 1.44x |
| 0.9985 | tagged | 1152 T | 8 | 392 | 24 | 0.916 | 7.68 | 8.39 | 1.98x |
| 0.99762 | tagged | 1152 T | 8 | 576 | 48 | 0.880 | 11.22 | 12.75 | 3.01x |
| 1 (G10) | per_position | 1152 T | 32 | 320 | 1 | 0.864 | 12.28 | 14.21 | 1.00x |
| 0.99999 | per_position | 1152 T | 32 | 352 | 2 | 0.926 | 13.44 | 14.51 | 1.02x |
| 0.9999 | per_position | 1152 T | 32 | 384 | 3 | 0.880 | 14.59 | 16.59 | 1.17x |
| 0.9996 | per_position | 1152 T | 32 | 608 | 6 | 0.890 | 22.56 | 25.35 | 1.78x |
| 0.9992 | per_position | 1152 T | 32 | 1024 | 12 | 0.879 | 37.38 | 42.50 | 2.99x |
| 0.9985 | per_position | 1152 T | 32 | 1184 | 32 | 0.021 | 43.62 | 2029.86 * | 142.81x |
| 0.99762 | per_position | 1152 T | 32 | 1184 | 64 | 0.000 | 44.62 | 2463398664721.73 * | 173306064608.66x |

## E2. One spare at the G10 optimum (8 regions, 192 spares, tagged)

Each spare has 414 private routing transistors, so its yield is 0.71 x q^414.

| q | spare yield | primaries that must be covered (mean) | spares needed per region (mean) |
|---:|---:|---:|---:|
| 1 (G10) | 0.710 | 74.2 | 13.1 of 24 |
| 0.99999 | 0.707 | 74.2 | 13.1 of 24 |
| 0.9999 | 0.681 | 74.2 | 13.6 of 24 |
| 0.9996 | 0.602 | 74.2 | 15.4 of 24 |
| 0.9992 | 0.510 | 74.2 | 18.2 of 24 |
| 0.9985 | 0.381 | 74.2 | 24.3 of 24 |
| 0.99762 | 0.265 | 74.2 | 35.1 of 24 |

## E3. Chip level: ALU spares with their routing, and K again

Register file fixed to the cheapest design found in E1 for that q (tagged); ALU spares chosen to minimise silicon per good chip; silicon x cycles uses the M2 beat model, base = 2, normalised to the best K in each row group.

### q = 1 (G10)  (register file: 8 regions, 192 spares, 1 shared copies, yield 0.929)

| org | K | ALU spares | ALU yield | ALU silicon (T) | per good chip rel. | silicon x cycles |
|:--|---:|---:|---:|---:|---:|---:|
| replicated | 1 | 6 | 0.973 | 32,616 | 5.338 | 15.60 |
| replicated | 2 | 7 | 0.987 | 25,731 | 5.055 | 7.63 |
| replicated | 4 | 7 | 0.980 | 19,809 | 4.917 | 3.94 |
| replicated | 8 | 8 | 0.979 | 19,224 | 4.902 | 2.20 |
| replicated | 16 | 10 | 0.973 | 22,158 | 5.022 | 1.36 |
| replicated | 32 | 15 | 0.954 | 32,913 | 5.455 | 1.00 |
| single | 1 | 2 | 1.000 | 117 | 4.244 | 14.16 |
| single | 2 | 2 | 0.999 | 216 | 4.249 | 7.32 |
| single | 4 | 3 | 0.999 | 567 | 4.259 | 3.90 |
| single | 8 | 4 | 0.998 | 1,404 | 4.290 | 2.20 |
| single | 16 | 6 | 0.993 | 3,942 | 4.388 | 1.36 |
| single | 32 | 12 | 0.978 | 14,796 | 4.778 | 1.00 |

### q = 0.9992  (register file: 8 regions, 288 spares, 8 shared copies, yield 0.920)

| org | K | ALU spares | ALU yield | ALU silicon (T) | per good chip rel. | silicon x cycles |
|:--|---:|---:|---:|---:|---:|---:|
| replicated | 1 | 60 | 0.256 | 310,608 | 59.520 | 104.42 |
| replicated | 2 | 60 | 0.743 | 207,468 | 16.432 | 14.88 |
| replicated | 4 | 44 | 0.889 | 115,380 | 10.680 | 5.14 |
| replicated | 8 | 36 | 0.924 | 80,460 | 9.163 | 2.47 |
| replicated | 16 | 34 | 0.934 | 71,190 | 8.775 | 1.43 |
| replicated | 32 | 36 | 0.919 | 76,572 | 9.087 | 1.00 |
| single | 1 | 2 | 1.000 | 117 | 6.101 | 14.28 |
| single | 2 | 3 | 1.000 | 297 | 6.106 | 7.38 |
| single | 4 | 4 | 1.000 | 720 | 6.119 | 3.93 |
| single | 8 | 5 | 0.999 | 1,701 | 6.154 | 2.21 |
| single | 16 | 8 | 0.996 | 5,112 | 6.275 | 1.37 |
| single | 32 | 16 | 0.979 | 19,440 | 6.811 | 1.00 |

### q = 0.99762  (register file: 8 regions, 576 spares, 48 shared copies, yield 0.880)

| org | K | ALU spares | ALU yield | ALU silicon (T) | per good chip rel. | silicon x cycles |
|:--|---:|---:|---:|---:|---:|---:|
| replicated | 1 | 0 | 0.065 | 1,728 | 198.275 | 75.23 |
| replicated | 2 | 0 | 0.065 | 1,728 | 198.275 | 38.84 |
| replicated | 4 | 60 | 0.092 | 156,708 | 190.989 | 19.88 |
| replicated | 8 | 60 | 0.152 | 132,948 | 110.805 | 6.45 |
| replicated | 16 | 60 | 0.246 | 124,308 | 67.545 | 2.38 |
| replicated | 32 | 60 | 0.396 | 126,468 | 42.016 | 1.00 |
| single | 1 | 3 | 1.000 | 162 | 12.756 | 14.50 |
| single | 2 | 3 | 1.000 | 297 | 12.762 | 7.49 |
| single | 4 | 5 | 1.000 | 873 | 12.779 | 3.98 |
| single | 8 | 7 | 0.999 | 2,295 | 12.829 | 2.24 |
| single | 16 | 12 | 0.997 | 7,452 | 13.017 | 1.38 |
| single | 32 | 27 | 0.980 | 32,211 | 14.026 | 1.00 |

