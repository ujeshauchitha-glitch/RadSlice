#include <cstdlib>
#include <string>

#include "radslice/experiment.hpp"
#include "test_util.hpp"

// ---------------------------------------------------------------------------
// The fault-injection harness: golden outputs, zero-fault equivalence, and
// deterministic fault scenarios whose classification can be predicted.
//
// Needs the workload ELFs: run scripts/build-workloads.sh first (needs a RISC-V gcc).
// ---------------------------------------------------------------------------

using namespace radslice;

#ifndef RADSLICE_WORKLOAD_DIR
#define RADSLICE_WORKLOAD_DIR "build/workloads"
#endif

namespace {

const char* kNames[] = {"coremark_like", "chacha20", "irq_loop"};

std::string elf(const std::string& n) { return std::string(RADSLICE_WORKLOAD_DIR) + "/" + n + ".elf"; }

struct Loaded {
    Workload w;
    Golden   g;
    explicit Loaded(const std::string& n) : w(elf(n)), g(w.ok() ? w.run_golden() : Golden{}) {}
};

RunResult go(const Loaded& L, Protection p, FaultRates r = {}, ChipSpec c = {}, ForcedFault f = {},
             u64 seed = 1) {
    Rng rng(seed);
    return L.w.run(L.g, p, r, c, f, rng);
}

Protection P(RfProtection rf, AluProtection alu = AluProtection::None, unsigned scrub = 0) {
    Protection p; p.rf = rf; p.alu = alu; p.scrub_period = scrub; return p;
}

ForcedFault flip_sp(u64 at, unsigned copy = 0) {
    ForcedFault f; f.active = true; f.at_instret = at; f.copy = copy; f.word = 2; f.bit = 30;
    return f;
}

void test_missing_workload_is_reported() {
    Workload w("no/such/file.elf");
    CHECK(!w.ok() && !w.error().empty());
}

void test_golden_runs_and_known_outputs() {
    for (const char* n : kNames) {
        Loaded L(n);
        CHECK_MSG(L.w.ok(), std::string("cannot load ") + elf(n) + " (run scripts/build-workloads.sh)");
        if (!L.w.ok()) continue;
        CHECK_MSG(L.g.ok, std::string(n) + ": " + L.g.error);
        CHECK(L.g.instret > 10000 && L.g.instret < 100000);
        CHECK(L.g.trace.size() == L.g.instret);
    }
    {   // ChaCha20: RFC 8439 section 2.3.2 block, computed independently in Python.
        Loaded L("chacha20");
        const u64 block0[8] = {0x15593bd1e4e7f110ull, 0xc47120a31fdd0f50ull, 0x0368c033c7f4d1c7ull,
                               0x4e6cd4c39aaa2204ull, 0x09aa9f07466482d2ull, 0xa2028bd905d7c214ull,
                               0xb94e16ded19c12b5ull, 0x4e3c50a2e883d0cbull};
        const u64 fold[8]   = {0xe50610604ca1957dull, 0x59efd5b5eac1f68eull, 0x9d7e436b8275f788ull,
                               0xcbc7df7c140dc97full, 0xa7008ed63a251733ull, 0x32f64cf5247ad0ccull,
                               0x2ae8717226bbe2feull, 0x9073de4c99eda31full};
        for (int i = 0; i < 8; ++i) {
            CHECK_MSG(L.g.result[8 + i] == block0[i], "chacha20 block 0 word pair " + std::to_string(i));
            CHECK_MSG(L.g.result[i] == fold[i], "chacha20 fold word pair " + std::to_string(i));
        }
        CHECK(L.g.result[16] == 0x421b6499ull);
    }
    {   // The interrupt workload really took its 60 interrupts.
        Loaded L("irq_loop");
        CHECK(L.g.result[0] == 60);
    }
}

void test_zero_faults_every_configuration_is_exact() {
    const Protection configs[] = {
        P(RfProtection::None), P(RfProtection::Parity), P(RfProtection::ParityDup),
        P(RfProtection::Tmr), P(RfProtection::Tmr, AluProtection::Dwc, 64),
        P(RfProtection::ParityDup, AluProtection::Tmr, 16)};
    for (const char* n : kNames) {
        Loaded L(n);
        if (!L.g.ok) continue;
        const RunResult base = go(L, P(RfProtection::None));
        for (const Protection& p : configs) {
            const RunResult r = go(L, p);
            CHECK_MSG(r.outcome == Outcome::Correct, std::string(n) + " " + to_string(p.rf) + "/" + to_string(p.alu));
            CHECK(r.instret == L.g.instret && r.result == L.g.result && r.first_divergence == -1);
            CHECK(r.rf_upsets == 0 && r.alu_upsets == 0);
            CHECK(r.model_cycles >= base.model_cycles);    // protection never makes it cheaper
        }
        // ALU re-execution multiplies the beats exactly.
        const RunResult dwc = go(L, P(RfProtection::None, AluProtection::Dwc));
        const RunResult tmr = go(L, P(RfProtection::None, AluProtection::Tmr));
        CHECK(dwc.alu_beats == 2 * base.alu_beats && tmr.alu_beats == 3 * base.alu_beats);
    }
}

void test_forced_upset_classification() {
    // Flip bit 30 of sp (x2), live in every workload: the next stack access leaves DRAM.
    for (const char* n : kNames) {
        Loaded L(n);
        if (!L.g.ok) continue;
        const u64 at = 500;
        const RunResult none = go(L, P(RfProtection::None), {}, {}, flip_sp(at));
        CHECK_MSG(none.outcome == Outcome::HangCrash, std::string(n) + " unprotected: " + to_string(none.outcome));
        CHECK(none.first_divergence >= 0);

        const RunResult par = go(L, P(RfProtection::Parity), {}, {}, flip_sp(at));
        CHECK_MSG(par.outcome == Outcome::DetectedUnrecovered, std::string(n) + " parity: " + to_string(par.outcome));

        for (unsigned copy : {0u, 1u}) {
            const RunResult dup = go(L, P(RfProtection::ParityDup), {}, {}, flip_sp(at, copy));
            CHECK_MSG(dup.outcome == Outcome::DetectedRecovered, std::string(n) + " dup: " + to_string(dup.outcome));
            CHECK(dup.result == L.g.result && dup.first_divergence == -1);
        }
        for (unsigned copy : {0u, 1u, 2u}) {
            const RunResult tmr = go(L, P(RfProtection::Tmr), {}, {}, flip_sp(at, copy));
            CHECK_MSG(tmr.outcome == Outcome::DetectedRecovered, std::string(n) + " tmr: " + to_string(tmr.outcome));
            CHECK(tmr.result == L.g.result && tmr.first_divergence == -1);
        }
    }
}

void test_alu_upsets_are_masked_by_reexecution() {
    Loaded L("chacha20");
    if (!L.g.ok) return;
    FaultRates r; r.alu_per_bit_per_beat = 2e-5;
    int none_ok = 0, dwc_ok = 0, tmr_ok = 0, upsets_seen = 0;
    const int N = 40;
    for (int i = 0; i < N; ++i) {
        const RunResult a = go(L, P(RfProtection::None), r, {}, {}, 100 + i);
        const RunResult b = go(L, P(RfProtection::None, AluProtection::Dwc), r, {}, {}, 100 + i);
        const RunResult c = go(L, P(RfProtection::None, AluProtection::Tmr), r, {}, {}, 100 + i);
        none_ok += (a.outcome == Outcome::Correct);
        dwc_ok  += (b.outcome == Outcome::DetectedRecovered || b.outcome == Outcome::Correct);
        tmr_ok  += (c.outcome == Outcome::DetectedRecovered || c.outcome == Outcome::Correct);
        upsets_seen += (a.alu_upsets > 0);
    }
    CHECK(upsets_seen > N / 2);          // the rate really does upset most unprotected runs
    CHECK(none_ok < N / 4);              // ... and most of those go wrong
    CHECK(dwc_ok >= N - 3 && tmr_ok >= N - 3);
}

void test_run_is_deterministic() {
    Loaded L("coremark_like");
    if (!L.g.ok) return;
    FaultRates r{1e-9, 1e-6};
    const RunResult a = go(L, P(RfProtection::Tmr, AluProtection::Dwc, 32), r, {}, {}, 42);
    const RunResult b = go(L, P(RfProtection::Tmr, AluProtection::Dwc, 32), r, {}, {}, 42);
    CHECK(a.outcome == b.outcome && a.instret == b.instret && a.rf_upsets == b.rf_upsets &&
          a.alu_upsets == b.alu_upsets && a.model_cycles == b.model_cycles && a.result == b.result);
}

void test_scrubbing_costs_cycles() {
    Loaded L("chacha20");
    if (!L.g.ok) return;
    const RunResult off = go(L, P(RfProtection::Tmr, AluProtection::None, 0));
    const RunResult on  = go(L, P(RfProtection::Tmr, AluProtection::None, 50));
    CHECK(on.model_cycles > off.model_cycles);
    CHECK(on.model_cycles - off.model_cycles <= L.g.instret / 50 + 1);
}

void test_boot_failure_with_permanent_defects() {
    Loaded L("chacha20");
    if (!L.g.ok) return;
    ChipSpec none_spare; none_spare.defects = true; none_spare.rf_spares = 0; none_spare.alu_spares = 0;
    int boot = 0;
    for (int i = 0; i < 10; ++i)
        boot += go(L, P(RfProtection::None), {}, none_spare, {}, 10 + i).outcome == Outcome::BootFailure;
    CHECK(boot == 10);                    // 256 slices at p = 0.71 with no spares never repairs

    ChipSpec plenty; plenty.defects = true; plenty.rf_spares = 160; plenty.alu_spares = 24;
    int ok = 0;
    for (int i = 0; i < 10; ++i) {
        const RunResult r = go(L, P(RfProtection::None), {}, plenty, {}, 20 + i);
        ok += (r.outcome == Outcome::Correct && r.result == L.g.result);
    }
    CHECK(ok >= 9);                       // repaired chips run the workload exactly

    // TMR needs three repaired register files, so it fails to boot more often than one copy.
    ChipSpec tight; tight.defects = true; tight.rf_spares = 120; tight.alu_spares = 24;
    int boot1 = 0, boot3 = 0;
    for (int i = 0; i < 60; ++i) {
        boot1 += go(L, P(RfProtection::None), {}, tight, {}, 300 + i).outcome == Outcome::BootFailure;
        boot3 += go(L, P(RfProtection::Tmr), {}, tight, {}, 300 + i).outcome == Outcome::BootFailure;
    }
    CHECK(boot3 > boot1);
}

// Every run, whatever the faults, must satisfy the definitions of its outcome class.
void test_outcome_invariants_over_many_runs() {
    const Protection configs[] = {P(RfProtection::None), P(RfProtection::Parity),
                                  P(RfProtection::ParityDup, AluProtection::Dwc, 32),
                                  P(RfProtection::Tmr, AluProtection::Tmr, 32)};
    int sdc_unprotected = 0, seen[kNumOutcomes] = {};
    for (const char* n : kNames) {
        Loaded L(n);
        if (!L.g.ok) continue;
        FaultRates r{2e-9, 2e-6};
        for (const Protection& p : configs)
            for (int i = 0; i < 40; ++i) {
                const RunResult x = go(L, p, r, {}, {}, 1000 + i);
                ++seen[static_cast<int>(x.outcome)];
                const u64 corr = x.rf_corrected + x.alu_corrected;
                switch (x.outcome) {
                    case Outcome::Correct:
                        CHECK(x.result == L.g.result && corr == 0);
                        break;
                    case Outcome::DetectedRecovered:
                        CHECK(x.result == L.g.result && corr > 0);
                        break;
                    case Outcome::Sdc:
                        CHECK(x.result != L.g.result);
                        CHECK(x.first_divergence >= 0);        // lockstep saw it too
                        break;
                    case Outcome::HangCrash:
                        CHECK(x.hang || x.crash);              // one of the two sub-causes is recorded
                        break;
                    default: break;
                }
                if (p.rf == RfProtection::None && p.alu == AluProtection::None && x.outcome == Outcome::Sdc)
                    ++sdc_unprotected;
            }
    }
    CHECK(sdc_unprotected > 5);                                // SDC does occur without protection
    CHECK(seen[static_cast<int>(Outcome::Correct)] > 0);
    CHECK(seen[static_cast<int>(Outcome::DetectedRecovered)] > 0);
    CHECK(seen[static_cast<int>(Outcome::DetectedUnrecovered)] > 0);
    CHECK(seen[static_cast<int>(Outcome::HangCrash)] > 0);
}

// --- G15: control state, memory, bursts, in-field defects ---

ForcedFault hit(ForcedKind kind, u64 at, unsigned bit, unsigned burst = 1, u64 mem_offset = 0,
                unsigned copy = 0, u32 word = 2) {
    ForcedFault f;
    f.active = true; f.kind = kind; f.at_instret = at; f.bit = bit; f.burst = burst;
    f.mem_offset = mem_offset; f.copy = copy; f.word = word;
    return f;
}

void test_control_state_upsets() {
    for (const char* n : kNames) {
        Loaded L(n);
        if (!L.g.ok) continue;
        Protection tmr = P(RfProtection::None);
        tmr.control_tmr = true;

        const RunResult pc = go(L, P(RfProtection::None), {}, {}, hit(ForcedKind::Pc, 500, 40));
        CHECK_MSG(pc.outcome == Outcome::HangCrash && pc.pc_upsets == 1,
                  std::string(n) + " pc flip: " + to_string(pc.outcome));
        CHECK(pc.first_divergence >= 0);
        const RunResult pcm = go(L, tmr, {}, {}, hit(ForcedKind::Pc, 500, 40));
        CHECK_MSG(pcm.outcome == Outcome::DetectedRecovered && pcm.control_masked == 1 && pcm.result == L.g.result,
                  std::string(n) + " pc flip, voted: " + to_string(pcm.outcome));

        const RunResult ir = go(L, P(RfProtection::None), {}, {}, hit(ForcedKind::Ir, 500, 0));
        CHECK(ir.ir_upsets == 1);
        CHECK_MSG(ir.first_divergence >= 0, std::string(n) + " an instruction with bit 0 flipped changes execution");
        const RunResult irm = go(L, tmr, {}, {}, hit(ForcedKind::Ir, 500, 0));
        CHECK(irm.outcome == Outcome::DetectedRecovered && irm.result == L.g.result && irm.first_divergence == -1);
    }
}

void test_memory_upsets_and_ecc() {
    Loaded L("chacha20");
    if (!L.g.ok) return;
    const u64 at  = L.g.instret - 5;                        // after the results are written
    const u64 off = kResultBase - 0x80000000ull;            // the first result word
    Protection ecc = P(RfProtection::None);
    ecc.mem_ecc = true;
    const RunResult none = go(L, P(RfProtection::None), {}, {}, hit(ForcedKind::Mem, at, 0, 1, off));
    CHECK(none.mem_upsets == 1 && none.outcome == Outcome::Sdc && none.result != L.g.result);
    const RunResult one = go(L, ecc, {}, {}, hit(ForcedKind::Mem, at, 0, 1, off));
    CHECK_MSG(one.outcome == Outcome::DetectedRecovered && one.result == L.g.result, "SECDED corrects one flip");
    const RunResult two_plain = go(L, P(RfProtection::None), {}, {}, hit(ForcedKind::Mem, at, 0, 2, off));
    CHECK(two_plain.outcome == Outcome::Sdc);
    const RunResult two = go(L, ecc, {}, {}, hit(ForcedKind::Mem, at, 0, 2, off));
    CHECK_MSG(two.outcome == Outcome::DetectedUnrecovered, "SECDED detects a double flip and stops");
    const RunResult three = go(L, ecc, {}, {}, hit(ForcedKind::Mem, at, 0, 3, off));
    CHECK_MSG(three.outcome == Outcome::Sdc, "a triple flip escapes SECDED in this worst-case model");
}

void test_ecc_acts_only_when_the_word_is_read_or_scrubbed() {
    Loaded L("chacha20");
    if (!L.g.ok) return;
    const u64 unused = 0x200000;                                // far from code, data, stack and results
    Protection ecc = P(RfProtection::None);
    ecc.mem_ecc = true;
    Protection scrubbed = ecc;
    scrubbed.mem_scrub_period = 100;
    const ForcedFault one = hit(ForcedKind::Mem, 300, 5, 1, unused);
    const ForcedFault two = hit(ForcedKind::Mem, 300, 5, 2, unused);
    // a flip nobody reads does nothing, with or without ECC
    CHECK(go(L, P(RfProtection::None), {}, {}, two).outcome == Outcome::Correct);
    CHECK(go(L, ecc, {}, {}, one).outcome == Outcome::Correct);
    CHECK(go(L, ecc, {}, {}, two).outcome == Outcome::Correct);
    // the scrubber reads everything: it corrects the single and stops on the double
    const RunResult s1 = go(L, scrubbed, {}, {}, one);
    CHECK(s1.outcome == Outcome::DetectedRecovered && s1.mem_corrected == 1);
    CHECK(go(L, scrubbed, {}, {}, two).outcome == Outcome::DetectedUnrecovered);
    // a double flip in an instruction that is fetched later stops the machine when it is fetched
    const u64 next_fetch = (L.g.trace[299].pc_after - 0x80000000ull) & ~7ull;   // fetched by the very next instruction
    const RunResult code = go(L, ecc, {}, {}, hit(ForcedKind::Mem, 300, 0, 2, next_fetch));
    CHECK(code.outcome == Outcome::DetectedUnrecovered);
    CHECK(go(L, ecc, {}, {}, hit(ForcedKind::Mem, 300, 0, 1, next_fetch)).outcome == Outcome::DetectedRecovered);
}

void test_rates_for_new_classes_default_to_nothing() {
    Loaded L("coremark_like");
    if (!L.g.ok) return;
    const RunResult a = go(L, P(RfProtection::Tmr), FaultRates{1e-9, 1e-6}, {}, {}, 77);
    FaultRates r{1e-9, 1e-6};
    r.burst = 1;
    const RunResult b = go(L, P(RfProtection::Tmr), r, {}, {}, 77);
    CHECK(a.outcome == b.outcome && a.model_cycles == b.model_cycles && a.rf_upsets == b.rf_upsets);
    CHECK(a.pc_upsets + a.ir_upsets + a.csr_upsets + a.mem_upsets + a.infield_defects + a.retests == 0);
}

void test_infield_defect_and_online_retest() {
    for (const char* n : kNames) {
        Loaded L(n);
        if (!L.g.ok) continue;
        ChipSpec chip;
        chip.rf_spares = 8;
        const ForcedFault d = hit(ForcedKind::Defect, 500, 3, 1, 0, 0, 2 * 8);   // copy 0, sp byte 0, bit 3 stuck at 1
        const RunResult unprot = go(L, P(RfProtection::None), {}, chip, d);
        // (irq_loop's sp already has bit 3 set at that point, so stuck-at-1 changes nothing there)
        if (std::string(n) != "irq_loop")
            CHECK_MSG(unprot.outcome != Outcome::Correct && unprot.outcome != Outcome::DetectedRecovered,
                      std::string(n) + " unprotected with a stuck sp bit: " + to_string(unprot.outcome));
        CHECK(unprot.infield_defects == 1);

        Protection tmr = P(RfProtection::Tmr);
        const RunResult voted = go(L, tmr, {}, chip, d);
        CHECK(voted.result == L.g.result && voted.first_divergence == -1 && voted.outcome == Outcome::DetectedRecovered);

        tmr.retest_period = 100;
        const RunResult fixed = go(L, tmr, {}, chip, d);
        CHECK(fixed.result == L.g.result && fixed.retests > 0);
        CHECK(fixed.model_cycles >= voted.model_cycles + fixed.retests * kRetestCyclesPerSlice * 3 * (256 + 8));
    }
}

}  // namespace

int main() {
    test_control_state_upsets();
    test_memory_upsets_and_ecc();
    test_ecc_acts_only_when_the_word_is_read_or_scrubbed();
    test_rates_for_new_classes_default_to_nothing();
    test_infield_defect_and_online_retest();
    test_missing_workload_is_reported();
    test_golden_runs_and_known_outputs();
    test_zero_faults_every_configuration_is_exact();
    test_forced_upset_classification();
    test_alu_upsets_are_masked_by_reexecution();
    test_run_is_deterministic();
    test_scrubbing_costs_cycles();
    test_boot_failure_with_permanent_defects();
    test_outcome_invariants_over_many_runs();
    return tu::summary("experiment");
}
