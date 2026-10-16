#include <cmath>
#include <string>

#include "radslice/selftest.hpp"
#include "radslice/transient_alu.hpp"
#include "test_util.hpp"

// ---------------------------------------------------------------------------
// Transient upsets in the ALU and the re-execution schemes.
//
// The central test is exhaustive over single upsets: for every operation, every
// slice-unit call it makes, and every output bit (and the carry), force that one
// upset and check what each protection scheme returns.
// ---------------------------------------------------------------------------

using namespace radslice;

namespace {

constexpr AluOp kOps[] = {AluOp::Add, AluOp::Sub, AluOp::And, AluOp::Or, AluOp::Xor,
                          AluOp::Sll, AluOp::Srl, AluOp::Sra, AluOp::Slt, AluOp::Sltu,
                          AluOp::AddW, AluOp::SubW, AluOp::SllW, AluOp::SrlW, AluOp::SraW};

const char* name(AluOp op) {
    static const char* n[] = {"add", "sub", "and", "or", "xor", "sll", "srl", "sra",
                              "slt", "sltu", "addw", "subw", "sllw", "srlw", "sraw"};
    return n[static_cast<int>(op)];
}

void test_no_faults_all_modes_exact_and_beat_multipliers() {
    for (unsigned k : {8u, 16u, 32u}) {
        Rng rng(k);
        u64 base_beats = 0;
        for (AluProtection m : {AluProtection::None, AluProtection::Dwc, AluProtection::Tmr}) {
            TransientAlu alu(64, k, 0);
            RedundantAlu r(alu, m);
            Rng g(100 + k);
            for (int i = 0; i < 3000; ++i) {
                const u64 a = g.next(), b = g.next();
                const AluOp op = kOps[i % 15];
                CHECK(r.exec(op, a, b) == alu_reference(op, a, b));
            }
            CHECK(r.stats().detected == 0 && alu.upsets() == 0);
            if (m == AluProtection::None) base_beats = alu.stats().beats;
            else if (m == AluProtection::Dwc) CHECK(alu.stats().beats == 2 * base_beats);
            else CHECK(alu.stats().beats == 3 * base_beats);
        }
        (void)rng;
    }
}

void test_every_single_upset() {
    for (unsigned k : {8u, 16u, 32u}) {
        Rng g(7 + k);
        for (AluOp op : kOps) {
            const unsigned beats = SlicedAlu::beats_for(op, 64, k);
            for (int trial = 0; trial < 4; ++trial) {
                const u64 a = g.next(), b = g.next();
                const u64 want = alu_reference(op, a, b);
                // Calls 0 .. 3*beats-1 cover all three executions of a TMR op.
                for (u64 call = 0; call < 3ull * beats; ++call)
                    for (unsigned which = 0; which <= k; ++which) {
                        for (AluProtection m : {AluProtection::Dwc, AluProtection::Tmr}) {
                            TransientAlu alu(64, k, 0);
                            RedundantAlu r(alu, m);
                            alu.arm_upset(call, which);
                            const u64 got = r.exec(op, a, b);
                            CHECK_MSG(got == want,
                                      std::string(to_string(m)) + " K=" + std::to_string(k) + " " +
                                          name(op) + " call=" + std::to_string(call) +
                                          " bit=" + std::to_string(which));
                        }
                    }
            }
        }
    }
}

void test_unprotected_alu_is_corrupted_by_an_upset() {
    TransientAlu alu(64, 16, 0);
    RedundantAlu r(alu, AluProtection::None);
    alu.arm_upset(0, 3);                       // flip bit 3 of slice 0's output
    CHECK(r.exec(AluOp::Add, 100, 23) == (123 ^ 8));
}

void test_dwc_costs_a_third_run_only_on_mismatch() {
    TransientAlu alu(64, 16, 0);
    RedundantAlu r(alu, AluProtection::Dwc);
    CHECK(r.exec(AluOp::Xor, 5, 3) == 6);
    CHECK(r.stats().executions == 2 && r.stats().detected == 0);
    alu.reset_call_counter();
    alu.arm_upset(1, 2);                       // upset in the first execution
    CHECK(r.exec(AluOp::Xor, 5, 3) == 6);
    CHECK(r.stats().detected == 1 && r.stats().corrected == 1);
    CHECK(r.stats().executions == 2 + 3);
}

void test_two_upsets_in_the_same_place_defeat_the_vote() {
    // The same output bit upset in two of the three executions out-votes the good one.
    // Run three executions by hand through the inner ALU to place both upsets.
    TransientAlu alu(64, 16, 0);
    const u64 a = 1000, b = 234;
    alu.arm_upset(0, 5);
    const u64 r1 = alu.exec(AluOp::Add, a, b);
    alu.arm_upset(alu.slice_calls(), 5);        // first slice call of the second execution
    const u64 r2 = alu.exec(AluOp::Add, a, b);
    const u64 r3 = alu.exec(AluOp::Add, a, b);
    CHECK(r1 == r2 && r1 != r3);
    CHECK(((r1 & r2) | (r1 & r3) | (r2 & r3)) == r1);   // the voter picks the bad value
}

void test_rate_matches_expectation() {
    // Each slice-unit call is upset with probability (K+1) * rate.
    for (unsigned k : {8u, 32u}) {
        TransientAlu alu(64, k, 0, 99);
        alu.set_rate(1e-3);
        RedundantAlu r(alu, AluProtection::None);
        Rng g(5);
        for (int i = 0; i < 200000; ++i) (void)r.exec(AluOp::Add, g.next(), g.next());
        const double n = static_cast<double>(alu.slice_calls());
        const double p = (k + 1) * 1e-3;
        const double sigma = std::sqrt(n * p * (1 - p));
        CHECK_MSG(std::fabs(static_cast<double>(alu.upsets()) - n * p) < 4 * sigma,
                  "K=" + std::to_string(k) + " upsets " + std::to_string(alu.upsets()) +
                      " expected " + std::to_string(n * p));
    }
}

void test_composes_with_permanent_defects() {
    // A defective unit remapped away (M3) and then transient upsets on top, voted out.
    TransientAlu alu(64, 16, 2);
    SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = 4; d.value = true;
    alu.set_defect(1, d);
    CHECK(alu_self_test(alu, 1).ok);
    RedundantAlu r(alu, AluProtection::Tmr);
    Rng g(3);
    alu.set_rate(2e-3);
    for (int i = 0; i < 20000; ++i) {
        const u64 a = g.next(), b = g.next();
        const AluOp op = kOps[i % 15];
        r.exec(op, a, b);
    }
    CHECK(alu.upsets() > 0 && r.stats().detected > 0);
}

}  // namespace

int main() {
    test_no_faults_all_modes_exact_and_beat_multipliers();
    test_every_single_upset();
    test_unprotected_alu_is_corrupted_by_an_upset();
    test_dwc_costs_a_third_run_only_on_mismatch();
    test_two_upsets_in_the_same_place_defeat_the_vote();
    test_rate_matches_expectation();
    test_composes_with_permanent_defects();
    return tu::summary("transient_alu");
}
