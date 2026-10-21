import math
import random
import unittest

from radslice import cluster_model as cm
from radslice import yield_model as ym

P = 0.71


def mc_pooled(primaries, spares, alpha, block, trials, seed):
    """Independent Monte Carlo of the same model, using Python's own gamma sampler."""
    rng = random.Random(seed)
    lam = cm.lam_for(P, alpha)
    sizes = cm._blocks(primaries + spares, block)
    ok = 0
    for _ in range(trials):
        bad = 0
        for size in sizes:
            m = rng.gammavariate(alpha, 1.0 / alpha)
            q = 1.0 - math.exp(-lam * m)
            bad += sum(1 for _ in range(size) if rng.random() < q)
        ok += bad <= spares
    return ok / trials


def mc_domains(domains, primaries, spares, alpha, block, trials, seed):
    rng = random.Random(seed)
    lam = cm.lam_for(P, alpha)
    d = primaries + spares
    ok = 0
    for _ in range(trials):
        mult = {}
        good = True
        for dom in range(domains):
            b = (dom * d) // block               # which block this domain's first slice is in
            if b not in mult:
                mult[b] = rng.gammavariate(alpha, 1.0 / alpha)
        for dom in range(domains):
            bad = 0
            for s in range(d):
                b = (dom * d + s) // block
                q = 1.0 - math.exp(-lam * mult.setdefault(b, rng.gammavariate(alpha, 1.0 / alpha)))
                bad += rng.random() < q
            good = good and bad <= spares
        ok += good
    return ok / trials


class Quadrature(unittest.TestCase):
    def test_marginal_yield_is_preserved_for_every_alpha(self):
        for alpha in (0.3, 1.0, 4.0, 30.0):
            lam = cm.lam_for(P, alpha)
            ms, ws = cm._grid(alpha)
            self.assertAlmostEqual(sum(ws), 1.0, places=9)
            self.assertAlmostEqual(sum(w * math.exp(-lam * m) for m, w in zip(ms, ws)), P, places=4,
                                   msg=f"alpha={alpha}")

    def test_block_pmf_is_a_distribution_with_the_right_mean(self):
        for alpha in (0.3, 2.0, 25.0):
            pmf = cm.block_pmf(16, P, alpha)
            self.assertAlmostEqual(sum(pmf), 1.0, places=6)
            self.assertAlmostEqual(sum(k * x for k, x in enumerate(pmf)), 16 * (1 - P), places=3)

    def test_large_alpha_is_the_binomial(self):
        for alpha in (1e4, None):
            pmf = cm.block_pmf(12, P, alpha)
            for k in range(13):
                exact = math.comb(12, k) * (1 - P) ** k * P ** (12 - k)
                self.assertAlmostEqual(pmf[k], exact, delta=2e-4)

    def test_correlation_formula_matches_the_pmf(self):
        for alpha in (0.5, 2.0, 10.0):
            pmf = cm.block_pmf(2, P, alpha)
            cov = pmf[2] - (1 - P) ** 2
            self.assertAlmostEqual(cov / (P * (1 - P)), cm.correlation(P, alpha), places=4)

    def test_count_variance_grows_with_block_size_as_theory_says(self):
        n, alpha = 20, 3.0
        pmf = cm.block_pmf(n, P, alpha)
        mean = n * (1 - P)
        var = sum((k - mean) ** 2 * x for k, x in enumerate(pmf))
        rho = cm.correlation(P, alpha)
        self.assertAlmostEqual(var, n * P * (1 - P) * (1 + (n - 1) * rho), places=2)


class PooledYield(unittest.TestCase):
    def test_independent_case_matches_the_closed_form_used_so_far(self):
        for s in (96, 128, 144):
            self.assertAlmostEqual(cm.pooled_yield(256, s, P, None), ym.file_yield_shared_pool(256, s, P), places=9)
        self.assertAlmostEqual(cm.pooled_yield(256, 128, P, 5e4, block=16), ym.file_yield_shared_pool(256, 128, P), delta=5e-4)

    def test_clustering_flattens_the_curve(self):
        # Same marginal yield; the independent curve is steep, the clustered one is not.
        self.assertGreater(cm.pooled_yield(256, 96, P, 4.0), cm.pooled_yield(256, 96, P, None))     # low target easier
        self.assertLess(cm.pooled_yield(256, 128, P, 4.0), cm.pooled_yield(256, 128, P, None) - 0.2)  # high target harder
        ys = [cm.pooled_yield(256, s, P, 1.0) for s in range(64, 200, 16)]
        self.assertEqual(ys, sorted(ys))

    def test_die_level_clustering_caps_the_yield(self):
        # With strong die-level clustering some dies are simply bad, whatever the spares.
        self.assertLess(cm.pooled_yield(256, 192, P, 0.3), 0.75)

    def test_matches_an_independent_monte_carlo(self):
        for (pr, sp, alpha, block) in ((64, 24, 2.0, 16), (64, 24, 3.0, None), (96, 30, 0.7, 24)):
            exact = cm.pooled_yield(pr, sp, P, alpha, block)
            est = mc_pooled(pr, sp, alpha, block, 12000, seed=11)
            sigma = math.sqrt(max(exact * (1 - exact), 1e-6) / 12000)
            self.assertLess(abs(est - exact), 4.5 * sigma + 2e-3, (pr, sp, alpha, block, exact, est))


