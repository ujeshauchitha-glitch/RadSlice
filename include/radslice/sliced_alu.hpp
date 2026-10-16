#pragma once

#include <array>

#include "backends.hpp"  // riscv-emu: AluBackend, AluOp, u64

namespace radslice {

// ---------------------------------------------------------------------------
// A bit-sliced integer ALU.
//
// The datapath is `xlen / K` identical K-bit slice units used one after another.
// Every ALU operation is expressed as a sequence of slice-unit operations plus
// wiring (carry between slices, sign bits, fill values) and nothing else, so a
// fault in one slice unit can later be modelled by replacing that one function.
//
// Nothing here is a measurement. A "beat" is one slice-unit operation; how long
// a beat takes is a model parameter, not a property of any device.
//
// `xlen` is a parameter only so the logic can be checked exhaustively at small
// widths; the model of interest is xlen = 64 with K = 8, 16 or 32. `xlen` must be
// a power of two (shift amounts are taken modulo xlen and xlen/2), and K must
// divide xlen/2 so that the low word of a W-form is a whole number of slices.
// ---------------------------------------------------------------------------

// The operations a physical slice unit performs. Everything else is built from
// these.
enum class SliceOp {
    Add,      // x + y + carry_in, with carry out
    And, Or, Xor,
    FunnelL,  // (x << amt | y >> (K - amt)) & mask    left shift, one output slice
    FunnelR,  // (x >> amt | y << (K - amt)) & mask    right shift, one output slice
};

struct SliceOut {
    u64  value;
    bool carry;
};

struct AluStats {
    u64                 ops        = 0;   // ALU operations executed
    u64                 beats      = 0;   // model cost: one per slice-unit operation
    u64                 unit_calls = 0;   // slice-unit invocations actually made
    std::array<u64, 15> per_op{};         // indexed by AluOp
};

class SlicedAlu : public AluBackend {
public:
    SlicedAlu(unsigned xlen, unsigned slice_bits);

    u64 exec(AluOp op, u64 a, u64 b) override;

    // What a fault-free slice unit does. The self-test compares physical units
    // against this.
    SliceOut ideal_slice(SliceOp op, u64 x, u64 y, u64 aux) const;

    unsigned xlen() const { return xlen_; }
    unsigned slice_bits() const { return k_; }
    unsigned num_slices() const { return n_; }

    const AluStats& stats() const { return stats_; }
    void            reset_stats() { stats_ = AluStats{}; }

    // Model cost of one operation, in beats: the number of slices it streams
    // through. 64-bit operations stream all of them; W forms stream only the
    // slices of the low word, and the sign-extension into the upper slices is
    // done by the write-back path and costs no ALU beat (a tier-C assumption).
    static unsigned beats_for(AluOp op, unsigned xlen, unsigned slice_bits);

protected:
    // The single place a slice unit's behaviour is defined. Later milestones
    // override this to inject faults into a chosen slice index.
    virtual SliceOut slice_unit(unsigned slice_index, SliceOp op, u64 x, u64 y, u64 aux);

private:
    u64 slice(u64 v, unsigned i) const { return (v >> (i * k_)) & kmask_; }

    SliceOut add_chain(u64 a, u64 b, bool carry_in, unsigned slices);
    u64      logic_chain(SliceOp op, u64 a, u64 b, unsigned slices);
    u64      shift_chain(u64 x, unsigned amount, bool left, u64 fill, unsigned slices);
    u64      sign_extend_word(u64 word) const;

    unsigned xlen_, k_, n_, nw_;   // nw_ = slices in the low word (xlen / 2)
    u64      xmask_, kmask_, wmask_;
    AluStats stats_;
};

}  // namespace radslice
