"""Published yield numbers from Ao, Zhou et al., "A RISC-V 32-bit microprocessor based on
two-dimensional semiconductors", Nature 640, 654-661 (2025), doi:10.1038/s41586-025-08759-9,
and what they imply for each other.

Every constant names where in the paper it comes from. They are the paper's numbers as
printed (several are rounded by the authors), not results of this project. The paper is not
redistributed here.

    python -m radslice.paper_data
"""
from __future__ import annotations

import math

# Main text, page 659: "The yield of 8-bit registers, each with 144 transistors, reached 71% at
# the wafer scale ... reaching approximately 7% for a 64-bit register with 1,152 transistors."
TRANSISTORS_8BIT = 144
TRANSISTORS_64BIT = 1152
YIELD_8BIT = 0.71
YIELD_64BIT_APPROX = 0.07

# Main text, page 657: three batches of wafers, each with 7,000 transistors, 800 randomly
# selected for testing; "The overall transistor yield reached 99.92%".
TRANSISTOR_YIELD = 0.9992

# Main text, page 657: a 30 x 30 array of 900 inverters in the 6 mm x 6 mm region of the chip;
# "898 out of 900 inverters worked correctly with a yield of 99.77%".
INVERTERS_TESTED = 900
INVERTERS_WORKING = 898
INVERTER_YIELD_PRINTED = 0.9977         # as printed; 898/900 is 99.778%, so the paper truncates
TRANSISTORS_PER_INVERTER = 2

# Extended Data Table 2 ("Datasheet"): integrated transistor count of the processor (the text
# says "over 5,900"). Extended Data Fig. 5d: the 32-bit data register is four 8-bit shift-register
# blocks, 576 transistors in all, which is 144 per block: the same block whose yield is 71%.
CHIP_TRANSISTORS = 5931


def wilson(k: int, n: int, z: float = 1.96) -> tuple[float, float]:
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return c - h, c + h


def per_transistor_yield(circuit_yield: float, transistors: int) -> float:
    """The per-transistor yield that independent failures would need to give this circuit yield."""
    return circuit_yield ** (1.0 / transistors)


def independent_yield(per_transistor: float, transistors: int) -> float:
    return per_transistor ** transistors


def inverter_per_transistor_yield() -> float:
    return per_transistor_yield(INVERTERS_WORKING / INVERTERS_TESTED, TRANSISTORS_PER_INVERTER)


def summary() -> dict:
    inv = inverter_per_transistor_yield()
    return dict(
        implied_8bit=per_transistor_yield(YIELD_8BIT, TRANSISTORS_8BIT),
        implied_64bit=per_transistor_yield(YIELD_64BIT_APPROX, TRANSISTORS_64BIT),
        scaled_64bit_from_8bit=YIELD_8BIT ** (TRANSISTORS_64BIT // TRANSISTORS_8BIT),
        transistor_level_8bit=independent_yield(TRANSISTOR_YIELD, TRANSISTORS_8BIT),
        transistor_level_64bit=independent_yield(TRANSISTOR_YIELD, TRANSISTORS_64BIT),
        inverter_level_8bit=independent_yield(inv, TRANSISTORS_8BIT),
        inverter_level_64bit=independent_yield(inv, TRANSISTORS_64BIT),
        chip_if_register_density=YIELD_8BIT ** (CHIP_TRANSISTORS / TRANSISTORS_8BIT),
        inverter_failure_interval=wilson(INVERTERS_TESTED - INVERTERS_WORKING, INVERTERS_TESTED),
    )


def main() -> None:
    s = summary()
    print(f"per-transistor yield implied by the 8-bit register (144 transistors, 71%):  {s['implied_8bit']:.5f}")
    print(f"per-transistor yield implied by the 64-bit register (1,152, ~7%):            {s['implied_64bit']:.5f}")
    print(f"64-bit register scaled from the 8-bit figure, 0.71^8:                          {s['scaled_64bit_from_8bit']:.4f}  (paper: ~0.07)")
    print(f"independent prediction from the transistor test (99.92%): 8-bit {s['transistor_level_8bit']:.3f}, 64-bit {s['transistor_level_64bit']:.3f}")
    print(f"independent prediction from the inverter test (898/900):  8-bit {s['inverter_level_8bit']:.3f}, 64-bit {s['inverter_level_64bit']:.3f}")
    print(f"failing transistors implied: transistor test {100 * (1 - TRANSISTOR_YIELD):.3f}%, "
          f"inverter test {100 * (1 - inverter_per_transistor_yield()):.3f}%, "
          f"registers {100 * (1 - s['implied_8bit']):.3f}% (8-bit) / {100 * (1 - s['implied_64bit']):.3f}% (64-bit)")
    lo, hi = s["inverter_failure_interval"]
    print(f"inverter failures 2 of 900: {100 * 2 / 900:.2f}% (95% interval {100 * lo:.2f}-{100 * hi:.2f}%)")
    print(f"extrapolation, NOT a paper claim: whole {CHIP_TRANSISTORS}-transistor chip at the register defect density: {s['chip_if_register_density']:.1e}")


if __name__ == "__main__":
    main()
