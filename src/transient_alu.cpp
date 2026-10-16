#include "radslice/transient_alu.hpp"

namespace radslice {

const char* to_string(AluProtection m) {
    switch (m) {
        case AluProtection::None: return "none";
        case AluProtection::Dwc:  return "dwc";
        case AluProtection::Tmr:  return "tmr";
    }
    return "?";
}

SliceOut TransientAlu::slice_unit(unsigned slice_index, SliceOp op, u64 x, u64 y, u64 aux) {
    SliceOut out = DefectiveSlicedAlu::slice_unit(slice_index, op, x, y, aux);
    const u64 call = calls_++;
    const unsigned k = slice_bits();

    int which = -1;
    if (armed_ && call == armed_call_) {
        which  = static_cast<int>(armed_which_);
        armed_ = false;
    } else if (rate_ > 0.0) {
        double p = (k + 1) * rate_;
        if (p > 1.0) p = 1.0;
        if (rng_.uniform() < p) which = static_cast<int>(rng_.below(k + 1));
    }
    if (which >= 0) {
        ++upsets_;
        if (static_cast<unsigned>(which) == k) out.carry = !out.carry;
        else out.value ^= (1ull << which);
    }
    return out;
}

u64 RedundantAlu::exec(AluOp op, u64 a, u64 b) {
    ++stats_.ops;
    switch (mode_) {
        case AluProtection::None:
            ++stats_.executions;
            return inner_.exec(op, a, b);
        case AluProtection::Dwc: {
            const u64 r1 = inner_.exec(op, a, b);
            const u64 r2 = inner_.exec(op, a, b);
            stats_.executions += 2;
            if (r1 == r2) return r1;
            ++stats_.detected;
            const u64 r3 = inner_.exec(op, a, b);
            ++stats_.executions;
            ++stats_.corrected;
            return majority(r1, r2, r3);
        }
        case AluProtection::Tmr: {
            const u64 r1 = inner_.exec(op, a, b);
            const u64 r2 = inner_.exec(op, a, b);
            const u64 r3 = inner_.exec(op, a, b);
            stats_.executions += 3;
            if (r1 == r2 && r2 == r3) return r1;
            ++stats_.detected;
            ++stats_.corrected;
            return majority(r1, r2, r3);
        }
    }
    return 0;
}

}  // namespace radslice
