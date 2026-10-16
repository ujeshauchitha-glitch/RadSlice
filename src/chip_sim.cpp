#include "radslice/chip_sim.hpp"

#include <cmath>

#include "radslice/defective_alu.hpp"
#include "radslice/selftest.hpp"

namespace radslice {

namespace {

constexpr AluOp kOps[] = {AluOp::Add, AluOp::Sub, AluOp::And, AluOp::Or, AluOp::Xor,
                          AluOp::Sll, AluOp::Srl, AluOp::Sra, AluOp::Slt, AluOp::Sltu,
                          AluOp::AddW, AluOp::SubW, AluOp::SllW, AluOp::SrlW, AluOp::SraW};

// A repaired chip must compute exactly what the plain ALU and an ideal register
// file would. Any difference here, after a passed self-test, is a test escape.
bool functionally_correct(SlicedRegFile& rf, DefectiveSlicedAlu& alu, Rng& rng) {
    for (int pass = 0; pass < 3; ++pass) {
        u64 want[32];
        for (u32 r = 0; r < 32; ++r) {
            want[r] = rng.next();
            rf.write(r, want[r]);
        }
        for (u32 r = 0; r < 32; ++r)
            if (rf.read(r) != want[r]) return false;
    }
    for (int i = 0; i < 300; ++i) {
        const u64 a = rng.next(), b = rng.next();
        const AluOp op = kOps[rng.below(15)];
        if (alu.exec(op, a, b) != alu_reference(op, a, b)) return false;
    }
    return true;
}

}  // namespace

void inject_regfile_defects(SlicedRegFile& rf, const ChipConfig& cfg, Rng& rng, ChipResult& res) {
    const double p_bad_rf = 1.0 - cfg.p;
    for (unsigned s = 0; s < rf.total_slices(); ++s) {
        if (rng.uniform() < p_bad_rf) {
            rf.set_defect(s, random_defect(rng, 8, false, cfg.activation));
            ++res.rf_defective_total;
            if (s < SlicedRegFile::kPrimarySlices) ++res.rf_defective_primary;
        }
    }
}

void inject_alu_defects(DefectiveSlicedAlu& alu, const ChipConfig& cfg, Rng& rng, ChipResult& res) {
    const double p_bad_alu = 1.0 - std::pow(cfg.p, cfg.k / 8.0);
    for (unsigned u = 0; u < alu.physical_units(); ++u) {
        if (rng.uniform() < p_bad_alu) {
            alu.set_defect(u, random_defect(rng, cfg.k, true, cfg.activation));
            ++res.alu_defective_total;
        }
    }
}

void inject_defects(SlicedRegFile& rf, DefectiveSlicedAlu& alu, const ChipConfig& cfg, Rng& rng,
                    ChipResult& res) {
    inject_regfile_defects(rf, cfg, rng, res);   // same draw order as before: register file first
    inject_alu_defects(alu, cfg, rng, res);
}

ChipResult simulate_chip(const ChipConfig& cfg, Rng& rng) {
    ChipResult res;
    SlicedRegFile      rf(cfg.rf_spares);
    DefectiveSlicedAlu alu(64, cfg.k, cfg.alu_spares, rng.next());
    rf.seed_defects(rng.next());
    inject_defects(rf, alu, cfg, rng, res);

    res.rf_ok  = regfile_self_test(rf, cfg.rounds).ok;
    res.alu_ok = alu_self_test(alu, cfg.rounds).ok;

    if (res.rf_ok && res.alu_ok) {
        // Exact, from the defect map: is any slice that the repaired chip actually
        // uses still defective? (A hard defect cannot be, after the self-test; a weak
        // one can slip through.)
        for (unsigned l = 0; l < SlicedRegFile::kPrimarySlices && !res.latent; ++l)
            res.latent = rf.defect(rf.remap(l)).present();
        for (unsigned l = 0; l < alu.num_slices() && !res.latent; ++l)
            res.latent = alu.defect(alu.mapping(l)).present();
    }

    if (cfg.verify && res.rf_ok && res.alu_ok) {
        res.verified = true;
        res.escape   = !functionally_correct(rf, alu, rng);
    }
    return res;
}

bool detect_weak_slice(double activation, unsigned rounds, Rng& rng) {
    SlicedRegFile rf(0);
    rf.seed_defects(rng.next());
    const unsigned victim = rng.below(SlicedRegFile::kPrimarySlices);
    rf.set_defect(victim, random_defect(rng, 8, false, activation));
    const RepairReport rep = regfile_self_test(rf, rounds);
    return rep.bad_primary >= 1;
}

}  // namespace radslice
