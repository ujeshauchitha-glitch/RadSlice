#pragma once

#include <cstdint>

#include "types.hpp"  // riscv-emu: u8, u32, u64

namespace radslice {

// A small deterministic generator (splitmix64). Used instead of <random>'s
// distributions so a seed gives the same stream on every compiler and platform.
class Rng {
public:
    explicit Rng(u64 seed = 1) : s_(seed) {}
    u64 next() {
        u64 z = (s_ += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    double   uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
    unsigned below(unsigned n) { return static_cast<unsigned>(next() % n); }

private:
    u64 s_;
};

// What a defective slice does. The model is deliberately the simplest fault a test
// can be wrong about: one output bit (or the carry) stuck at a value, active on
// every access (activation = 1, a hard defect) or on each access with probability
// `activation` (a weak, intermittent defect). All of this is tier C.
enum class DefectKind : u8 { None, StuckBit, StuckCarry };

struct SliceDefect {
    DefectKind kind       = DefectKind::None;
    unsigned   bit        = 0;      // which output bit (StuckBit)
    bool       value      = false;  // the stuck value
    double     activation = 1.0;    // probability the defect shows on an access
    bool present() const { return kind != DefectKind::None; }
};

// Draw a defect for a slice `slice_bits` wide. Carry defects only make sense for
// slices that produce a carry (ALU units), so they are opt-in.
inline SliceDefect random_defect(Rng& rng, unsigned slice_bits, bool allow_carry,
                                 double activation = 1.0) {
    SliceDefect d;
    d.activation = activation;
    d.value      = (rng.next() & 1) != 0;
    if (allow_carry && rng.below(slice_bits + 1) == slice_bits) {
        d.kind = DefectKind::StuckCarry;
    } else {
        d.kind = DefectKind::StuckBit;
        d.bit  = rng.below(slice_bits);
    }
    return d;
}

// True if the defect shows on this access.
inline bool defect_active(const SliceDefect& d, Rng& rng) {
    return d.present() && (d.activation >= 1.0 || rng.uniform() < d.activation);
}

inline u64 force_bit(u64 v, unsigned bit, bool value) {
    return value ? (v | (1ull << bit)) : (v & ~(1ull << bit));
}

}  // namespace radslice
