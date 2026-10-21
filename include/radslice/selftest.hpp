#pragma once

#include "radslice/defective_alu.hpp"
#include "radslice/sliced_regfile.hpp"

namespace radslice {

struct RepairReport {
    unsigned bad_primary = 0;   // defective slices among the ones the chip needs
    unsigned bad_spare   = 0;   // defective spares (unusable)
    unsigned remapped    = 0;   // primaries successfully moved onto good spares
    bool     ok          = true;  // false: not enough good slices, the chip is a reject
};

// Boot self-test and repair of the register file. Every physical slice (primary and
// spare) has patterns 0x00, 0xFF, 0xAA, 0x55 written and read back `rounds` times
// through the physical path; a slice that ever reads back wrong is bad. Each bad
// primary is remapped to the next good spare. Leaves the register file zeroed.
//
// Assumes the remap table and the test logic itself are fault-free.
//
// `domains` > 1 restricts each spare to one region: the primaries are split into `domains` equal
// regions and the spares into `domains` equal groups, and a bad primary can only be remapped to a
// good spare of its own region. domains = 1 is the ideal shared pool. The spare count must divide
// evenly, and 256 must be divisible by `domains`.
RepairReport regfile_self_test(SlicedRegFile& rf, unsigned rounds, unsigned domains = 1);

// Same for the ALU: every physical slice unit runs a fixed set of add, logic and
// funnel-shift vectors `rounds` times and is compared with the ideal slice. The
// logical positions are then assigned to the good units in order.
RepairReport alu_self_test(DefectiveSlicedAlu& alu, unsigned rounds);

}  // namespace radslice
