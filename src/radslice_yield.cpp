// radslice_yield: Monte Carlo over simulated chips. Prints CSV; nothing here is a
// measurement. Every chip's defects are sampled from the tier-C model, then the real
// boot self-test and remap run on the real register-file and ALU models.
//
//   radslice_yield --mode yield --trials 20000 --seed 1 --p 0.71 --k 16
//                  --rf 0,32,64,96,128 --alu 0,1,2 [--rounds 1] [--activation 1] [--verify]
//   radslice_yield --mode weak  --trials 20000 --seed 1 --q 0.1,0.5 --rounds 1,2,4,8
//
// A fixed --seed gives identical output on every run and platform.

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "radslice/chip_sim.hpp"

namespace {

std::vector<double> list(const std::string& s) {
    std::vector<double> v;
    std::stringstream ss(s);
    for (std::string tok; std::getline(ss, tok, ',');) v.push_back(std::strtod(tok.c_str(), nullptr));
    return v;
}

}  // namespace

int main(int argc, char** argv) {
    std::string mode = "yield", rf = "0", alu = "0", q = "0.5", rounds = "1";
    unsigned long long trials = 1000, seed = 1;
    double p = 0.71, activation = 1.0;
    unsigned k = 16;
    bool verify = false, share_die = false;
    double cluster_alpha = 0.0;
    unsigned cluster_block = 0, alu_cluster_block = 0, domains = 1, copies = 1;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto val = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "--mode") mode = val();
        else if (a == "--trials") trials = std::strtoull(val().c_str(), nullptr, 0);
        else if (a == "--seed") seed = std::strtoull(val().c_str(), nullptr, 0);
        else if (a == "--p") p = std::strtod(val().c_str(), nullptr);
        else if (a == "--k") k = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
        else if (a == "--rf") rf = val();
        else if (a == "--alu") alu = val();
        else if (a == "--q") q = val();
        else if (a == "--rounds") rounds = val();
        else if (a == "--activation") activation = std::strtod(val().c_str(), nullptr);
        else if (a == "--verify") verify = true;
        else if (a == "--cluster-alpha") cluster_alpha = std::strtod(val().c_str(), nullptr);
        else if (a == "--cluster-block") cluster_block = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
        else if (a == "--alu-cluster-block") alu_cluster_block = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
        else if (a == "--domains") domains = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
        else if (a == "--copies") copies = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
        else if (a == "--share-die") share_die = true;
        else { std::cerr << "unknown option " << a << "\n"; return 2; }
    }

    using namespace radslice;
    unsigned long long row = 0;
    auto row_rng = [&]() { return Rng::stream(seed, row++); };

    if (mode == "weak") {
        std::cout << "activation,rounds,trials,detected\n";
        for (double qq : list(q))
            for (double rr : list(rounds)) {
                Rng rng = row_rng();
                unsigned long long hit = 0;
                for (unsigned long long t = 0; t < trials; ++t)
                    hit += detect_weak_slice(qq, static_cast<unsigned>(rr), rng) ? 1 : 0;
                std::cout << qq << "," << static_cast<unsigned>(rr) << "," << trials << "," << hit << "\n";
            }
        return 0;
    }
    if (mode != "yield") { std::cerr << "unknown mode " << mode << "\n"; return 2; }

    std::cout << "rf_spares,alu_spares,k,p,trials,rf_ok,alu_ok,both_ok,verified,escapes,latent,"
                 "mean_rf_defective_primary,mean_alu_defective,"
                 "cluster_alpha,cluster_block,alu_cluster_block,domains,copies,share_die\n";
    for (double rs : list(rf))
        for (double as : list(alu)) {
            ChipConfig cfg;
            cfg.p = p; cfg.k = k; cfg.rf_spares = static_cast<unsigned>(rs);
            cfg.alu_spares = static_cast<unsigned>(as);
            cfg.rounds = static_cast<unsigned>(list(rounds).at(0));
            cfg.activation = activation; cfg.verify = verify;
            cfg.cluster_alpha = cluster_alpha; cfg.cluster_block = cluster_block;
            cfg.alu_cluster_block = alu_cluster_block; cfg.spare_domains = domains;
            cfg.rf_copies = copies; cfg.share_die = share_die;
            Rng rng = row_rng();
            unsigned long long rf_ok = 0, alu_ok = 0, both = 0, ver = 0, esc = 0, lat = 0;
            double sum_rf = 0, sum_alu = 0;
            for (unsigned long long t = 0; t < trials; ++t) {
                const ChipResult r = simulate_chip(cfg, rng);
                rf_ok += r.rf_ok; alu_ok += r.alu_ok; both += (r.rf_ok && r.alu_ok);
                ver += r.verified; esc += r.escape; lat += r.latent;
                sum_rf += r.rf_defective_primary; sum_alu += r.alu_defective_total;
            }
            std::cout << cfg.rf_spares << "," << cfg.alu_spares << "," << cfg.k << "," << cfg.p << ","
                      << trials << "," << rf_ok << "," << alu_ok << "," << both << "," << ver << ","
                      << esc << "," << lat << "," << sum_rf / static_cast<double>(trials) << ","
                      << sum_alu / static_cast<double>(trials) << "," << cluster_alpha << ","
                      << cluster_block << "," << alu_cluster_block << "," << domains << ","
                      << copies << "," << (share_die ? 1 : 0) << "\n";
        }
    return 0;
}
