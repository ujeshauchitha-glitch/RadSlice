## What the published wafer map allows

- Matrix scale (8 registers, matrices independent of each other): clustering stronger than alpha = 9.4 (intra-block failure correlation 0.027) is rejected at about 95%.
- Neighbouring matrices: Moran's I = -0.083 (expected -0.037), P(positive autocorrelation) = 0.67; neighbour correlation -0.08. No spatial structure between neighbours.
- **Not constrained:** a defect rate common to every matrix on the wafer (wafer-, batch- or die-level variation) shifts all matrices together and leaves no difference between them, so one wafer cannot show it.

## Simulator (real self-test and remap) against the exact model

20000 simulated chips per row; the exact column is computed independently by numerical integration.

| case | simulated | exact | z |
|---|---:|---:|---:|
| pooled, die-level, alpha=20, S=128 | 0.7774 | 0.7795 | -0.68 |
| pooled, die-level, alpha=20, S=160 | 0.9411 | 0.9436 | -1.58 |
| pooled, die-level, alpha=9.37, S=128 | 0.7167 | 0.7172 | -0.15 |
| pooled, die-level, alpha=9.37, S=160 | 0.8715 | 0.8750 | -1.49 |
| pooled, die-level, alpha=4, S=128 | 0.6667 | 0.6652 | +0.48 |
| pooled, die-level, alpha=4, S=160 | 0.7859 | 0.7896 | -1.29 |
| pooled, die-level, alpha=1, S=128 | 0.6390 | 0.6309 | +2.35 |
| pooled, die-level, alpha=1, S=160 | 0.6919 | 0.6963 | -1.35 |
| pooled, blocks of 32, alpha=9.37, S=128 | 0.9171 | 0.9156 | +0.75 |
| pooled, blocks of 32, alpha=9.37, S=144 | 0.9871 | 0.9868 | +0.46 |
| pooled, blocks of 32, alpha=4, S=128 | 0.8602 | 0.8627 | -1.00 |
| pooled, blocks of 32, alpha=4, S=144 | 0.9618 | 0.9607 | +0.85 |
| pooled, blocks of 32, alpha=1, S=128 | 0.7591 | 0.7567 | +0.79 |
| pooled, blocks of 32, alpha=1, S=144 | 0.8656 | 0.8664 | -0.36 |
| pooled, blocks of 8, alpha=4, S=128 | 0.9445 | 0.9421 | +1.46 |
| 8 regions, independent, S=128 | 0.1650 | 0.1614 | +1.39 |
| 8 regions, alpha=4, block = region, S=128 | 0.0420 | 0.0432 | -0.84 |
| 8 regions, alpha=4, die-level, S=128 | 0.4152 | 0.4160 | -0.24 |
| 32 regions, independent, S=128 | 0.0001 | 0.0001 | +0.72 |
| 32 regions, alpha=4, block = region, S=128 | 0.0000 | 0.0000 | -0.19 |
| 32 regions, alpha=4, die-level, S=128 | 0.1178 | 0.1191 | -0.55 |
| 3 register-file copies on one die, alpha=20, S=144 | 0.8221 | 0.8176 | +1.67 |
| 3 register-file copies on one die, alpha=4, S=144 | 0.6823 | 0.6839 | -0.49 |

Largest |z| over 23 cases: 2.35.

## E1. Spares needed for a yield target (256 primary slices, p = 0.71, ideal shared pool)

`alpha` is the clustering strength (smaller = stronger); `rho` the pairwise failure correlation inside a block. The data allow alpha of about 9.4 or more at matrix scale. 'n/a' means the target cannot be reached with 640 spares.

