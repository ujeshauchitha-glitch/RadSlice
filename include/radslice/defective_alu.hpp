#pragma once

#include <vector>

#include "radslice/defects.hpp"
#include "radslice/sliced_alu.hpp"

namespace radslice {

// A SlicedAlu with permanent defects and spare slice units.
//
// There are `num_slices() + spare_units` physical slice units. A mapping assigns
// each logical slice position to a physical unit (the identity at first); the boot
// self-test rewrites it to route around defective units. As in the register file,
// any spare can serve any position (an ideal crossbar), which makes yields computed
// from this an upper bound.
class DefectiveSlicedAlu : public SlicedAlu {
public:
    DefectiveSlicedAlu(unsigned xlen, unsigned slice_bits, unsigned spare_units, u64 seed = 1);

    unsigned physical_units() const { return static_cast<unsigned>(defects_.size()); }
    void     set_defect(unsigned unit, const SliceDefect& d);
    const SliceDefect& defect(unsigned unit) const { return defects_.at(unit); }

    unsigned mapping(unsigned logical) const { return map_.at(logical); }
    void     set_mapping(unsigned logical, unsigned unit) { map_.at(logical) = unit; }

    // Run one slice operation on a specific physical unit, bypassing the mapping.
    // This is what the self-test uses to examine units one at a time.
    SliceOut unit_exec(unsigned unit, SliceOp op, u64 x, u64 y, u64 aux);

protected:
    SliceOut slice_unit(unsigned slice_index, SliceOp op, u64 x, u64 y, u64 aux) override;

private:
    SliceOut corrupt(unsigned unit, SliceOut out);

    std::vector<SliceDefect> defects_;
    std::vector<unsigned>    map_;   // logical slice -> physical unit
    bool                     any_defect_ = false;
    Rng                      rng_;
};

}  // namespace radslice
