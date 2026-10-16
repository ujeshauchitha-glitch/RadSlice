#pragma once

#include "radslice/defective_alu.hpp"
#include "radslice/defects.hpp"
#include "radslice/sliced_regfile.hpp"

namespace radslice {

struct ChipConfig {
    double   p          = 0.71;   // probability an 8-bit slice is good (tier C)
    unsigned k          = 16;     // ALU slice width
    unsigned rf_spares  = 0;      // spare byte slices in the register file
    unsigned alu_spares = 0;      // spare slice units in the ALU
    unsigned rounds     = 1;      // self-test repetitions
    double   activation = 1.0;    // 1.0 = hard defects; < 1 = intermittent (weak)
    bool     verify     = false;  // after repair, check the chip computes correctly
};

struct ChipResult {
    bool     rf_ok = false, alu_ok = false;   // repaired by the self-test and remap
    bool     verified = false;                // a functional check was run
    bool     escape = false;                  // passed the self-test, then computed wrongly
    bool     latent = false;                  // passed, but a defective slice is still in use
    unsigned rf_defective_primary = 0;        // before repair
    unsigned rf_defective_total   = 0;
    unsigned alu_defective_total  = 0;
};

// The two halves of inject_defects, for chips with several register-file copies.
void inject_regfile_defects(SlicedRegFile& rf, const ChipConfig& cfg, Rng& rng, ChipResult& res);
void inject_alu_defects(DefectiveSlicedAlu& alu, const ChipConfig& cfg, Rng& rng, ChipResult& res);

// Sample defects into a register file and an ALU already built with their spares.
// Fills the defect counts of `res`.
void inject_defects(SlicedRegFile& rf, DefectiveSlicedAlu& alu, const ChipConfig& cfg, Rng& rng,
                    ChipResult& res);

// Sample one chip's defects, run the real boot self-tests and remaps on the real
// register-file and ALU models, and report whether it was repaired.
//
// An 8-bit register slice is good with probability p. An ALU slice of K bits has the
// same defect density per bit: good with probability p^(K/8).
ChipResult simulate_chip(const ChipConfig& cfg, Rng& rng);

// One weak-defect slice in an otherwise good register file: does a self-test of
// `rounds` rounds find it?
bool detect_weak_slice(double activation, unsigned rounds, Rng& rng);

}  // namespace radslice