| alpha | rho | 90%, blocks of 32 | 99%, blocks of 32 | 90%, die-level | 99%, die-level | 99% area, die-level |
|---|---:|---:|---:|---:|---:|---:|
| independent | 0.000 | 120 | 134 | 120 | 134 | 1.52x |
| 50 | 0.006 | 122 | 137 | 134 | 163 | 1.64x |
| 20 | 0.014 | 124 | 140 | 148 | 195 | 1.76x |
| 9.37105 | 0.030 | 127 | 147 | 169 | 248 | 1.97x |
| 4 | 0.067 | 133 | 159 | 211 | 377 | 2.47x |
| 1 | 0.225 | 152 | 198 | 400 | n/a | n/a |

Yield at fixed spares:

| spares | die-level, independent | die-level, 50 | die-level, 20 | die-level, 9.37105 | die-level, 4 | die-level, 1 |
|---:|---:|---:|---:|---:|---:|---:|
| 96 | 0.258 | 0.361 | 0.409 | 0.445 | 0.482 | 0.544 |
| 112 | 0.748 | 0.654 | 0.617 | 0.595 | 0.581 | 0.591 |
| 128 | 0.972 | 0.859 | 0.779 | 0.717 | 0.665 | 0.631 |
| 144 | 0.999 | 0.955 | 0.884 | 0.810 | 0.734 | 0.666 |
| 160 | 1.000 | 0.988 | 0.944 | 0.875 | 0.790 | 0.696 |
| 192 | 1.000 | 0.999 | 0.989 | 0.949 | 0.869 | 0.747 |

## E2. Spares restricted to regions (128 spares in total; 256 primaries)

A region has its own primaries and its own spares, and a spare can serve only its own region. One region is the ideal pool; 32 regions is one region per register (4 private spares each), which is the M0 'per-register' case.

| regions | independent | alpha = 9.4, block = region | alpha = 4, block = region | alpha = 4, die-level |
|---:|---:|---:|---:|---:|
| 1 | 0.9718 | 0.7172 | 0.6652 | 0.6652 |
| 2 | 0.8431 | 0.5123 | 0.4444 | 0.6147 |
| 4 | 0.5281 | 0.2595 | 0.2009 | 0.5347 |
| 8 | 0.1614 | 0.0662 | 0.0432 | 0.4160 |
| 16 | 0.0126 | 0.0046 | 0.0024 | 0.2643 |
| 32 | 0.0001 | 0.0000 | 0.0000 | 0.1191 |

## E3. ALU spare area for 99% ALU yield, by slice width K (ALU treated as one clustered block)

| alpha | K = 8 | K = 16 | K = 32 | ordering holds? |
|---|---:|---:|---:|---|
| independent | 2.12x | 4.00x | 12.00x | yes |
| 20 | 2.38x | 4.75x | 15.50x | yes |
| 9.37105 | 2.50x | 5.25x | 21.00x | yes |
| 4 | 2.88x | 7.50x | 43.50x | yes |

## E4. Register-file copies (TMR) when the copies share one die-level defect rate

Boot yield of the register files only, per-copy spares as shown; alpha 'independent' means the copies fail independently.

| spares per copy | alpha | 1 copy | 3 copies | 3 copies / 1 copy |
|---:|---|---:|---:|---:|
| 144 | independent | 0.9990 | 0.9969 | 0.998 |
| 144 | 50 | 0.9546 | 0.9063 | 0.949 |
| 144 | 20 | 0.8843 | 0.8176 | 0.924 |
| 144 | 9.37105 | 0.8095 | 0.7453 | 0.921 |
| 144 | 4 | 0.7340 | 0.6839 | 0.932 |
| 144 | 1 | 0.6659 | 0.6398 | 0.961 |
| 160 | independent | 1.0000 | 1.0000 | 1.000 |
| 160 | 50 | 0.9880 | 0.9728 | 0.985 |
| 160 | 20 | 0.9436 | 0.9052 | 0.959 |
| 160 | 9.37105 | 0.8750 | 0.8270 | 0.945 |
| 160 | 4 | 0.7896 | 0.7463 | 0.945 |
| 160 | 1 | 0.6963 | 0.6718 | 0.965 |
