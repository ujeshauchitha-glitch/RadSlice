import unittest

from radslice import wafer_map as wm


class PublishedWaferMap(unittest.TestCase):
    """The transcribed Supplementary Fig. 7 map reproduces the numbers quoted in docs/results.md."""

    def setUp(self):
        self.grid = wm.load_grid()
        self.s = wm.summarize(self.grid)

    def test_shape_and_counts(self):
        self.assertEqual((self.s["matrices"], self.s["registers"]), (28, 224))
        self.assertEqual((self.s["failed"], self.s["works"]), (59, 165))
        self.assertAlmostEqual(self.s["pass_rate"], 165 / 224)

    def test_pass_rate_is_near_the_captions_71_percent(self):
        # The caption says "can reach 71%"; counting the map gives 73.7%. Both are recorded.
        self.assertAlmostEqual(self.s["pass_rate"], 0.7366, places=3)
        self.assertLess(abs(self.s["pass_rate"] - wm.CAPTION_YIELD), 0.03)

    def test_two_of_28_matrices_are_perfect_as_independence_predicts(self):
        self.assertEqual(self.s["perfect_matrices"], 2)                 # 2/28 = 7.1%
        self.assertAlmostEqual(self.s["expected_perfect_if_independent"], 2.4, delta=0.2)

    def test_no_clustering_within_or_between_matrices(self):
        t = wm.independence_tests(self.grid, reps=4000, seed=1)
        self.assertGreater(t["p_variance_at_least"], 0.5)               # not over-dispersed: no bad matrices
        self.assertLess(t["observed_variance"], t["binomial_variance"])  # slightly under-dispersed
        self.assertGreater(t["p_adjacent_at_least"], 0.05)              # no excess adjacent failures

    def test_first_register_of_a_matrix_rarely_fails(self):
        t = wm.independence_tests(self.grid, reps=100, seed=1)
        self.assertEqual(t["row0_failed"], 1)
        self.assertLess(t["p_row0_at_most"], 0.01)                      # an unexplained position effect

    def test_sourced_parameters_agree_with_the_map(self):
        from radslice.params import load_params
        p = load_params()
        self.assertEqual(p["yield.measured_8bit_shift_register"].value, wm.CAPTION_YIELD)
        self.assertAlmostEqual(p["yield.measured_8bit_shift_register_from_map"].value, self.s["pass_rate"], places=4)
        self.assertEqual(p["yield.p_slice_good"].value, wm.CAPTION_YIELD)        # the lower reading is used
        self.assertAlmostEqual(p["yield.implied_per_transistor"].value,
                               self.s["caption_per_transistor_yield"], places=5)

    def test_binom_cdf(self):
        self.assertAlmostEqual(wm.binom_cdf(8, 8, 0.3), 1.0)
        self.assertAlmostEqual(wm.binom_cdf(0, 8, 0.5), 0.5**8)

    def test_bad_map_is_rejected(self):
        import tempfile, pathlib
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / "m.txt"
            f.write_text("GGGG\n" * 28)
            with self.assertRaises(ValueError):
                wm.load_grid(f)


if __name__ == "__main__":
    unittest.main()
