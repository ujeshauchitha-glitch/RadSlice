import unittest

from radslice import paper_data as pd
from radslice.params import load_params


class PublishedYieldNumbers(unittest.TestCase):
    def setUp(self):
        self.s = pd.summary()

    def test_the_two_register_yields_imply_one_per_transistor_yield(self):
        # 71% at 144 transistors and ~7% at 1,152 transistors give the same per-transistor yield:
        # the yield falls with transistor count the way independent failures predict.
        self.assertAlmostEqual(self.s["implied_8bit"], 0.99762, places=5)
        self.assertAlmostEqual(self.s["implied_64bit"], 0.99769, places=4)
        self.assertLess(abs(self.s["implied_8bit"] - self.s["implied_64bit"]), 0.0002)

    def test_the_papers_64bit_figure_is_the_8bit_figure_to_the_eighth(self):
        self.assertEqual(pd.TRANSISTORS_64BIT, 8 * pd.TRANSISTORS_8BIT)
        self.assertAlmostEqual(self.s["scaled_64bit_from_8bit"], 0.0646, places=4)
        self.assertLess(abs(self.s["scaled_64bit_from_8bit"] - pd.YIELD_64BIT_APPROX), 0.01)

    def test_device_level_yields_overpredict_the_register_yield(self):
        # Isolated transistors and inverters are better than registers built from them.
        self.assertGreater(self.s["transistor_level_8bit"], 0.85)        # vs 0.71 observed
        self.assertGreater(self.s["inverter_level_8bit"], 0.80)
        self.assertGreater(self.s["transistor_level_64bit"], 0.25)       # vs ~0.07 observed
        self.assertGreater(self.s["inverter_level_64bit"], 0.20)

    def test_inverter_failure_interval_contains_the_transistor_test_implied_rate(self):
        lo, hi = self.s["inverter_failure_interval"]
        self.assertAlmostEqual(2 / 900, 0.0022, places=4)
        self.assertLess(lo, 0.0008 * 2)       # an inverter has two transistors at 0.08% each
        self.assertGreater(hi, 0.0022)

    def test_sourced_parameters_agree_with_the_paper_constants(self):
        p = load_params()
        self.assertEqual(p["yield.measured_8bit_shift_register"].value, pd.YIELD_8BIT)
        self.assertEqual(p["yield.measured_64bit_register"].value, pd.YIELD_64BIT_APPROX)
        self.assertEqual(p["yield.measured_transistor"].value, pd.TRANSISTOR_YIELD)
        # The paper prints 99.77% for 898/900 = 99.778%, i.e. truncated; the parameter holds the printed value.
        self.assertEqual(p["yield.measured_inverter"].value, pd.INVERTER_YIELD_PRINTED)
        self.assertAlmostEqual(pd.INVERTER_YIELD_PRINTED, pd.INVERTERS_WORKING / pd.INVERTERS_TESTED, delta=2e-4)
        self.assertAlmostEqual(p["yield.implied_per_transistor"].value, self.s["implied_8bit"], places=5)


if __name__ == "__main__":
    unittest.main()
