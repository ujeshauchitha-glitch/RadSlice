#include "radslice/sliced_alu.hpp"

#include <stdexcept>

namespace radslice {

namespace {

constexpr u64 low_mask(unsigned bits) {
    return bits >= 64 ? ~0ull : ((1ull << bits) - 1);
}

bool is_w_form(AluOp op) {
    switch (op) {
        case AluOp::AddW: case AluOp::SubW: case AluOp::SllW:
        case AluOp::SrlW: case AluOp::SraW:
            return true;
        default:
            return false;
    }
}

}  // namespace

SlicedAlu::SlicedAlu(unsigned xlen, unsigned slice_bits) : xlen_(xlen), k_(slice_bits) {
    const bool pow2 = xlen != 0 && (xlen & (xlen - 1)) == 0;
    if (!pow2 || xlen > 64 || xlen < 2)
        throw std::invalid_argument("xlen must be a power of two in [2, 64]");
    if (k_ == 0 || k_ > 32 || (xlen / 2) % k_ != 0)
        throw std::invalid_argument("slice width must be in [1, 32] and divide xlen/2");
    n_     = xlen_ / k_;
    nw_    = (xlen_ / 2) / k_;
    xmask_ = low_mask(xlen_);
    kmask_ = low_mask(k_);
    wmask_ = low_mask(xlen_ / 2);
}

unsigned SlicedAlu::beats_for(AluOp op, unsigned xlen, unsigned slice_bits) {
    return (is_w_form(op) ? xlen / 2 : xlen) / slice_bits;
}

SliceOut SlicedAlu::slice_unit(unsigned /*slice_index*/, SliceOp op, u64 x, u64 y, u64 aux) {
    return ideal_slice(op, x, y, aux);
}

SliceOut SlicedAlu::ideal_slice(SliceOp op, u64 x, u64 y, u64 aux) const {
    switch (op) {
        case SliceOp::Add: {
            const u64 s = x + y + aux;
            return {s & kmask_, ((s >> k_) & 1) != 0};
        }
        case SliceOp::And: return {x & y, false};
        case SliceOp::Or:  return {x | y, false};
        case SliceOp::Xor: return {x ^ y, false};
        case SliceOp::FunnelL:
            return {aux == 0 ? x : (((x << aux) | (y >> (k_ - aux))) & kmask_), false};
        case SliceOp::FunnelR:
            return {aux == 0 ? x : (((x >> aux) | (y << (k_ - aux))) & kmask_), false};
    }
    return {0, false};
}

// ----- chains ---------------------------------------------------------------

// Every slice-unit call goes through here so the call count is independent of
// whatever a derived class does inside slice_unit().
#define UNIT(i, op, x, y, aux) (++stats_.unit_calls, slice_unit((i), (op), (x), (y), (aux)))

SliceOut SlicedAlu::add_chain(u64 a, u64 b, bool carry_in, unsigned slices) {
    u64  r = 0;
    bool c = carry_in;
    for (unsigned i = 0; i < slices; ++i) {
        const SliceOut o = UNIT(i, SliceOp::Add, slice(a, i), slice(b, i), c ? 1 : 0);
        r |= o.value << (i * k_);
        c = o.carry;
    }
    return {r, c};
}

u64 SlicedAlu::logic_chain(SliceOp op, u64 a, u64 b, unsigned slices) {
    u64 r = 0;
    for (unsigned i = 0; i < slices; ++i)
        r |= UNIT(i, op, slice(a, i), slice(b, i), 0).value << (i * k_);
    return r;
}

// Shift `slices` slices of x by `amount` bits (amount < slices * K). Output slice
// i is built from at most two input slices: the one `q` slices away and its
// neighbour, where q = amount / K and the residual r = amount % K is handled
// inside the slice unit. Slices that would come from outside x take `fill`.
u64 SlicedAlu::shift_chain(u64 x, unsigned amount, bool left, u64 fill, unsigned slices) {
    const unsigned q = amount / k_;
    const unsigned r = amount % k_;
    u64 out = 0;
    for (unsigned i = 0; i < slices; ++i) {
        SliceOut o;
        if (left) {
            const int hi = static_cast<int>(i) - static_cast<int>(q);
            const int lo = hi - 1;
            const u64 xs = hi >= 0 ? slice(x, static_cast<unsigned>(hi)) : 0;
            const u64 ys = lo >= 0 ? slice(x, static_cast<unsigned>(lo)) : 0;
            o = UNIT(i, SliceOp::FunnelL, xs, ys, r);
        } else {
            const unsigned hi = i + q;
            const unsigned lo = i + q + 1;
            const u64 xs = hi < slices ? slice(x, hi) : fill;
            const u64 ys = lo < slices ? slice(x, lo) : fill;
            o = UNIT(i, SliceOp::FunnelR, xs, ys, r);
        }
        out |= o.value << (i * k_);
    }
    return out;
}

// The write-back path replicates bit (xlen/2 - 1), the top bit of the slice that
// holds it, across the upper slices.
u64 SlicedAlu::sign_extend_word(u64 word) const {
    const unsigned sign_bit = xlen_ / 2 - 1;
    return ((word >> sign_bit) & 1) ? (word | (xmask_ & ~wmask_)) : word;
}

// ----- the ALU --------------------------------------------------------------

u64 SlicedAlu::exec(AluOp op, u64 a, u64 b) {
    a &= xmask_;
    b &= xmask_;
    ++stats_.ops;
    stats_.beats += beats_for(op, xlen_, k_);
    ++stats_.per_op[static_cast<std::size_t>(op)];

    const unsigned wbits  = xlen_ / 2;
    const u64      sa     = (a >> (xlen_ - 1)) & 1;
    const u64      sb     = (b >> (xlen_ - 1)) & 1;
    const u64      wsign  = (a >> (wbits - 1)) & 1;
    const u64      alow   = a & wmask_;
    const u64      blow   = b & wmask_;
    const unsigned shamt  = static_cast<unsigned>(b & (xlen_ - 1));
    const unsigned wshamt = static_cast<unsigned>(b & (wbits - 1));

    switch (op) {
        case AluOp::Add:  return add_chain(a, b, false, n_).value;
        // a - b = a + ~b + 1; the carry chain starts at 1 instead of needing a
        // separate borrow path.
        case AluOp::Sub:  return add_chain(a, ~b & xmask_, true, n_).value;
        case AluOp::And:  return logic_chain(SliceOp::And, a, b, n_);
        case AluOp::Or:   return logic_chain(SliceOp::Or, a, b, n_);
        case AluOp::Xor:  return logic_chain(SliceOp::Xor, a, b, n_);

        case AluOp::Sll:  return shift_chain(a, shamt, true, 0, n_);
        case AluOp::Srl:  return shift_chain(a, shamt, false, 0, n_);
        case AluOp::Sra:  return shift_chain(a, shamt, false, sa ? kmask_ : 0, n_);

        // a < b unsigned  <=>  no carry out of a + ~b + 1.
        case AluOp::Sltu: return add_chain(a, ~b & xmask_, true, n_).carry ? 0 : 1;
        // Signed: if the signs differ the negative one is smaller, otherwise the
        // unsigned comparison of the two (same-sign) values is the answer. The
        // subtract chain runs either way, as the hardware would.
        case AluOp::Slt: {
            const bool ltu = !add_chain(a, ~b & xmask_, true, n_).carry;
            return (sa != sb) ? sa : (ltu ? 1 : 0);
        }

        case AluOp::AddW: return sign_extend_word(add_chain(alow, blow, false, nw_).value);
        case AluOp::SubW:
            return sign_extend_word(add_chain(alow, ~blow & wmask_, true, nw_).value);
        case AluOp::SllW: return sign_extend_word(shift_chain(alow, wshamt, true, 0, nw_));
        case AluOp::SrlW: return sign_extend_word(shift_chain(alow, wshamt, false, 0, nw_));
        case AluOp::SraW:
            return sign_extend_word(shift_chain(alow, wshamt, false, wsign ? kmask_ : 0, nw_));
    }
    return 0;
}

#undef UNIT

}  // namespace radslice
