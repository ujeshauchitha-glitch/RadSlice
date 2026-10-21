import unittest

from radslice import yield_model as ym
from radslice.params import load_params

P = 0.71


class ClosedForm(unittest.TestCase):
    def test_64bit_register_no_spares(self):
        # 8 byte slices at 0.71 each: 0.71**8 = 0.0646 (project brief).
        self.assertAlmostEqual(ym.register_yield(8, P), 0.0646, places=4)

    def test_binom_tail_edges(self):
        self.assertEqual(ym.binom_tail(10, 0, P), 1.0)
        self.assertEqual(ym.binom_tail(10, 11, P), 0.0)
        self.assertAlmostEqual(ym.binom_tail(10, 10, P), P**10, places=12)

    def test_pool_equals_p_pow_n_without_spares(self):
        self.assertAlmostEqual(ym.file_yield_shared_pool(256, 0, P), P**256, places=300)

    def test_spare_table(self):
        rows = {s: (a, pr, sp) for s, a, pr, sp in ym.spare_table(P)}
        a, pr, sp = rows[128]
        self.assertAlmostEqual(a, 1.5)
        self.assertAlmostEqual(sp, 0.9718, places=3)   # "about 97%" in the brief
        self.assertLess(pr, 1e-3)                       # per-register far worse
        self.assertLess(rows[96][2], 0.5)               # sharp cliff below 128
        self.assertIsNone(rows[16][1])                  # 16 does not split over 32 regs

    def test_pool_yield_monotone_in_spares(self):
        ys = [ym.file_yield_shared_pool(256, s, P) for s in range(0, 160, 8)]
        self.assertEqual(ys, sorted(ys))


class MonteCarlo(unittest.TestCase):
    def test_pool_matches_closed_form_within_4_sigma(self):
        exact = ym.file_yield_shared_pool(256, 128, P)
        est, se = ym.mc_shared_pool(256, 128, P, trials=20000, seed=12345)
        self.assertLess(abs(est - exact), 4 * se)


class Params(unittest.TestCase):
    def test_tiers_are_valid_and_sourced_values_name_a_source(self):
        params = load_params()
        self.assertTrue(params)
        for p in params.values():
            self.assertIn(p.tier, ("A", "B", "C"), p.name)
            if p.tier in ("A", "B"):
                self.assertTrue(p.source, f"{p.name} is tier {p.tier} but names no source")

    def test_only_the_paper_derived_yield_values_are_sourced(self):
        # Everything except the values read or derived from the Nature paper stays tier C.
        sourced = {n for n, p in load_params().items() if p.tier != "C"}
        self.assertEqual(sourced, {"yield.measured_8bit_shift_register",
                                   "yield.measured_8bit_shift_register_from_map",
                                   "yield.measured_64bit_register", "yield.measured_transistor",
                                   "yield.measured_inverter", "yield.cluster_alpha_min_matrix_scale",
                                   "yield.p_slice_good", "yield.implied_per_transistor",
                                   "cost.rf_block_transistors", "cost.adder_bit_transistors",
                                   "cost.selection_module_transistors", "cost.mux2_transistors",
                                   "cost.store_bit_transistors"})

    def test_yield_param_matches_test_constant(self):
        self.assertEqual(load_params()["yield.p_slice_good"].value, P)

    def test_tier_a_requires_source(self):
        import tempfile, pathlib
        from radslice.params import load_params as lp
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / "p.toml"
            f.write_text('[x]\nvalue = 1\ntier = "A"\n')
            with self.assertRaises(ValueError):
                lp(f)
            f.write_text('[x]\nvalue = 1\ntier = "Z"\n')
            with self.assertRaises(ValueError):
                lp(f)


class AluClosedForm(unittest.TestCase):
    def test_no_spares_is_p_to_the_eighth_for_every_k(self):
        # Same defect density per bit: total ALU area is the same for any K.
        for k in (8, 16, 32):
            self.assertAlmostEqual(ym.alu_yield(k, 0, P), P**8, places=12)

    def test_unit_counts_and_area(self):
        self.assertEqual(ym.alu_units(8), 8)
        self.assertEqual(ym.alu_units(32), 2)
        self.assertAlmostEqual(ym.alu_area_ratio(8, 4), 1.5)
        self.assertAlmostEqual(ym.alu_area_ratio(32, 1), 1.5)

    def test_spares_help_and_are_monotone(self):
        for k in (8, 16, 32):
            ys = [ym.alu_yield(k, s, P) for s in range(0, 12)]
            self.assertEqual(ys, sorted(ys))
            self.assertGreater(ys[-1], ys[0])

    def test_min_spares_reaches_target(self):
        s = ym.min_spares_for(8, P, 0.99)
        self.assertGreaterEqual(ym.alu_yield(8, s, P), 0.99)
        self.assertLess(ym.alu_yield(8, s - 1, P), 0.99)


if __name__ == "__main__":
    unittest.main()
