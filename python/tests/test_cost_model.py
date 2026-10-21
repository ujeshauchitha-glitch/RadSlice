import unittest

from radslice import cluster_model as cm
from radslice import cost_model as cost
from radslice.cost_model import Costs

P = 0.71
BASE = cost.RF_PRIMARIES * cost.T_RF_BLOCK


class TestAnchors(unittest.TestCase):
    def test_published_anchors(self):
        self.assertEqual((cost.T_RF_BLOCK, cost.T_ADD_BIT, cost.T_SEL4), (144, 27, 27))
        self.assertEqual(cost.T_STORE_BIT, 18)
        self.assertEqual(BASE, 36864)

    def test_a_four_input_mux_costs_the_published_selection_module(self):
        c = Costs()
        self.assertEqual(cost.T_MUX2, 9)
        self.assertEqual(cost.mux_T(4, c), cost.T_SEL4)
        self.assertEqual(cost.mux_T(1, c), 0)
        self.assertEqual(cost.mux_T(0, c), 0)


class TestParamsAgree(unittest.TestCase):
    def test_constants_match_params_toml(self):
        from radslice.params import load_params
        ps = load_params()
        self.assertEqual(ps["cost.rf_block_transistors"].value, cost.T_RF_BLOCK)
        self.assertEqual(ps["cost.adder_bit_transistors"].value, cost.T_ADD_BIT)
        self.assertEqual(ps["cost.selection_module_transistors"].value, cost.T_SEL4)
        self.assertEqual(ps["cost.mux2_transistors"].value, cost.T_MUX2)
        self.assertEqual(ps["cost.store_bit_transistors"].value, cost.T_STORE_BIT)
        d = Costs()
        self.assertEqual(ps["cost.tag_compare_transistors"].value, d.tag_compare_T)
        self.assertEqual(ps["cost.alu_factor"].value, d.alu_factor)
        self.assertEqual(ps["cost.other_silicon"].value, d.other_T)


class TestRegisterFileCost(unittest.TestCase):
    def test_no_spares_no_routing(self):
        for topo in ("tagged", "per_position"):
            self.assertEqual(cost.rf_routing_T(0, 1, Costs(topology=topo)), 0)
        self.assertEqual(cost.rf_storage_T(0), BASE)

    def test_tagged_hand_value(self):
        # 128 spares, one region: steering 2 ports * 128 spares * 8 lanes * 8 bits * 9 T = 147,456;
        # table 128 spares * (8 tag bits + valid) * (18 + 9 * 3 ports) T = 51,840.
        self.assertEqual(cost.rf_routing_T(128, 1), 147456 + 51840)

    def test_per_position_hand_value(self):
        # 256 positions * 8 bits * 2 ports * a 129-input mux (128 * 9 T)
        self.assertEqual(cost.rf_routing_T(128, 1, Costs(topology="per_position")), 256 * 8 * 2 * 9 * 128)

    def test_routing_exceeds_the_storage_it_protects(self):
        # G10 claim 1, as pinned numbers: routing per spare is about 10x the spare's own storage (tagged)
        for s in (32, 128, 256):
            ratio = cost.rf_routing_T(s, 1) / (s * cost.T_RF_BLOCK)
            self.assertGreater(ratio, 10.0)
            self.assertLess(ratio, 11.0)

    def test_limited_reach_is_cheaper_per_spare(self):
        r = [cost.rf_routing_T(128, n) / 128 for n in (1, 2, 4, 8, 16)]
        self.assertEqual(r, sorted(r, reverse=True))

    def test_free_crossbar_scales_everything_away(self):
        free = Costs(routing_scale=0.0)
        self.assertEqual(cost.rf_routing_T(128, 1, free), 0)
        self.assertEqual(cost.alu_routing_T(8, 4, "replicated", free), 0)

    def test_costs_scale_with_knobs(self):
        base = cost.rf_routing_T(64, 8)
        self.assertGreater(cost.rf_routing_T(64, 8, Costs(read_ports=3)), base)
        self.assertGreater(cost.rf_routing_T(64, 8, Costs(tag_compare_T=27.0)), base)
        self.assertAlmostEqual(cost.rf_routing_T(64, 8, Costs(routing_scale=2.0)), 2 * base)


class TestAluCost(unittest.TestCase):
    def test_units_and_silicon(self):
        self.assertEqual(cost.alu_units(8, 2, "replicated"), 10)
        self.assertEqual(cost.alu_units(8, 2, "single"), 3)
        self.assertEqual(cost.alu_total_T(8, 0, "replicated"), 8 * 216)
        self.assertEqual(cost.alu_total_T(1, 0, "single"), 27)

    def test_hand_values(self):
        # single, K=8, 2 spares: 3 units * 216 + result select (9 lines * a 3-input mux = 18 T each)
        self.assertEqual(cost.alu_total_T(8, 2, "single"), 3 * 216 + 9 * 18)
        # replicated, K=8, 1 spare: operand mux into the spare 21 lines * 8-input (63 T) = 1,323
        # plus 8 positions * 9 result lines * 2-input (9 T) = 648
        self.assertEqual(cost.alu_routing_T(8, 1, "replicated"), 1323 + 648)

    def test_replicated_routing_dwarfs_single(self):
        # more positions means more routing: >10x at K = 1 and 8, and still more at K = 32 (2 positions)
        for k in (1, 8):
            self.assertGreater(cost.alu_routing_T(k, 4, "replicated"), 10 * cost.alu_routing_T(k, 4, "single"))
        self.assertGreater(cost.alu_routing_T(32, 4, "replicated"), cost.alu_routing_T(32, 4, "single"))

    def test_alu_factor(self):
        self.assertEqual(cost.alu_unit_T(8, Costs(alu_factor=4.0)), 4 * 216)

    def test_cost_per_good(self):
        self.assertEqual(cost.cost_per_good(100.0, 0.5), 200.0)
        self.assertEqual(cost.cost_per_good(100.0, 0.0), float("inf"))


class TestStudyNumbers(unittest.TestCase):
    """Pins the headline numbers of results/crossbar_study.md (independent defects, default costs)."""

    @staticmethod
    def per_good(n, s, c=Costs()):
        y = (cm.pooled_yield(256, s, P) if n == 1 else cm.domain_yield(n, 256 // n, s // n, P))
        return cost.rf_total_T(s, n, c) / y / BASE

    def test_ideal_pool_with_the_c2_design(self):
        self.assertAlmostEqual(self.per_good(1, 128), 7.11, places=2)
        self.assertAlmostEqual(self.per_good(1, 128, Costs(routing_scale=0.0)), 1.54, places=2)

    def test_eight_regions_beat_the_ideal_pool(self):
        best8 = min(self.per_good(8, s) for s in range(160, 233, 8))
        self.assertAlmostEqual(best8, 4.21, places=2)
        self.assertLess(best8, self.per_good(1, 128))
        self.assertLess(best8, self.per_good(4, 160))
        self.assertLess(best8, self.per_good(16, 256))


if __name__ == "__main__":
    unittest.main()
