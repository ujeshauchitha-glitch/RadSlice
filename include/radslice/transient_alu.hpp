#pragma once

#include "radslice/defective_alu.hpp"

namespace radslice {

// ---------------------------------------------------------------------------
// Transient upsets in the ALU, and time-redundant protection against them (M4).
//
// An upset flips one output bit (or the carry) of one slice-unit operation, once. It
// does not persist, so re-executing the operation draws a fresh, independent outcome.
// Permanent defects (M3) still apply underneath, via the DefectiveSlicedAlu base.
//
// The rate is per output bit per beat (tier C, swept, never quoted as a measurement).
// With K data bits and the carry, an operation is upset with probability (K+1) * rate
// (capped at 1), and the flipped bit is chosen uniformly.
// ---------------------------------------------------------------------------

class TransientAlu : public DefectiveSlicedAlu {
public:
    TransientAlu(unsigned xlen, unsigned slice_bits, unsigned spare_units, u64 seed = 1)
        : DefectiveSlicedAlu(xlen, slice_bits, spare_units, seed), rng_(seed ^ 0x5eedull) {}

    void set_rate(double per_bit_per_beat) { rate_ = per_bit_per_beat; }
    double rate() const { return rate_; }
    u64  upsets() const { return upsets_; }
    u64  slice_calls() const { return calls_; }

    // Deterministic injection for tests: upset the output of the slice-unit call with
    // this index (counted from the last reset), flipping output bit `which`
    // (0..K-1 = data bit, K = carry).
    void arm_upset(u64 call_index, unsigned which) {
        armed_ = true; armed_call_ = call_index; armed_which_ = which;
    }
    void reset_call_counter() { calls_ = 0; }

protected:
    SliceOut slice_unit(unsigned slice_index, SliceOp op, u64 x, u64 y, u64 aux) override;

private:
    double   rate_ = 0.0;
    u64      upsets_ = 0, calls_ = 0;
    bool     armed_ = false;
    u64      armed_call_ = 0;
    unsigned armed_which_ = 0;
    Rng      rng_;
};

enum class AluProtection { None, Dwc, Tmr };

const char* to_string(AluProtection m);

struct AluProtStats {
    u64 ops = 0;          // architectural operations requested
    u64 executions = 0;   // times the inner ALU actually ran (the beat multiplier)
    u64 detected = 0;     // operations whose redundant runs disagreed
    u64 corrected = 0;    // ... and were resolved by the vote
};

// A time-redundant wrapper. Dwc runs each operation twice and, on a mismatch, a third
// time and takes the bitwise majority; Tmr always runs three times and votes. Each
// extra execution costs a full set of beats, which is the price. The compare and vote
// logic are assumed fault-free.
class RedundantAlu : public AluBackend {
public:
    RedundantAlu(TransientAlu& inner, AluProtection mode) : inner_(inner), mode_(mode) {}

    u64 exec(AluOp op, u64 a, u64 b) override;

    AluProtection mode() const { return mode_; }
    const AluProtStats& stats() const { return stats_; }

private:
    static u64 majority(u64 a, u64 b, u64 c) { return (a & b) | (a & c) | (b & c); }

    TransientAlu& inner_;
    AluProtection mode_;
    AluProtStats  stats_;
};

}  // namespace radslice
