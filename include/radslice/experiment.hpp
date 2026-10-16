#pragma once

#include <array>
#include <string>
#include <vector>

#include "radslice/chip_sim.hpp"
#include "radslice/protected_regfile.hpp"
#include "radslice/transient_alu.hpp"

namespace radslice {

// ---------------------------------------------------------------------------
// Fault-injection experiments (M4).
//
// A workload is run once on the plain golden core, recording a trace (the PC after
// each instruction and the value written to rd). It is then run many times on the
// hardware model with upsets injected, in lockstep with that trace, and each run is
// assigned exactly one outcome:
//
//   Correct              finished, output identical to golden, no protection fired
//   DetectedRecovered    finished, output identical, and a protection mechanism
//                        detected and corrected at least one upset along the way
//   DetectedUnrecovered  the hardware detected an upset it could not correct and
//                        stopped (fail-stop). Not in the original outcome list; it is
//                        distinct from silent corruption and from a hang, and parity
//                        without a second copy can only end this way.
//   Sdc                  finished, output differs from golden, nothing noticed
//   HangCrash            did not finish within the budget (hang), or ended in a trap or
//                        an error exit (crash). Counted separately in RunResult.
//   BootFailure          the chip could not be repaired at the manufacturing self-test
//                        (only when permanent defects are enabled)
//
// Upsets arrive as a Poisson process over model cycles in the register file (one
// process over every exposed bit) and per output bit per beat in the ALU. The rates
// are tier-C parameters and are always reported with the results.
// ---------------------------------------------------------------------------

enum class Outcome { Correct, DetectedRecovered, DetectedUnrecovered, Sdc, HangCrash, BootFailure };
constexpr int kNumOutcomes = 6;
const char* to_string(Outcome o);

constexpr u64 kResultBase  = 0x80100000ull;   // the workloads' result window
constexpr unsigned kResultWords = 32;
constexpr u64 kCrashCode   = 0x100;           // exit code a workload uses to report a crash

struct Protection {
    RfProtection  rf  = RfProtection::None;
    AluProtection alu = AluProtection::None;
    unsigned scrub_period = 0;   // scrub one register word every N retired instructions; 0 = off
};

struct FaultRates {
    double rf_per_bit_per_cycle = 0.0;
    double alu_per_bit_per_beat = 0.0;
};

struct ChipSpec {
    bool     defects    = false;   // sample permanent defects (M3) and run the boot self-test
    double   p          = 0.71;
    unsigned k          = 16;      // ALU slice width
    unsigned rf_spares  = 0;       // per register-file copy
    unsigned alu_spares = 0;
};

// A single deterministic upset, for tests: flip `bit` of register `word` in register
// file copy `copy` just after instruction number `at_instret` retires.
struct ForcedFault {
    bool     active = false;
    u64      at_instret = 0;
    unsigned copy = 0;
    u32      word = 2;      // x2 = sp, live in every workload
    unsigned bit = 30;
};

struct TraceEntry {
    u64 pc_after;
    u64 rd_value;
    u8  rd;        // 0 if the instruction writes no register
};

struct Golden {
    bool        ok = false;
    std::string error;
    u64         instret = 0;
    u64         exit_code = 0;
    std::vector<TraceEntry> trace;
    std::array<u64, kResultWords> result{};
};

struct RunResult {
    Outcome  outcome = Outcome::Correct;
    bool     hang = false, crash = false;
    u64      instret = 0;
    u64      alu_beats = 0;
    u64      model_cycles = 0;       // instret + alu beats + scrub reads (tier-C model)
    u64      rf_upsets = 0, alu_upsets = 0;
    u64      rf_corrected = 0, alu_corrected = 0, scrub_corrections = 0;
    long long first_divergence = -1; // first retired instruction whose pc or rd value differed
    std::array<u64, kResultWords> result{};
};

class Workload {
public:
    // Load an ELF. `ok()` is false (and `error()` says why) if it cannot be read.
    explicit Workload(const std::string& elf_path);
    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }
    const std::string& path() const { return path_; }
    const std::vector<u8>& image() const { return image_; }

    // Run on the plain golden core with no backends, recording the trace.
    Golden run_golden(u64 max_steps = 50'000'000) const;

    // One injection run on the hardware model.
    RunResult run(const Golden& golden, const Protection& prot, const FaultRates& rates,
                  const ChipSpec& chip, const ForcedFault& forced, Rng& rng) const;

private:
    std::string      path_, error_;
    std::vector<u8>  image_;
    bool             ok_ = false;
};

}  // namespace radslice
