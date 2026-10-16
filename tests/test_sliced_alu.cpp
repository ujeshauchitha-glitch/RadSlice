#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "radslice/sliced_alu.hpp"
#include "test_util.hpp"

// ---------------------------------------------------------------------------
// The sliced ALU against an independent reference.
//
// The reference below is written separately from both the sliced ALU and the
// emulator's alu_reference(), and works at any width, so the same logic can be
// checked exhaustively at 8 bits and densely at 64. At 64 bits it is also
// checked against the emulator's own ALU, which ties the test oracle to the
// golden model.
// ---------------------------------------------------------------------------

using radslice::SlicedAlu;

namespace {

constexpr AluOp kOps[] = {
    AluOp::Add, AluOp::Sub, AluOp::And, AluOp::Or, AluOp::Xor,
    AluOp::Sll, AluOp::Srl, AluOp::Sra, AluOp::Slt, AluOp::Sltu,
    AluOp::AddW, AluOp::SubW, AluOp::SllW, AluOp::SrlW, AluOp::SraW,
};

const char* name(AluOp op) {
    static const char* n[] = {"add", "sub", "and", "or", "xor", "sll", "srl", "sra",
                              "slt", "sltu", "addw", "subw", "sllw", "srlw", "sraw"};
    return n[static_cast<int>(op)];
}

u64 mask(unsigned bits) { return bits >= 64 ? ~0ull : (1ull << bits) - 1; }

// Interpret the low `bits` bits of v as a signed number.
i64 sx(u64 v, unsigned bits) {
    v &= mask(bits);
    return static_cast<i64>(((v >> (bits - 1)) & 1) ? (v | ~mask(bits)) : v);
}

u64 ref(AluOp op, u64 a, u64 b, unsigned X) {
    const unsigned W = X / 2;
    const u64 m = mask(X), wm = mask(W);
    a &= m;
    b &= m;
    auto widen = [&](u64 w) { return static_cast<u64>(sx(w, W)) & m; };  // sign-extend a word
    switch (op) {
        case AluOp::Add:  return (a + b) & m;
        case AluOp::Sub:  return (a - b) & m;
        case AluOp::And:  return a & b;
        case AluOp::Or:   return a | b;
        case AluOp::Xor:  return a ^ b;
        case AluOp::Sll:  return (a << (b & (X - 1))) & m;
        case AluOp::Srl:  return a >> (b & (X - 1));
        case AluOp::Sra:  return static_cast<u64>(sx(a, X) >> (b & (X - 1))) & m;
        case AluOp::Slt:  return sx(a, X) < sx(b, X) ? 1 : 0;
        case AluOp::Sltu: return a < b ? 1 : 0;
        case AluOp::AddW: return widen((a + b) & wm);
        case AluOp::SubW: return widen((a - b) & wm);
        case AluOp::SllW: return widen(((a & wm) << (b & (W - 1))) & wm);
        case AluOp::SrlW: return widen((a & wm) >> (b & (W - 1)));
        case AluOp::SraW: return widen(static_cast<u64>(sx(a, W) >> (b & (W - 1))) & wm);
    }
    return 0;
}

void check_one(SlicedAlu& alu, AluOp op, u64 a, u64 b, unsigned X, unsigned K) {
    const u64 got = alu.exec(op, a, b);
    const u64 want = ref(op, a, b, X);
    if (X == 64) {  // at full width, also compare directly with the golden ALU
        CHECK_MSG(got == alu_reference(op, a, b),
                  std::string("K=") + std::to_string(K) + " " + name(op) + " a=" + tu::hex(a) +
                      " b=" + tu::hex(b) + " differs from the golden ALU");
    }
    CHECK_MSG(got == want,
              std::string("X=") + std::to_string(X) + " K=" + std::to_string(K) + " " +
                  name(op) + " a=" + tu::hex(a) + " b=" + tu::hex(b) + " got=" +
                  tu::hex(got) + " want=" + tu::hex(want));
}

void check_beat_invariant(const SlicedAlu& alu, unsigned X, unsigned K) {
    // Every beat is exactly one slice-unit call, for every operation.
    CHECK(alu.stats().unit_calls == alu.stats().beats);
    u64 expect = 0;
    for (AluOp op : kOps)
        expect += alu.stats().per_op[static_cast<int>(op)] * SlicedAlu::beats_for(op, X, K);
    CHECK(alu.stats().beats == expect);
}

// ---- exhaustive at 8 bits -------------------------------------------------

void test_exhaustive_8bit() {
    for (unsigned K : {1u, 2u, 4u}) {
        SlicedAlu alu(8, K);
        for (unsigned a = 0; a < 256; ++a)
            for (unsigned b = 0; b < 256; ++b)
                for (AluOp op : kOps) check_one(alu, op, a, b, 8, K);
        check_beat_invariant(alu, 8, K);
    }
}

// ---- 16 bits: every a against a structured set of b -----------------------

void test_16bit_dense() {
    std::mt19937_64 rng(16);
    std::vector<u64> bs = {0, 1, 2, 7, 8, 15, 16, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000,
                           0x8001, 0xffff, 0xfffe, 0xff00, 0x00ff, 0x0f0f, 0xf0f0};
    while (bs.size() < 40) bs.push_back(rng() & 0xffff);
    for (unsigned K : {2u, 8u}) {
        SlicedAlu alu(16, K);
        for (unsigned a = 0; a < 65536; ++a)
            for (u64 b : bs)
                for (AluOp op : kOps) check_one(alu, op, a, b, 16, K);
        check_beat_invariant(alu, 16, K);
    }
}

// ---- 64 bits --------------------------------------------------------------

// Values that sit on every slice and word boundary, for every K at once.
std::vector<u64> boundary_pool() {
    std::vector<u64> v = {0, 1, 2, 3, ~0ull, ~0ull - 1, 0x7fff'ffff'ffff'ffffull,
                          0x8000'0000'0000'0000ull, 0x7fff'ffffull, 0x8000'0000ull,
                          0xffff'ffffull, 0x1'0000'0000ull, 0xffff'ffff'8000'0000ull,
                          0xffff'ffff'7fff'ffffull, 0x0123'4567'89ab'cdefull,
                          0xfedc'ba98'7654'3210ull};
    for (unsigned p = 0; p < 64; ++p) {
        const u64 bit = 1ull << p;
        v.push_back(bit);
        v.push_back(bit - 1);
        v.push_back(~bit);
        v.push_back(0 - bit);
    }
    return v;
}

void test_64bit_reference_matches_golden_alu() {
    // Ties the independent reference to the emulator's ALU.
    std::mt19937_64 rng(64);
    const std::vector<u64> pool = boundary_pool();
    for (int i = 0; i < 200000; ++i) {
        const u64 a = (i % 3 == 0) ? pool[rng() % pool.size()] : rng();
        const u64 b = (i % 3 == 1) ? pool[rng() % pool.size()] : rng();
        for (AluOp op : kOps)
            CHECK_MSG(ref(op, a, b, 64) == alu_reference(op, a, b),
                      std::string("test oracle disagrees with golden ALU: ") + name(op) +
                          " a=" + tu::hex(a) + " b=" + tu::hex(b));
    }
}

void test_64bit_directed_and_random() {
    const std::vector<u64> pool = boundary_pool();
    for (unsigned K : {8u, 16u, 32u}) {
        SlicedAlu alu(64, K);

        // Every pair from the boundary pool: carry and borrow across each slice
        // boundary, sign changes, word boundaries.
        for (u64 a : pool)
            for (u64 b : pool)
                for (AluOp op : kOps) check_one(alu, op, a, b, 64, K);

        // Every shift amount, with junk in the upper bits of the amount register
        // to prove only the low 6 (or 5) bits are used.
        std::mt19937_64 rng(1000 + K);
        for (u64 a : pool)
            for (unsigned s = 0; s < 64; ++s)
                for (AluOp op : {AluOp::Sll, AluOp::Srl, AluOp::Sra, AluOp::SllW,
                                 AluOp::SrlW, AluOp::SraW}) {
                    check_one(alu, op, a, s, 64, K);
                    check_one(alu, op, a, s | (rng() << 6), 64, K);
                }

        // Seeded random operands.
        for (int i = 0; i < 300000; ++i) {
            const u64 a = rng(), b = rng();
            for (AluOp op : kOps) check_one(alu, op, a, b, 64, K);
        }
        check_beat_invariant(alu, 64, K);
    }
}

// Specific carry-ripple and borrow-ripple cases the way a human would write them.
void test_ripple_cases() {
    for (unsigned K : {8u, 16u, 32u}) {
        SlicedAlu alu(64, K);
        // 0x00..ff ff + 1 carries through every slice below the top.
        for (unsigned j = 1; j < 64 / K; ++j) {
            const u64 a = (1ull << (j * K)) - 1;
            CHECK(alu.exec(AluOp::Add, a, 1) == a + 1);
            CHECK(alu.exec(AluOp::Sub, a + 1, 1) == a);       // borrow ripples back down
        }
        CHECK(alu.exec(AluOp::Add, ~0ull, 1) == 0);           // carry out of the top is dropped
        CHECK(alu.exec(AluOp::Sub, 0, 1) == ~0ull);
        // W forms: carry into bit 32 is dropped, and the result is sign-extended from bit 31.
        CHECK(alu.exec(AluOp::AddW, 0x7fff'ffffull, 1) == 0xffff'ffff'8000'0000ull);
        CHECK(alu.exec(AluOp::AddW, 0xffff'ffffull, 1) == 0);
        CHECK(alu.exec(AluOp::SubW, 0, 1) == ~0ull);
        CHECK(alu.exec(AluOp::SubW, 0x8000'0000ull, 1) == 0x7fff'ffffull);
        // slt vs sltu on the same bits.
        CHECK(alu.exec(AluOp::Slt, ~0ull, 0) == 1);
        CHECK(alu.exec(AluOp::Sltu, ~0ull, 0) == 0);
        CHECK(alu.exec(AluOp::Slt, 0x8000'0000'0000'0000ull, 0x7fff'ffff'ffff'ffffull) == 1);
        CHECK(alu.exec(AluOp::Sltu, 0x8000'0000'0000'0000ull, 0x7fff'ffff'ffff'ffffull) == 0);
        // Cross-slice shifts.
        CHECK(alu.exec(AluOp::Sll, 1, 63) == 0x8000'0000'0000'0000ull);
        CHECK(alu.exec(AluOp::Srl, 0x8000'0000'0000'0000ull, 63) == 1);
        CHECK(alu.exec(AluOp::Sra, 0x8000'0000'0000'0000ull, 63) == ~0ull);
        CHECK(alu.exec(AluOp::SraW, 0x8000'0000ull, 31) == ~0ull);
        CHECK(alu.exec(AluOp::SrlW, 0x8000'0000ull, 0) == 0xffff'ffff'8000'0000ull);
    }
}

void test_beat_counts() {
    // Overhead is exactly the number of slices streamed.
    CHECK(SlicedAlu::beats_for(AluOp::Add, 64, 8) == 8);
    CHECK(SlicedAlu::beats_for(AluOp::Add, 64, 16) == 4);
    CHECK(SlicedAlu::beats_for(AluOp::Add, 64, 32) == 2);
    CHECK(SlicedAlu::beats_for(AluOp::AddW, 64, 8) == 4);
    CHECK(SlicedAlu::beats_for(AluOp::SraW, 64, 16) == 2);
    CHECK(SlicedAlu::beats_for(AluOp::SllW, 64, 32) == 1);
    CHECK(SlicedAlu::beats_for(AluOp::Slt, 64, 32) == 2);
}

void test_bad_geometry_is_rejected() {
    auto throws = [](unsigned x, unsigned k) {
        try { SlicedAlu a(x, k); } catch (const std::invalid_argument&) { return true; }
        return false;
    };
    CHECK(throws(64, 0));
    CHECK(throws(64, 64));     // slice wider than 32
    CHECK(throws(64, 5));      // does not divide the word
    CHECK(throws(48, 8));      // xlen not a power of two
    CHECK(throws(128, 8));
    CHECK(!throws(64, 8));
    CHECK(!throws(64, 32));
}

}  // namespace

int main() {
    test_bad_geometry_is_rejected();
    test_beat_counts();
    test_ripple_cases();
    test_64bit_reference_matches_golden_alu();
    test_exhaustive_8bit();
    test_16bit_dense();
    test_64bit_directed_and_random();
    return tu::summary("sliced_alu");
}
