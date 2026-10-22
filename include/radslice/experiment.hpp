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

    // G15. Control state (PC, fetched instruction, CSRs) triplicated and voted: every upset in it is
    // masked. Without it a flip in the PC or an instruction word is simply taken.
    bool     control_tmr = false;
    // SECDED over each 64-bit memory word: one flipped bit in a word is corrected when the word is
    // read or written, two are detected and stop the machine when the word is read, three or more
    // escape (a worst case: real codes catch some of them). A scrub every `mem_scrub_period`
    // instructions corrects pending single flips so that they do not pair up into doubles; it only
    // notes a double. With 0 there is no scrub and flips accumulate until the word is touched.
    bool     mem_ecc = false;
    unsigned mem_scrub_period = 0;
    // Online self-test and remap of the register file every N retired instructions (0 = off). Costs
    // `kRetestCyclesPerSlice` model cycles per physical slice per copy. If a copy has run out of spares
    // the retest fails: parity-dup and TMR carry on (the vote still masks), the others fail-stop.
    unsigned retest_period = 0;
};

constexpr u64 kRetestCyclesPerSlice = 8;   // 4 patterns x (write + read); tier C

struct FaultRates {
    double rf_per_bit_per_cycle = 0.0;
    double alu_per_bit_per_beat = 0.0;
    // G15. All per bit per model cycle except the last, and all tier C.
    double pc_per_bit_per_cycle  = 0.0;   // 64 exposed bits
    double ir_per_bit_per_cycle  = 0.0;   // 32 exposed bits of the instruction being fetched
    double csr_per_bit_per_cycle = 0.0;   // 5 CSRs x 64 bits: mstatus, mie, mtvec, mscratch, mepc
    double mem_per_bit_per_cycle = 0.0;   // per bit of the memory the program touches (its footprint)
    double infield_per_slice_per_cycle = 0.0;  // a stuck-bit defect appears in a register-file slice
    unsigned burst = 1;                   // bits flipped by one register-file or memory upset (adjacent, one word)
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
enum class ForcedKind { RegFile, Pc, Ir, Mem, Defect };

struct ForcedFault {
    bool     active = false;
    u64      at_instret = 0;
    unsigned copy = 0;
    u32      word = 2;      // x2 = sp, live in every workload
    unsigned bit = 30;
    // G15: what to hit. RegFile uses copy/word/bit and `burst`. Pc flips `bit` of the PC. Ir flips
    // `bit` (0..31) of the next instruction fetched. Mem flips `burst` bits of the byte at
    // DRAM base + mem_offset starting at `bit` (a word-relative bit, 0..63). Defect puts a stuck
    // bit in physical slice `word` of copy `copy`.
    ForcedKind kind = ForcedKind::RegFile;
    unsigned   burst = 1;
    u64        mem_offset = 0;
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
    u64         mem_words_touched = 0;   // distinct 64-bit words of DRAM read, written or fetched (the footprint)
    std::vector<u64> touched_offsets;    // their byte offsets from the DRAM base, ascending
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
    u64      pc_upsets = 0, ir_upsets = 0, csr_upsets = 0, mem_upsets = 0;
    u64      control_masked = 0, mem_corrected = 0, infield_defects = 0, retests = 0, retest_failures = 0;
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
