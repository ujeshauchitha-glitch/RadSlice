# G19 step 4: accuracy of the protected MAC array under upsets

Network: 64-32-10 int8 digit classifier, fault-free accuracy 0.9685. Each cell is 2700 images (the held-out set, repeated with fresh faults). Accuracy counts a detected failure as no answer. `silent` is the fraction answered differently from the fault-free network with no warning. Accuracy floor used here: 0.9585 (one point below fault-free). All rates are tier C and swept.

## accumulator upsets (accumulator upset per element per cycle)

| scheme | elements | 0 | 1e-05 | 0.0001 | 0.0003 | 0.001 | 0.003 | 0.01 |
|---|---|---|---|---|---|---|---|---|
| no protection | 1x | 0.969 / 0.000 / x1.00 | 0.968 / 0.001 / x1.00 | 0.957 / 0.011 / x1.00 | 0.936 / 0.038 / x1.00 | 0.860 / 0.127 / x1.00 | 0.697 / 0.295 / x1.00 | 0.372 / 0.628 / x1.00 |
| duplicate + compare + retry | 2x | 0.969 / 0.000 / x1.00 | 0.969 / 0.000 / x1.01 | 0.964 / 0.000 / x1.09 | 0.897 / 0.000 / x1.29 | 0.252 / 0.000 / x1.96 | 0.000 / 0.000 / x2.77 | 0.000 / 0.000 / x3.00 |
| triple vote | 3x | 0.969 / 0.000 / x1.00 | 0.969 / 0.000 / x1.00 | 0.969 / 0.000 / x1.00 | 0.969 / 0.000 / x1.00 | 0.966 / 0.003 / x1.00 | 0.941 / 0.033 / x1.00 | 0.727 / 0.263 / x1.00 |
| checksum lane + retry (clean reload) | 1.125x lanes + one 16-bit-weight lane | 0.969 / 0.000 / x1.00 | 0.969 / 0.000 / x1.01 | 0.968 / 0.000 / x1.05 | 0.953 / 0.000 / x1.16 | 0.661 / 0.001 / x1.57 | 0.023 / 0.000 / x2.38 | 0.000 / 0.000 / x2.96 |
| checksum lane + retry (no clean copy) | 1.125x lanes + one 16-bit-weight lane | 0.969 / 0.000 / x1.00 | 0.969 / 0.000 / x1.01 | 0.968 / 0.000 / x1.05 | 0.952 / 0.000 / x1.16 | 0.650 / 0.000 / x1.57 | 0.031 / 0.000 / x2.36 | 0.000 / 0.000 / x2.96 |

Each cell: accuracy / silent wrong / cycles relative to the unprotected array.

Highest rate at which accuracy stays at or above the floor:

- no protection: 1e-05
- duplicate + compare + retry: 0.0001
- triple vote: 0.001
- checksum lane + retry (clean reload): 0.0001
- checksum lane + retry (no clean copy): 0.0001

## weight upsets (weight-bit upset per bit per image)

| scheme | elements | 0 | 1e-05 | 0.0001 | 0.0003 | 0.001 | 0.003 | 0.01 | 0.03 |
|---|---|---|---|---|---|---|---|---|---|
| no protection | 1x | 0.969 / 0.000 / x1.00 | 0.969 / 0.000 / x1.00 | 0.967 / 0.001 / x1.00 | 0.969 / 0.005 / x1.00 | 0.961 / 0.018 / x1.00 | 0.952 / 0.035 / x1.00 | 0.856 / 0.132 / x1.00 | 0.656 / 0.341 / x1.00 |
| duplicate + compare + retry | 2x | 0.969 / 0.000 / x1.00 | 0.969 / 0.001 / x1.00 | 0.969 / 0.002 / x1.00 | 0.967 / 0.005 / x1.00 | 0.966 / 0.012 / x1.00 | 0.950 / 0.037 / x1.00 | 0.880 / 0.113 / x1.00 | 0.657 / 0.338 / x1.00 |
| triple vote | 3x | 0.969 / 0.000 / x1.00 | 0.968 / 0.000 / x1.00 | 0.968 / 0.001 / x1.00 | 0.967 / 0.006 / x1.00 | 0.963 / 0.013 / x1.00 | 0.943 / 0.041 / x1.00 | 0.858 / 0.133 / x1.00 | 0.644 / 0.347 / x1.00 |
| checksum lane + retry (clean reload) | 1.125x lanes + one 16-bit-weight lane | 0.969 / 0.000 / x1.00 | 0.969 / 0.000 / x1.03 | 0.969 / 0.000 / x1.21 | 0.969 / 0.000 / x1.52 | 0.969 / 0.000 / x1.90 | 0.969 / 0.000 / x2.00 | 0.969 / 0.000 / x2.00 | 0.969 / 0.000 / x2.00 |
| checksum lane + retry (no clean copy) | 1.125x lanes + one 16-bit-weight lane | 0.969 / 0.000 / x1.00 | 0.844 / 0.000 / x1.05 | 0.252 / 0.000 / x1.43 | 0.015 / 0.000 / x2.01 | 0.000 / 0.000 / x2.79 | 0.000 / 0.000 / x3.00 | 0.000 / 0.000 / x3.00 | 0.000 / 0.000 / x3.00 |

Each cell: accuracy / silent wrong / cycles relative to the unprotected array.

Highest rate at which accuracy stays at or above the floor:

- no protection: 0.001
- duplicate + compare + retry: 0.001
- triple vote: 0.001
- checksum lane + retry (clean reload): 0.03
- checksum lane + retry (no clean copy): 0
