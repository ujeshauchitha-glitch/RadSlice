#include "radslice/chip_sim.hpp"

#include <cmath>
#include <vector>

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

// lam such that E[exp(-lam * m)] = p for m ~ Gamma(alpha, mean 1): the marginal slice yield is p.
double cluster_lambda(double p, double alpha) { return alpha * (std::pow(p, -1.0 / alpha) - 1.0); }

double draw_multiplier(Rng& rng, double alpha) { return rng.gamma(alpha) / alpha; }

// Where a physical register-file slice sits in the layout that defines the cluster blocks.
// Primaries come first and spares after, unless the spares are split into domains, in which
// case each domain is laid out contiguously as its primaries then its own spares.
unsigned layout_position(unsigned phys, unsigned spares, unsigned domains) {
    const unsigned primaries = SlicedRegFile::kPrimarySlices;
    if (domains <= 1 || spares == 0) return phys;
    const unsigned d = primaries / domains, s = spares / domains;
    if (phys < primaries) return (phys / d) * (d + s) + (phys % d);
    const unsigned j = phys - primaries;
    return (j / s) * (d + s) + d + (j % s);
}

}  // namespace

void inject_regfile_defects(SlicedRegFile& rf, const ChipConfig& cfg, Rng& rng, ChipResult& res,
                            double die_multiplier) {
    if (cfg.cluster_alpha > 0.0) {
        const double lam = cluster_lambda(cfg.p, cfg.cluster_alpha);
        const unsigned spares = rf.total_slices() - SlicedRegFile::kPrimarySlices;
        std::vector<double> mult;                       // one per block, drawn the first time it is needed
        for (unsigned s = 0; s < rf.total_slices(); ++s) {
            double m = die_multiplier;
            if (m < 0.0) {
                const unsigned pos = layout_position(s, spares, cfg.spare_domains);
                const unsigned b = cfg.cluster_block ? pos / cfg.cluster_block : 0;
                while (mult.size() <= b) mult.push_back(draw_multiplier(rng, cfg.cluster_alpha));
                m = mult[b];
            }
            if (rng.uniform() < -std::expm1(-lam * m)) {
                rf.set_defect(s, random_defect(rng, 8, false, cfg.activation));
                ++res.rf_defective_total;
                if (s < SlicedRegFile::kPrimarySlices) ++res.rf_defective_primary;
            }
        }
        return;
    }
    const double p_bad_rf = 1.0 - cfg.p;
    for (unsigned s = 0; s < rf.total_slices(); ++s) {
        if (rng.uniform() < p_bad_rf) {
            rf.set_defect(s, random_defect(rng, 8, false, cfg.activation));
            ++res.rf_defective_total;
            if (s < SlicedRegFile::kPrimarySlices) ++res.rf_defective_primary;
        }
    }
}

void inject_alu_defects(DefectiveSlicedAlu& alu, const ChipConfig& cfg, Rng& rng, ChipResult& res,
                        double die_multiplier) {
    if (cfg.cluster_alpha > 0.0) {
        // An ALU unit of K bits is K/8 byte-slices of silicon, so its hazard is lam * (K/8) * m.
        const double lam   = cluster_lambda(cfg.p, cfg.cluster_alpha);
        const double kappa = cfg.k / 8.0;
        std::vector<double> mult;
        for (unsigned u = 0; u < alu.physical_units(); ++u) {
            double m = die_multiplier;
            if (m < 0.0) {
                const unsigned b = cfg.alu_cluster_block ? u / cfg.alu_cluster_block : 0;
                while (mult.size() <= b) mult.push_back(draw_multiplier(rng, cfg.cluster_alpha));
                m = mult[b];
            }
            if (rng.uniform() < -std::expm1(-lam * kappa * m)) {
                alu.set_defect(u, random_defect(rng, cfg.k, true, cfg.activation));
                ++res.alu_defective_total;
            }
        }
        return;
    }
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
    const double die = (cfg.share_die && cfg.cluster_alpha > 0.0) ? draw_multiplier(rng, cfg.cluster_alpha) : -1.0;
    inject_regfile_defects(rf, cfg, rng, res, die);   // register file first, as before
    inject_alu_defects(alu, cfg, rng, res, die);

    res.rf_ok  = regfile_self_test(rf, cfg.rounds, cfg.spare_domains).ok;
    // Further register-file copies (TMR needs three): each is sampled and repaired on its own, and all
    // must be repairable. With share_die they see the same die multiplier, so they fail together more.
    for (unsigned c = 1; c < cfg.rf_copies; ++c) {
        SlicedRegFile extra(cfg.rf_spares);
        extra.seed_defects(rng.next());
        ChipResult scratch;
        inject_regfile_defects(extra, cfg, rng, scratch, die);
        res.rf_defective_total += scratch.rf_defective_total;
        res.rf_ok = regfile_self_test(extra, cfg.rounds, cfg.spare_domains).ok && res.rf_ok;
    }
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