class DomainYield(unittest.TestCase):
    def test_independent_case_is_a_product_of_binomial_tails(self):
        exact = ym.binom_tail(16 + 4, 16, P) ** 4
        self.assertAlmostEqual(cm.domain_yield(4, 16, 4, P, None), exact, places=9)

    def test_matches_an_independent_monte_carlo_when_blocks_and_domains_align(self):
        # domain = 20 slices; block of 20 (= domain), 40 (two domains), 10 (half a domain)
        for block in (20, 40, 10):
            exact = cm.domain_yield(4, 16, 4, P, 1.5, block)
            est = mc_domains(4, 16, 4, 1.5, block, 12000, seed=5)
            sigma = math.sqrt(max(exact * (1 - exact), 1e-6) / 12000)
            self.assertLess(abs(est - exact), 4.5 * sigma + 2e-3, (block, exact, est))

    def test_misaligned_blocks_are_refused_not_silently_wrong(self):
        with self.assertRaises(ValueError):
            cm.domain_yield(4, 16, 4, P, 1.5, 30)

    def test_restricted_pools_lose_to_the_ideal_pool(self):
        ideal = cm.pooled_yield(64, 16, P, 2.0, block=20)
        domain = cm.domain_yield(4, 16, 4, P, 2.0, 20)
        self.assertLess(domain, ideal)


class ReferenceValuesUsedByTheCppTests(unittest.TestCase):
    """tests/test_cluster.cpp checks the C++ simulator against these exact values; pinning them
    here means neither implementation can drift without a test failing."""

    def test_pooled(self):
        for s, want in ((96, 0.4825), (128, 0.6652), (160, 0.7896)):
            self.assertAlmostEqual(cm.pooled_yield(256, s, P, 4.0), want, places=4)
        self.assertAlmostEqual(cm.pooled_yield(256, 112, P, 2.0, block=32), 0.6280, places=4)
        self.assertAlmostEqual(cm.pooled_yield(256, 128, P, 2.0, block=32), 0.8089, places=4)

    def test_region_restricted(self):
        self.assertAlmostEqual(cm.domain_yield(8, 32, 16, P, None), 0.1614, places=4)
        self.assertAlmostEqual(cm.domain_yield(8, 32, 16, P, 2.0, 48), 0.0327, places=4)
        self.assertAlmostEqual(cm.domain_yield(8, 32, 16, P, 2.0, 96), 0.1141, places=4)

    def test_copies_sharing_a_die(self):
        self.assertAlmostEqual(cm.die_level_chip_yield(3, 128, 16, 0, P, None, False), 0.9178, places=4)
        self.assertAlmostEqual(cm.die_level_chip_yield(3, 128, 16, 0, P, 3.0, False), 0.6033, places=4)
        self.assertAlmostEqual(cm.die_level_chip_yield(1, 128, 16, 0, P, 3.0, False), 0.6528, places=4)
        self.assertAlmostEqual(cm.die_level_chip_yield(3, 128, 16, 8, P, 3.0, True), 0.5889, places=4)

    def test_single_unit_alu(self):
        self.assertAlmostEqual(ym.alu_yield_single(16, 1, P), 0.7541, places=4)
        self.assertAlmostEqual(cm.die_level_chip_yield(0, 0, 16, 1, P, None, True, alu_positions=1), 0.7541, places=4)
        self.assertAlmostEqual(cm.die_level_chip_yield(0, 0, 16, 2, P, 4.0, True, alu_positions=1), 0.8500, places=4)
        self.assertAlmostEqual(cm.die_level_chip_yield(0, 0, 8, 1, P, 4.0, True, alu_positions=1), 0.9021, places=4)

    def test_independent_limit_of_the_chip_formula(self):
        self.assertAlmostEqual(cm.die_level_chip_yield(1, 128, 16, 0, P, None, False),
                               ym.file_yield_shared_pool(256, 128, P), places=9)
        self.assertAlmostEqual(cm.die_level_chip_yield(1, 128, 16, 8, P, None, True),
                               ym.file_yield_shared_pool(256, 128, P) * ym.alu_yield(16, 8, P), places=9)


class WhatTheWaferMapAllows(unittest.TestCase):
    def test_the_parameter_matches_the_computed_bound(self):
        from radslice.params import load_params
        from radslice.wafer_map import load_grid
        counts = [r.count("R") for r in load_grid()]
        p_good = 1 - sum(counts) / (8 * len(counts))
        self.assertAlmostEqual(load_params()["yield.cluster_alpha_min_matrix_scale"].value,
                               cm.alpha_lower_bound(counts, 8, p_good), places=2)

    def test_the_published_map_bounds_clustering_at_matrix_scale(self):
        from radslice.wafer_map import load_grid
        counts = [r.count("R") for r in load_grid()]
        p_good = 1 - sum(counts) / (8 * len(counts))
        a = cm.alpha_lower_bound(counts, 8, p_good)
        self.assertIsNotNone(a)
        self.assertTrue(5.0 < a < 15.0, a)                         # about 9.4
        self.assertLess(cm.correlation(p_good, a), 0.05)            # intra-block correlation at most a few percent
        # The data are, if anything, under-dispersed, so independence is the best fit.
        self.assertGreater(cm.loglik(counts, 8, p_good, None), cm.loglik(counts, 8, p_good, a))


if __name__ == "__main__":
    unittest.main()
