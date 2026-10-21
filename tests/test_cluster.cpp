#include <cmath>
#include <string>
#include <vector>

#include "radslice/chip_sim.hpp"
#include "radslice/defective_alu.hpp"
#include "radslice/selftest.hpp"
#include "test_util.hpp"

// ---------------------------------------------------------------------------
// Clustered defects (G9): the gamma sampler, the clustered chip simulator, and region-restricted
// sparing.
//
// The reference yields below come from the exact model in python/radslice/cluster_model.py, which is
// an independent implementation (numerical integration and convolution, not sampling). The same
// numbers are pinned by python/tests/test_cluster_model.py so neither side can drift.
// ---------------------------------------------------------------------------

using namespace radslice;

namespace {

constexpr double P = 0.71;

double tail(unsigned n, unsigned k, double p) {          // P(Binomial(n, p) >= k)
    double s = 0;
    for (unsigned i = k; i <= n; ++i)
        s += std::exp(std::lgamma(n + 1.0) - std::lgamma(i + 1.0) - std::lgamma(n - i + 1.0) +
                      i * std::log(p) + (n - i) * std::log1p(-p));
    return s;
}

ChipConfig base(unsigned rf_spares) {
    ChipConfig c;
    c.rf_spares = rf_spares;
    c.alu_spares = 8;
    return c;
}

// Fraction of simulated chips whose register file (all copies) was repaired, with the exact value
// it should match; checked within Monte Carlo error.
void check_rf_yield(const char* what, const ChipConfig& cfg, double exact, u64 seed, int n = 20000) {
    Rng rng(seed);
    int ok = 0;
    for (int t = 0; t < n; ++t) ok += simulate_chip(cfg, rng).rf_ok;
    const double est = static_cast<double>(ok) / n;
    const double sigma = std::sqrt(std::max(exact * (1 - exact), 1e-4) / n);
    CHECK_MSG(std::fabs(est - exact) < 4.5 * sigma + 1e-3,
              std::string(what) + ": simulated " + std::to_string(est) + " vs exact " + std::to_string(exact));
}

// Fraction of simulated chips whose ALU was repaired, against an exact value.
void check_alu_yield(const char* what, const ChipConfig& cfg, double exact, u64 seed, int n = 20000) {
    Rng rng(seed);
    int ok = 0;
    for (int t = 0; t < n; ++t) ok += simulate_chip(cfg, rng).alu_ok;
    const double est = static_cast<double>(ok) / n;
    const double sigma = std::sqrt(std::max(exact * (1 - exact), 1e-4) / n);
    CHECK_MSG(std::fabs(est - exact) < 4.5 * sigma + 1e-3,
              std::string(what) + ": simulated " + std::to_string(est) + " vs exact " + std::to_string(exact));
}

void test_gamma_sampler() {
    for (double alpha : {0.3, 1.0, 4.0, 25.0}) {
        Rng rng(static_cast<u64>(alpha * 100));
        const int n = 400000;
        double sum = 0, sq = 0;
        for (int i = 0; i < n; ++i) {
            const double m = rng.gamma(alpha) / alpha;   // mean 1, variance 1/alpha
            sum += m; sq += m * m;
        }
        const double mean = sum / n, var = sq / n - mean * mean;
        CHECK_MSG(std::fabs(mean - 1.0) < 4.5 * std::sqrt(1.0 / (alpha * n)), "gamma mean alpha=" + std::to_string(alpha));
        CHECK_MSG(std::fabs(var - 1.0 / alpha) < 0.05 / alpha + 0.01, "gamma variance alpha=" + std::to_string(alpha));
    }
}

void test_marginal_slice_yield_is_preserved_under_clustering() {
    // The whole point of the calibration: clustering must not change the average defect density.
    for (double alpha : {0.5, 2.0, 8.0}) {
        ChipConfig cfg = base(0);
        cfg.cluster_alpha = alpha;
        cfg.cluster_block = 32;
        Rng rng(7);
        const int chips = 6000;
        std::vector<double> frac;
        double sum = 0;
        for (int t = 0; t < chips; ++t) {
            const double f = simulate_chip(cfg, rng).rf_defective_total / 256.0;
            frac.push_back(f); sum += f;
        }
        const double mean = sum / chips;
        double var = 0;
        for (double f : frac) var += (f - mean) * (f - mean);
        var /= (chips - 1);
        CHECK_MSG(std::fabs(mean - (1 - P)) < 4.5 * std::sqrt(var / chips),
                  "alpha=" + std::to_string(alpha) + " mean defect fraction " + std::to_string(mean));
    }
}

void test_pooled_yield_matches_the_exact_model() {
    for (auto [s, exact] : {std::pair<unsigned, double>{96, 0.4825}, {128, 0.6652}, {160, 0.7896}}) {
        ChipConfig c = base(s); c.cluster_alpha = 4.0;                        // die-level clustering
        check_rf_yield(("die-level alpha=4, S=" + std::to_string(s)).c_str(), c, exact, 100 + s);
    }
    for (auto [s, exact] : {std::pair<unsigned, double>{112, 0.628}, {128, 0.8089}}) {
        ChipConfig c = base(s); c.cluster_alpha = 2.0; c.cluster_block = 32;  // blocks of 32 slices
        check_rf_yield(("block=32 alpha=2, S=" + std::to_string(s)).c_str(), c, exact, 200 + s);
    }
}

void test_independent_path_is_unchanged() {
    ChipConfig c = base(128);                                                // cluster_alpha = 0
    check_rf_yield("independent, S=128", c, tail(256 + 128, 256, P), 300);
}

void test_region_restricted_spares() {
    // 8 regions of 32 primaries, each with its own 16 spares. Independent: a product of binomial tails.
    ChipConfig c = base(128); c.spare_domains = 8;
    const double indep = std::pow(tail(48, 32, P), 8);
    CHECK(std::fabs(indep - 0.1614) < 5e-4);
    check_rf_yield("8 regions, independent", c, indep, 400);
    c.cluster_alpha = 2.0; c.cluster_block = 48;                             // a block is exactly a region
    check_rf_yield("8 regions, alpha=2, block = region", c, 0.0327, 401);
    c.cluster_block = 96;                                                    // a block is two regions
    check_rf_yield("8 regions, alpha=2, block = 2 regions", c, 0.1141, 402);
    // And the ideal pool with the same spares is far better than the restricted one.
    CHECK(tail(256 + 128, 256, P) > 0.95);
}

void test_remap_respects_regions() {
    // Two regions, one spare each. Two bad primaries in region 0 cannot both be repaired by region 1's spare.
    SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = 1; d.value = true;
    {
        SlicedRegFile rf(2);
        rf.set_defect(5, d); rf.set_defect(6, d);                            // both in region 0
        CHECK(regfile_self_test(rf, 1, 1).ok);                               // ideal pool: two spares, two defects
    }
    {
        SlicedRegFile rf(2);
        rf.set_defect(5, d); rf.set_defect(6, d);
        const RepairReport r = regfile_self_test(rf, 1, 2);                  // one spare per region
        CHECK(!r.ok && r.remapped == 1);
    }
    {
        SlicedRegFile rf(2);
        rf.set_defect(5, d); rf.set_defect(200, d);                          // one in each region
        CHECK(regfile_self_test(rf, 1, 2).ok);
        CHECK(rf.remap(5) == 256 && rf.remap(200) == 257);                   // each used its own region's spare
    }
    {
        SlicedRegFile rf(3);                                                 // 3 spares cannot split into 2 regions
        CHECK(!regfile_self_test(rf, 1, 2).ok);
        SlicedRegFile rf2(2);
        CHECK(!regfile_self_test(rf2, 1, 3).ok);                             // 256 is not divisible by 3
    }
}

void test_tmr_copies_and_a_shared_die() {
    ChipConfig c = base(128); c.rf_copies = 3;
    const double one = tail(256 + 128, 256, P);
    check_rf_yield("3 independent copies", c, std::pow(one, 3), 500);        // 0.9178
    c.cluster_alpha = 3.0; c.share_die = true;
    check_rf_yield("3 copies sharing a die, alpha=3", c, 0.6033, 501);
    c.rf_copies = 1;
    check_rf_yield("1 copy on a clustered die, alpha=3", c, 0.6528, 502);
    // Whole chip with the ALU (K=16, 8 spares), three copies, shared die.
    ChipConfig w = base(128); w.rf_copies = 3; w.cluster_alpha = 3.0; w.share_die = true;
    Rng rng(503);
    int ok = 0; const int n = 20000;
    for (int t = 0; t < n; ++t) { const ChipResult r = simulate_chip(w, rng); ok += (r.rf_ok && r.alu_ok); }
    const double est = static_cast<double>(ok) / n, exact = 0.5889;
    CHECK_MSG(std::fabs(est - exact) < 4.5 * std::sqrt(exact * (1 - exact) / n) + 1e-3,
              "whole chip: " + std::to_string(est) + " vs " + std::to_string(exact));
}

// One K-bit unit reused for every slice position, plus spare units (G10).
void test_single_unit_alu() {
    constexpr AluOp ops[] = {AluOp::Add, AluOp::Sub, AluOp::And, AluOp::Or, AluOp::Xor, AluOp::Sll, AluOp::Srl,
                             AluOp::Sra, AluOp::Slt, AluOp::Sltu, AluOp::AddW, AluOp::SubW, AluOp::SllW,
                             AluOp::SrlW, AluOp::SraW};
    auto matches = [&](DefectiveSlicedAlu& alu, u64 seed) {
        Rng r(seed);
        for (int i = 0; i < 6000; ++i) {
            const u64 a = r.next(), b = r.next();
            const AluOp op = ops[i % 15];
            if (alu.exec(op, a, b) != alu_reference(op, a, b)) return false;
        }
        return true;
    };
    for (unsigned k : {8u, 16u, 32u}) {
        DefectiveSlicedAlu alu(64, k, 1, 5, true);
        CHECK(alu.single_unit() && alu.physical_units() == 2);       // one reused unit + one spare
        SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = 2; d.value = false;
        alu.set_defect(0, d);
        CHECK(!matches(alu, 1));                                      // the reused unit is bad, so everything is wrong
        CHECK(alu_self_test(alu, 1).ok);
        for (unsigned i = 0; i < alu.num_slices(); ++i) CHECK(alu.mapping(i) == 1);   // every position now uses the spare
        CHECK(matches(alu, 2));
        alu.set_defect(1, d);
        CHECK(!alu_self_test(alu, 1).ok);                             // both units bad: no repair is possible
    }
    // Yield: it works if any of the 1 + spares units is good.
    ChipConfig c = base(160); c.alu_single_unit = true; c.k = 16; c.alu_spares = 1;
    check_alu_yield("single unit K=16, 1 spare, independent", c, 0.7541, 600);
    c.cluster_alpha = 4.0; c.alu_spares = 2;
    check_alu_yield("single unit K=16, 2 spares, die-level alpha=4", c, 0.85, 601);
    c.k = 8; c.alu_spares = 1;
    check_alu_yield("single unit K=8, 1 spare, die-level alpha=4", c, 0.9021, 602);
}

}  // namespace

int main() {
    test_gamma_sampler();
    test_marginal_slice_yield_is_preserved_under_clustering();
    test_independent_path_is_unchanged();
    test_pooled_yield_matches_the_exact_model();
    test_region_restricted_spares();
    test_remap_respects_regions();
    test_tmr_copies_and_a_shared_die();
    test_single_unit_alu();
    return tu::summary("cluster");
}
