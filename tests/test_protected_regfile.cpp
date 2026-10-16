#include <array>
#include <string>

#include "radslice/protected_regfile.hpp"
#include "radslice/selftest.hpp"
#include "test_util.hpp"

// ---------------------------------------------------------------------------
// Register-file protection schemes against single and multiple upsets.
//
// The property tests enumerate every possible single upset (every exposed bit of
// every copy) and check what each scheme does with it; the directed tests pin down
// the behaviours that are easy to get wrong: where parity can and cannot see an
// upset, what the voter does with two bad copies, and what scrubbing buys.
// ---------------------------------------------------------------------------

using namespace radslice;

namespace {

constexpr RfProtection kModes[] = {RfProtection::None, RfProtection::Parity,
                                   RfProtection::ParityDup, RfProtection::Tmr};

std::array<u64, 32> fill(ProtectedRegFile& rf, u64 seed) {
    Rng rng(seed);
    std::array<u64, 32> truth{};
    for (u32 w = 1; w < 32; ++w) {
        truth[w] = rng.next();
        rf.write(w, truth[w]);
    }
    return truth;
}

void test_clean_roundtrip_all_modes() {
    for (RfProtection m : kModes) {
        ProtectedRegFile rf(m);
        const auto truth = fill(rf, 1);
        for (u32 w = 1; w < 32; ++w) CHECK(rf.read(w) == truth[w]);
        CHECK(rf.stats().detected == 0 && !rf.fail_stop());
    }
}

void test_exposed_bits() {
    CHECK(ProtectedRegFile(RfProtection::None).exposed_bits() == 31 * 64);
    CHECK(ProtectedRegFile(RfProtection::Parity).exposed_bits() == 31 * 65);
    CHECK(ProtectedRegFile(RfProtection::ParityDup).exposed_bits() == 2 * 31 * 65);
    CHECK(ProtectedRegFile(RfProtection::Tmr).exposed_bits() == 3 * 31 * 64);
}

void test_peek_has_no_side_effects() {
    ProtectedRegFile rf(RfProtection::Tmr);
    const auto truth = fill(rf, 2);
    rf.flip_data(0, 5, 9);
    const RfProtStats before = rf.stats();
    CHECK(rf.peek(5) == truth[5]);
    CHECK(rf.stats().reads == before.reads && rf.stats().detected == 0 && rf.stats().writes == before.writes);
    // And the upset is still there: peek did not repair it.
    CHECK(rf.copy(0).peek(5) != truth[5]);
}

// ---- every single upset, one at a time --------------------------------------

void test_every_single_upset_none_is_silent() {
    ProtectedRegFile probe(RfProtection::None);
    for (u64 i = 0; i < probe.exposed_bits(); ++i) {
        ProtectedRegFile rf(RfProtection::None);
        const auto truth = fill(rf, 3);
        rf.inject(i);
        const u32 word = static_cast<u32>(1 + i / 64);
        CHECK(rf.read(word) == (truth[word] ^ (1ull << (i % 64))));  // returned wrong, silently
        CHECK(!rf.fail_stop() && rf.stats().detected == 0);
    }
}

void test_every_single_upset_parity_is_detected() {
    ProtectedRegFile probe(RfProtection::Parity);
    for (u64 i = 0; i < probe.exposed_bits(); ++i) {
        ProtectedRegFile rf(RfProtection::Parity);
        fill(rf, 4);
        rf.inject(i);
        const u32 word = static_cast<u32>(1 + i / 65);
        (void)rf.read(word);
        CHECK(rf.fail_stop() && rf.stats().detected == 1);   // seen, not corrected
    }
}

void test_every_single_upset_is_corrected_by_dup_and_tmr() {
    for (RfProtection m : {RfProtection::ParityDup, RfProtection::Tmr}) {
        ProtectedRegFile probe(m);
        for (u64 i = 0; i < probe.exposed_bits(); ++i) {
            ProtectedRegFile rf(m);
            const auto truth = fill(rf, 5);
            rf.inject(i);
            for (u32 w = 1; w < 32; ++w) CHECK(rf.read(w) == truth[w]);
            CHECK(!rf.fail_stop());
            // And the repair stuck: a second pass finds nothing left to fix.
            const u64 fixed = rf.stats().corrected;
            for (u32 w = 1; w < 32; ++w) (void)rf.read(w);
            CHECK(rf.stats().corrected == fixed);
        }
    }
}

// ---- directed multi-upset cases --------------------------------------------

void test_dup_two_bad_copies_fail_stop() {
    ProtectedRegFile rf(RfProtection::ParityDup);
    fill(rf, 6);
    rf.flip_data(0, 7, 3);
    rf.flip_data(1, 7, 40);
    (void)rf.read(7);
    CHECK(rf.fail_stop() && rf.stats().uncorrectable == 1);
}

void test_dup_even_weight_upset_is_caught_only_by_the_compare() {
    ProtectedRegFile rf(RfProtection::ParityDup);
    const auto truth = fill(rf, 7);
    rf.flip_data(0, 9, 1);
    rf.flip_data(0, 9, 2);                                  // two flips: parity still matches
    CHECK(rf.copy(0).parity_ok(9));
    (void)rf.read(9);
    CHECK(rf.fail_stop());                                  // copies differ, cannot tell which is right
    (void)truth;
}

void test_parity_alone_misses_even_weight_upsets() {
    ProtectedRegFile rf(RfProtection::Parity);
    const auto truth = fill(rf, 8);
    rf.flip_data(0, 4, 10);
    rf.flip_data(0, 4, 20);
    CHECK(rf.read(4) != truth[4]);       // wrong data ...
    CHECK(!rf.fail_stop());              // ... and not noticed: silent corruption
}

void test_tmr_masks_one_bad_copy_but_not_two_agreeing_ones() {
    ProtectedRegFile rf(RfProtection::Tmr);
    auto truth = fill(rf, 9);
    rf.flip_data(0, 6, 12);
    rf.flip_data(1, 6, 50);                                 // different bits in different copies
    CHECK(rf.read(6) == truth[6]);                          // majority is right, per bit
    // The same bit upset in two copies out-votes the good one: silent corruption.
    rf.write(8, truth[8]);
    rf.flip_data(0, 8, 5);
    rf.flip_data(1, 8, 5);
    CHECK(rf.read(8) == (truth[8] ^ (1ull << 5)));
    CHECK(!rf.fail_stop());
}

void test_scrubbing_stops_accumulation() {
    const u32 w = 11;
    // Without scrubbing, two upsets in different copies at the same bit defeat TMR.
    {
        ProtectedRegFile rf(RfProtection::Tmr);
        const auto truth = fill(rf, 10);
        rf.flip_data(0, w, 7);
        rf.flip_data(1, w, 7);
        CHECK(rf.read(w) != truth[w]);
    }
    // With a scrub pass between them, the first upset is already repaired.
    {
        ProtectedRegFile rf(RfProtection::Tmr);
        const auto truth = fill(rf, 10);
        rf.flip_data(0, w, 7);
        for (int i = 0; i < 31; ++i) rf.scrub_one();        // one full pass
        CHECK(rf.stats().scrub_corrections == 1);
        rf.flip_data(1, w, 7);
        CHECK(rf.read(w) == truth[w]);
    }
}

void test_scrubbing_parity_only_halts_on_an_unread_register() {
    ProtectedRegFile rf(RfProtection::Parity);
    fill(rf, 11);
    rf.flip_data(0, 20, 0);                                 // a register nobody will ever read
    CHECK(!rf.fail_stop());
    for (int i = 0; i < 31; ++i) rf.scrub_one();
    CHECK(rf.fail_stop());                                  // the scrubber turns a harmless upset into a halt
}

void test_scrub_none_is_a_no_op() {
    ProtectedRegFile rf(RfProtection::None);
    fill(rf, 12);
    for (int i = 0; i < 40; ++i) rf.scrub_one();
    CHECK(rf.stats().scrubbed_words == 0);
}

void test_composes_with_permanent_defects() {
    // A stuck slice in one TMR copy is repaired by that copy's own spare, then an
    // upset elsewhere is still voted out.
    ProtectedRegFile rf(RfProtection::Tmr, 4);
    SliceDefect d; d.kind = DefectKind::StuckBit; d.bit = 3; d.value = true;
    rf.copy(1).set_defect(5 * 8 + 2, d);
    CHECK(regfile_self_test(rf.copy(1), 1).ok);
    const auto truth = fill(rf, 13);
    rf.flip_data(2, 5, 17);
    for (u32 w = 1; w < 32; ++w) CHECK(rf.read(w) == truth[w]);
}

}  // namespace

int main() {
    test_clean_roundtrip_all_modes();
    test_exposed_bits();
    test_peek_has_no_side_effects();
    test_every_single_upset_none_is_silent();
    test_every_single_upset_parity_is_detected();
    test_every_single_upset_is_corrected_by_dup_and_tmr();
    test_dup_two_bad_copies_fail_stop();
    test_dup_even_weight_upset_is_caught_only_by_the_compare();
    test_parity_alone_misses_even_weight_upsets();
    test_tmr_masks_one_bad_copy_but_not_two_agreeing_ones();
    test_scrubbing_stops_accumulation();
    test_scrubbing_parity_only_halts_on_an_unread_register();
    test_scrub_none_is_a_no_op();
    test_composes_with_permanent_defects();
    return tu::summary("protected_regfile");
}
