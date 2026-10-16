// radslice_inject: run one fault-injection experiment cell and print one CSV row.
//
//   radslice_inject --elf build/workloads/chacha20.elf --rf tmr --alu dwc --scrub 64
//       --lambda-rf 1e-8 --lambda-alu 1e-8 --runs 1000 --seed 1
//
// Options: --rf none|parity|parity-dup|tmr   --alu none|dwc|tmr   --scrub N
//          --lambda-rf R  --lambda-alu R   (per bit per model cycle / per output bit per beat)
//          --runs N --first-run I --seed S --k 8|16|32 --label TEXT
//          --defects P RF_SPARES ALU_SPARES   (permanent defects with the M3 self-test)
//          --verbose   (one line per run)
//
// Run i uses the stream Rng(seed + i * golden-ratio), so results are reproducible and a
// campaign can be split across processes with --first-run / --runs. Upset rates are
// tier-C parameters; nothing here is a measurement.

#include <cstdlib>
#include <iostream>
#include <string>

#include "radslice/experiment.hpp"

using namespace radslice;

int main(int argc, char** argv) {
    std::string elf, label, rf = "none", alu = "none";
    unsigned scrub = 0, k = 16;
    double lam_rf = 0, lam_alu = 0;
    unsigned long long runs = 100, first = 0, seed = 1;
    ChipSpec chip;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto val = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "--elf") elf = val();
        else if (a == "--rf") rf = val();
        else if (a == "--alu") alu = val();
        else if (a == "--scrub") scrub = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
        else if (a == "--lambda-rf") lam_rf = std::strtod(val().c_str(), nullptr);
        else if (a == "--lambda-alu") lam_alu = std::strtod(val().c_str(), nullptr);
        else if (a == "--runs") runs = std::strtoull(val().c_str(), nullptr, 0);
        else if (a == "--first-run") first = std::strtoull(val().c_str(), nullptr, 0);
        else if (a == "--seed") seed = std::strtoull(val().c_str(), nullptr, 0);
        else if (a == "--k") k = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
        else if (a == "--label") label = val();
        else if (a == "--verbose") verbose = true;
        else if (a == "--defects") {
            chip.defects = true;
            chip.p = std::strtod(val().c_str(), nullptr);
            chip.rf_spares = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
            chip.alu_spares = static_cast<unsigned>(std::strtoul(val().c_str(), nullptr, 0));
        } else { std::cerr << "unknown option " << a << "\n"; return 2; }
    }
    chip.k = k;

    Protection prot;
    if (rf == "none") prot.rf = RfProtection::None;
    else if (rf == "parity") prot.rf = RfProtection::Parity;
    else if (rf == "parity-dup") prot.rf = RfProtection::ParityDup;
    else if (rf == "tmr") prot.rf = RfProtection::Tmr;
    else { std::cerr << "bad --rf\n"; return 2; }
    if (alu == "none") prot.alu = AluProtection::None;
    else if (alu == "dwc") prot.alu = AluProtection::Dwc;
    else if (alu == "tmr") prot.alu = AluProtection::Tmr;
    else { std::cerr << "bad --alu\n"; return 2; }
    prot.scrub_period = scrub;

    if (elf.empty()) { std::cerr << "need --elf\n"; return 2; }
    const Workload w(elf);
    if (!w.ok()) { std::cerr << "error: " << w.error() << "\n"; return 1; }
    const Golden g = w.run_golden();
    if (!g.ok) { std::cerr << "error: golden run failed: " << g.error << "\n"; return 1; }

    // The unprotected sliced core with no upsets: the cycle baseline for overheads.
    ChipSpec clean = chip;
    clean.defects = false;
    Rng brng(1);
    const RunResult base = w.run(g, Protection{}, FaultRates{}, clean, ForcedFault{}, brng);

    FaultRates rates{lam_rf, lam_alu};
    unsigned long long count[kNumOutcomes] = {};
    unsigned long long hang = 0, crash = 0, diverged = 0, with_upset = 0;
    double sum_cycles = 0, sum_rf_up = 0, sum_alu_up = 0, sum_corr = 0;
    unsigned long long ran = 0;

    for (unsigned long long i = first; i < first + runs; ++i) {
        Rng rng = Rng::stream(seed, i);
        const RunResult r = w.run(g, prot, rates, chip, ForcedFault{}, rng);
        ++count[static_cast<int>(r.outcome)];
        hang += r.hang; crash += r.crash;
        diverged += r.first_divergence >= 0;
        with_upset += (r.rf_upsets + r.alu_upsets) > 0;
        if (r.outcome != Outcome::BootFailure) {
            sum_cycles  += static_cast<double>(r.model_cycles);
            ++ran;
        }
        sum_rf_up += static_cast<double>(r.rf_upsets);
        sum_alu_up += static_cast<double>(r.alu_upsets);
        sum_corr += static_cast<double>(r.rf_corrected + r.alu_corrected);
        if (verbose)
            std::cerr << "run " << i << " " << to_string(r.outcome) << " instret=" << r.instret
                      << " rf_upsets=" << r.rf_upsets << " alu_upsets=" << r.alu_upsets
                      << " first_div=" << r.first_divergence << "\n";
    }

    const double n = static_cast<double>(runs);
    std::cout << "label,workload,rf,alu,scrub,lambda_rf,lambda_alu,k,defects_p,rf_spares,alu_spares,"
                 "runs,correct,detected_recovered,detected_unrecovered,sdc,hang_crash,boot_failure,"
                 "hang,crash,diverged,with_upset,mean_rf_upsets,mean_alu_upsets,mean_corrections,"
                 "golden_instret,baseline_cycles,mean_cycles\n";
    std::cout << label << "," << elf << "," << rf << "," << alu << "," << scrub << "," << lam_rf << ","
              << lam_alu << "," << k << "," << (chip.defects ? chip.p : 0.0) << "," << chip.rf_spares
              << "," << chip.alu_spares << "," << runs;
    for (int o = 0; o < kNumOutcomes; ++o) std::cout << "," << count[o];
    std::cout << "," << hang << "," << crash << "," << diverged << "," << with_upset << ","
              << sum_rf_up / n << "," << sum_alu_up / n << "," << sum_corr / n << "," << g.instret
              << "," << base.model_cycles << "," << (ran ? sum_cycles / static_cast<double>(ran) : 0.0)
              << "\n";
    return 0;
}
