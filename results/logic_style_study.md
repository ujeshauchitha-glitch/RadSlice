# G11: n-type only versus complementary logic

The paper's chip is n-type only with enhancement/depletion ratioed logic (main text p. 656-657), so mu = 1 is the style every earlier number assumed. mu scales every transistor count (a complementary n-input gate has 2n transistors, a ratioed n-only gate n + 1: 4/3 for 2 inputs, 8/5 for 4, towards 2). A block of mu times the transistors yields p^mu at an unchanged per-transistor yield; routing yields q^mu per same function. Nothing here models timing, static power, or a different yield for p-type devices.

## E1. The cheapest register file against mu

Tagged routing, spares' private routing counted, no shared block (as in G14, first row group). `silicon` is the absolute silicon relative to the n-only bare register file (mu x the relative figure).

| mu | slice yield p^mu | routing q | regions | spares | yield | cost per good (own bare file = 1) | absolute silicon per good, n-only bare file = 1 |
|---:|---:|:--|---:|---:|---:|---:|---:|
| 1.00 | 0.710 | none (fault-free) | 8 | 192 | 0.929 | 4.21 | 4.21 |
| 1.00 | 0.710 | 0.9992 (isolated device) | 8 | 280 | 0.916 | 5.72 | 5.72 |
| 1.00 | 0.710 | 0.99762 (register-implied) | 8 | 568 | 0.907 | 10.58 | 10.58 |
| 1.33 | 0.633 | none (fault-free) | 8 | 264 | 0.944 | 5.29 | 7.06 |
| 1.33 | 0.633 | 0.9992 (isolated device) | 8 | 424 | 0.920 | 8.06 | 10.75 |
| 1.33 | 0.633 | 0.99762 (register-implied) | 8 | 1064 | 0.914 | 18.72 | 24.96 |
| 1.60 | 0.578 | none (fault-free) | 8 | 320 | 0.936 | 6.24 | 9.99 |
| 1.60 | 0.578 | 0.9992 (isolated device) | 8 | 568 | 0.922 | 10.41 | 16.66 |
| 1.60 | 0.578 | 0.99762 (register-implied) | 8 | 1200 | 0.256 | 74.77 | 119.63 |
| 2.00 | 0.504 | none (fault-free) | 8 | 424 | 0.948 | 7.83 | 15.66 |
| 2.00 | 0.504 | 0.9992 (isolated device) | 8 | 848 | 0.926 | 14.95 | 29.90 |
| 2.00 | 0.504 | 0.99762 (register-implied) | none | none | below 1e-6 | not reachable | not reachable |

Search limits: 1,200 register-file spares and 60 ALU spares. A row at a limit (and the mu = 1.6 row with yield 0.256) shows the limit, not an optimum.

## E2. Chip level: ALU width and organisation against mu

Register file fixed to the E1 optimum at routing q = 0.9992^mu; ALU spares chosen to minimise silicon per good chip; silicon x cycles with the M2 beat model, base = 2; normalised to the best K in each row group.

| mu | organisation | K* (silicon x cycles) | per good chip, own bare file = 1 at K* | ALU spares at K* |
|---:|:--|---:|---:|---:|
| 1.00 | replicated | 32 | 8.69 | 35 |
| 1.00 | single | 32 | 6.43 | 15 |
| 1.33 | replicated | 32 | 14.66 | 60 |
| 1.33 | single | 32 | 9.28 | 27 |
| 1.60 | replicated | 32 | 28.79 | 60 |
| 1.60 | single | 32 | 12.25 | 41 |
| 2.00 | replicated | 32 | 125.35 | 60 |
| 2.00 | single | 32 | 18.50 | 60 |

