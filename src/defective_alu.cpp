#include "radslice/defective_alu.hpp"

#include <algorithm>
#include <numeric>

namespace radslice {

DefectiveSlicedAlu::DefectiveSlicedAlu(unsigned xlen, unsigned slice_bits,
                                       unsigned spare_units, u64 seed, bool single_unit)
    : SlicedAlu(xlen, slice_bits), single_(single_unit), rng_(seed) {
    map_.resize(num_slices());
    if (single_) {
        defects_.assign(1 + spare_units, SliceDefect{});
        std::fill(map_.begin(), map_.end(), 0u);       // every position uses the one unit
    } else {
        defects_.assign(num_slices() + spare_units, SliceDefect{});
        std::iota(map_.begin(), map_.end(), 0u);
    }
}

void DefectiveSlicedAlu::set_defect(unsigned unit, const SliceDefect& d) {
    defects_.at(unit) = d;
    any_defect_ = any_defect_ || d.present();
}

SliceOut DefectiveSlicedAlu::corrupt(unsigned unit, SliceOut out) {
    if (!any_defect_) return out;
    const SliceDefect& d = defects_[unit];
    if (!defect_active(d, rng_)) return out;
    if (d.kind == DefectKind::StuckBit) out.value = force_bit(out.value, d.bit, d.value);
    else if (d.kind == DefectKind::StuckCarry) out.carry = d.value;
    return out;
}

SliceOut DefectiveSlicedAlu::slice_unit(unsigned slice_index, SliceOp op, u64 x, u64 y,
                                        u64 aux) {
    return corrupt(map_[slice_index], SlicedAlu::slice_unit(slice_index, op, x, y, aux));
}

SliceOut DefectiveSlicedAlu::unit_exec(unsigned unit, SliceOp op, u64 x, u64 y, u64 aux) {
    return corrupt(unit, SlicedAlu::slice_unit(unit, op, x, y, aux));
}

}  // namespace radslice
