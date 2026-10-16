#include <cmath>
#include <string>
#include <vector>

#include "radslice/chip_sim.hpp"
#include "radslice/defective_alu.hpp"
#include "radslice/selftest.hpp"
#include "test_util.hpp"

// ---------------------------------------------------------------------------
// Permanent defects, boot self-test and remap.
//
// Two kinds of check. Mechanism: a planted defect really corrupts results, the
// self-test finds it, and the remap routes around it. Statistics: over many sampled
// chips, the repair rate matches the closed-form binomial within Monte Carlo error.
// ---------------------------------------------------------------------------

using namespace radslice;

namespace {

// P(Binomial(n, p) >= k), independent of the Python model.
double tail(unsigned n, unsigned k, double p) {
    double s = 0;
    for (unsigned i = k; i <= n; ++i)
        s += std::exp(std::lgamma(n + 1.0) - std::lgamma(i + 1.0) - std::lgamma(n - i + 1.0) +
                      i * std::log(p) + (n - i) * std::log1p(-p));
    return s;
}

void check_rate(const char* what, unsigned long long hits, unsigned long long n, double exact) {
    const double est   = static_cast<double>(hits) / static_cast<double>(n);
    const double sigma = std::sqrt(exact * (1 - exact) / static_cast<double>(n));
    // Where the exact value is ~0 or ~1, sigma is ~0; allow a few events.
    const double tol = std::max(4 * sigma, 3.0 / static_cast<double>(n));
    CHECK_MSG(std::fabs(est - exact) <= tol,
              std::string(what) + ": simulated " + std::to_string(est) + " vs closed form " +
                  std::to_string(exact) + " (tolerance " + std::to_string(tol) + ")");
}

bool regfile_roundtrips(SlicedRegFile& rf, Rng& rng) {
    for (int pass = 0; pass < 4; ++pass) {
        u64 want[32];
        for (u32 r = 0; r < 32; ++r) { want[r] = rng.next(); rf.write(r, want[r]); }
        for (u32 r = 0; r < 32; ++r) if (rf.read(r) != want[r]) return false;
    }
    return true;
}

bool alu_matches_plain(DefectiveSlicedAlu& alu, Rng& rng, int n = 20000) {
    constexpr AluOp ops[] = {AluOp::Add, AluOp::Sub, AluOp::And, AluOp::Or, AluOp::Xor,
                             AluOp::Sll, AluOp::Srl, AluOp::Sra, AluOp::Slt, AluOp::Sltu,
                             AluOp::AddW, AluOp::SubW, AluOp::SllW, AluOp::SrlW, AluOp::SraW};
    for (int i = 0; i < n; ++i) {
        const u64 a = rng.next(), b = rng.next();
        const AluOp op = ops[i % 15];
        if (alu.exec(op, a, b) != alu_reference(op, a, b)) return false;
    }
    return true;
}

// ---- mechanism -------------------------------------------------------------

void test_closed_form_helper() {
    CHECK(std::fabs(tail(8, 8, 0.71) - std::pow(0.71, 8)) < 1e-12);
    CHECK(std::fabs(tail(5, 0, 0.3) - 1.0) < 1e-12);
}

void test_planted_regfile_defect_is_found_and_remapped() {
    Rng rng(11);
    SlicedRegFile rf(8);
    SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = 2; d.value = true;
    rf.set_defect(5 * 8 + 3, d);                       // word 5, byte 3

    CHECK(!regfile_roundtrips(rf, rng));               // the defect really corrupts data
    const RepairReport rep = regfile_self_test(rf, 1);
    CHECK(rep.ok && rep.bad_primary == 1 && rep.remapped == 1 && rep.bad_spare == 0);
    CHECK(rf.remap(5 * 8 + 3) >= SlicedRegFile::kPrimarySlices);   // moved to a spare
    CHECK(regfile_roundtrips(rf, rng));                // and now everything works
}

void test_defective_spare_is_skipped() {
    Rng rng(12);
    SlicedRegFile rf(4);
    SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = 0; d.value = false;
    rf.set_defect(10, d);
    rf.set_defect(SlicedRegFile::kPrimarySlices + 0, d);   // the first spare is bad too
    const RepairReport rep = regfile_self_test(rf, 1);
    CHECK(rep.ok && rep.bad_primary == 1 && rep.bad_spare == 1);
    CHECK(rf.remap(10) == SlicedRegFile::kPrimarySlices + 1);
    CHECK(regfile_roundtrips(rf, rng));
}

void test_too_many_defects_rejects_the_chip() {
    SlicedRegFile rf(2);
    SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = 7; d.value = true;
    for (unsigned s : {3u, 40u, 200u}) rf.set_defect(s, d);
    const RepairReport rep = regfile_self_test(rf, 1);
    CHECK(!rep.ok && rep.bad_primary == 3 && rep.remapped == 2);
}

void test_every_single_fault_is_detected() {
    // The complete modelled fault set, one fault at a time: any stuck bit of a
    // register slice, and any stuck data bit or stuck carry of an ALU unit.
    for (unsigned bit = 0; bit < 8; ++bit)
        for (bool v : {false, true}) {
            SlicedRegFile rf(0);
            SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = bit; d.value = v;
            rf.set_defect(77, d);
            CHECK(regfile_self_test(rf, 1).bad_primary == 1);
        }
    for (unsigned k : {8u, 16u, 32u})
        for (unsigned bit = 0; bit <= k; ++bit)       // bit == k means "the carry"
            for (bool v : {false, true}) {
                DefectiveSlicedAlu alu(64, k, 1);
                SliceDefect d; d.value = v;
                if (bit == k) d.kind = DefectKind::StuckCarry;
                else { d.kind = DefectKind::StuckBit; d.bit = bit; }
                alu.set_defect(0, d);
                const RepairReport rep = alu_self_test(alu, 1);
                CHECK(rep.ok && rep.bad_primary == 1);
            }
}

void test_alu_defect_corrupts_then_remap_repairs() {
    for (unsigned k : {8u, 16u, 32u}) {
        Rng rng(100 + k);
        DefectiveSlicedAlu alu(64, k, 2);
        SliceDefect data; data.kind = DefectKind::StuckBit; data.bit = 3; data.value = false;
        SliceDefect carry; carry.kind = DefectKind::StuckCarry; carry.value = true;
        alu.set_defect(1, data);
        alu.set_defect(0, carry);

        CHECK(!alu_matches_plain(alu, rng));            // defects really corrupt results
        const RepairReport rep = alu_self_test(alu, 1);
        CHECK(rep.ok && rep.bad_primary == 2);
        CHECK(alu.mapping(0) >= 2 && alu.mapping(1) >= 2);
        CHECK(alu_matches_plain(alu, rng));             // and the repaired ALU is exact
    }
}

void test_alu_rejected_when_not_enough_good_units() {
    DefectiveSlicedAlu alu(64, 32, 0);                  // 2 units, no spares
    SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = 0; d.value = true;
    alu.set_defect(1, d);
    CHECK(!alu_self_test(alu, 1).ok);
}

// ---- statistics ------------------------------------------------------------

void test_hard_defects_never_escape() {
    ChipConfig cfg;
    cfg.rf_spares = 128; cfg.alu_spares = 6; cfg.k = 16; cfg.verify = true;
    Rng rng(2024);
    unsigned verified = 0, escapes = 0;
    for (int t = 0; t < 4000; ++t) {
        const ChipResult r = simulate_chip(cfg, rng);
        verified += r.verified;
        escapes  += r.escape;
    }
    CHECK(verified > 2000);        // plenty of chips passed the test and were checked
    CHECK(escapes == 0);           // and none of them computed wrongly
}

void test_defect_sampling_rate() {
    ChipConfig cfg;
    cfg.rf_spares = 0; cfg.alu_spares = 0;
    Rng rng(7);
    const int n = 20000;
    double sum = 0;
    for (int t = 0; t < n; ++t) sum += simulate_chip(cfg, rng).rf_defective_primary;
    const double mean = sum / n, expect = 256 * (1 - cfg.p);
    const double sigma = std::sqrt(256 * cfg.p * (1 - cfg.p) / n);
    CHECK_MSG(std::fabs(mean - expect) < 4 * sigma,
              "mean defective primaries " + std::to_string(mean) + " vs " + std::to_string(expect));
}

void test_register_file_yield_matches_closed_form() {
    const unsigned long long n = 20000;
    for (unsigned spares : {112u, 128u, 144u}) {
        ChipConfig cfg; cfg.rf_spares = spares; cfg.alu_spares = 8; cfg.k = 16;
        Rng rng(5000 + spares);
        unsigned long long ok = 0;
        for (unsigned long long t = 0; t < n; ++t) ok += simulate_chip(cfg, rng).rf_ok;
        check_rate(("rf spares=" + std::to_string(spares)).c_str(), ok, n,
                   tail(256 + spares, 256, cfg.p));
    }
}

void test_alu_yield_matches_closed_form() {
    const unsigned long long n = 20000;
    struct C { unsigned k, spares; };
    for (C c : {C{8, 3}, C{8, 6}, C{16, 2}, C{16, 4}, C{32, 1}, C{32, 3}}) {
        ChipConfig cfg; cfg.k = c.k; cfg.alu_spares = c.spares; cfg.rf_spares = 160;
        Rng rng(9000 + c.k * 10 + c.spares);
        unsigned long long ok = 0;
        for (unsigned long long t = 0; t < n; ++t) ok += simulate_chip(cfg, rng).alu_ok;
        const unsigned units = 64 / c.k;
        check_rate(("alu K=" + std::to_string(c.k) + " spares=" + std::to_string(c.spares)).c_str(),
                   ok, n, tail(units + c.spares, units, std::pow(cfg.p, c.k / 8.0)));
    }
}

void test_weak_defect_escape_matches_closed_form() {
    // A weak defect shows with probability q on each read. Two of the four test
    // patterns expose a given stuck bit, so one round misses it with (1-q)^2.
    const unsigned long long n = 20000;
    for (double q : {0.1, 0.3}) {
        for (unsigned rounds : {1u, 2u, 4u, 8u}) {
            Rng rng(31 + rounds + static_cast<unsigned>(q * 100));
            unsigned long long hit = 0;
            for (unsigned long long t = 0; t < n; ++t) hit += detect_weak_slice(q, rounds, rng);
            check_rate(("weak q=" + std::to_string(q) + " rounds=" + std::to_string(rounds)).c_str(),
                       hit, n, 1.0 - std::pow(1.0 - q, 2.0 * rounds));
        }
    }
}

// Per-run random streams must be unrelated. Seeds of the form seed + index * golden
// ratio (what this project first used) give the same stream shifted by one draw, which
// made runs and yield rows correlated.
bool streams_overlap(Rng a, Rng b, int n) {
    u64 av[64];
    for (int i = 0; i < n; ++i) av[i] = a.next();
    for (int i = 0; i < n; ++i) {
        const u64 x = b.next();
        for (int j = 0; j < n; ++j)
            if (av[j] == x) return true;
    }
    return false;
}

void test_per_run_streams_are_not_shifted_copies() {
    const u64 gamma = 0x9e3779b97f4a7c15ull;
    int old_overlaps = 0, new_overlaps = 0;
    for (u64 i = 0; i < 200; ++i) {
        old_overlaps += streams_overlap(Rng(1 + gamma * (i + 1)), Rng(1 + gamma * (i + 2)), 64);
        new_overlaps += streams_overlap(Rng::stream(1, i), Rng::stream(1, i + 1), 64);
    }
    CHECK(old_overlaps == 200);   // the old construction: every neighbouring pair overlaps
    CHECK(new_overlaps == 0);     // the hashed construction: none do
    // Same index, same seed -> same stream; different seeds -> different streams.
    CHECK(Rng::stream(5, 3).next() == Rng::stream(5, 3).next());
    CHECK(Rng::stream(5, 3).next() != Rng::stream(6, 3).next());
}

}  // namespace

int main() {
    test_per_run_streams_are_not_shifted_copies();
    test_closed_form_helper();
    test_planted_regfile_defect_is_found_and_remapped();
    test_defective_spare_is_skipped();
    test_too_many_defects_rejects_the_chip();
    test_every_single_fault_is_detected();
    test_alu_defect_corrupts_then_remap_repairs();
    test_alu_rejected_when_not_enough_good_units();
    test_hard_defects_never_escape();
    test_defect_sampling_rate();
    test_register_file_yield_matches_closed_form();
    test_alu_yield_matches_closed_form();
    test_weak_defect_escape_matches_closed_form();
    return tu::summary("defects");
}
