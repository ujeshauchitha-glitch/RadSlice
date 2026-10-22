#include "radslice/experiment.hpp"

#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <set>

#include "bus.hpp"
#include "clint.hpp"
#include "cpu.hpp"
#include "decoder.hpp"
#include "dram.hpp"
#include "elf_loader.hpp"
#include "radslice/selftest.hpp"
#include "syscon.hpp"

namespace radslice {

const char* to_string(Outcome o) {
    switch (o) {
        case Outcome::Correct:             return "correct";
        case Outcome::DetectedRecovered:   return "detected_recovered";
        case Outcome::DetectedUnrecovered: return "detected_unrecovered";
        case Outcome::Sdc:                 return "sdc";
        case Outcome::HangCrash:           return "hang_crash";
        case Outcome::BootFailure:         return "boot_failure";
    }
    return "?";
}

namespace {

constexpr u64 kDramBytes = 4ull * 1024 * 1024;

// DRAM with a SECDED ledger (G15). With `ecc` off it behaves exactly like Dram.
//
// An upset is a mask of flipped bits in one 64-bit word. With ECC the flips are not applied to the
// stored bytes; they are held in a ledger of what the check bits would be reporting:
//   one bit    corrected whenever the word is read or written (the entry then goes away), or by a scrub
//   two bits   detected but uncorrectable: reading the word raises `uncorrectable` (a scrub only notes it)
//   three+     escapes: the flips are applied silently (a worst case; real codes catch some)
// Flips accumulate across upsets until the word is read, written or scrubbed.
class EccDram : public Dram {
public:
    using Dram::Dram;
    bool ecc = false;
    bool uncorrectable = false;
    u64  corrected = 0;
    bool track = false;                       // record which 64-bit words are touched (the memory footprint)
    std::set<u64> touched;

    void upset(u64 word_off, u64 mask) {
        if (!ecc) { flip_bytes(word_off, mask); return; }
        const auto it = ledger_.find(word_off);
        const u64 combined = (it == ledger_.end() ? 0 : it->second) ^ mask;
        const int weight = __builtin_popcountll(combined);
        if (weight == 0) ledger_.erase(word_off);
        else if (weight <= 2) ledger_[word_off] = combined;
        else { ledger_.erase(word_off); flip_bytes(word_off, combined); }
    }
    void flip_bytes(u64 word_off, u64 mask) {
        if (word_off + 8 > bytes().size()) return;
        for (unsigned i = 0; i < 8; ++i) bytes()[word_off + i] ^= static_cast<u8>(mask >> (8 * i));
    }
    // A pass of the scrubber over every word: singles are corrected. A double is detected and the word
    // is poisoned, not acted on: the machine only stops if the word is later read (or partly written),
    // as a real system would not halt over memory nobody uses.
    void scrub() {
        for (auto it = ledger_.begin(); it != ledger_.end();) {
            if (__builtin_popcountll(it->second) == 1) { ++corrected; it = ledger_.erase(it); }
            else ++it;
        }
    }
    Result<u64> load(u64 offset, unsigned size_bytes) override {
        if (track) note(offset, size_bytes);
        if (ecc) check(offset, size_bytes, false);
        return Dram::load(offset, size_bytes);
    }
    Status store(u64 offset, unsigned size_bytes, u64 value) override {
        if (track) note(offset, size_bytes);
        if (ecc) check(offset, size_bytes, true);
        return Dram::store(offset, size_bytes, value);
    }

private:
    void note(u64 offset, unsigned size_bytes) {
        for (u64 w = offset & ~7ull; w < offset + size_bytes; w += 8) touched.insert(w);
    }
    void check(u64 offset, unsigned size_bytes, bool is_store) {
        if (ledger_.empty()) return;
        for (u64 w = offset & ~7ull; w < offset + size_bytes; w += 8) {
            const auto it = ledger_.find(w);
            if (it == ledger_.end()) continue;
            if (__builtin_popcountll(it->second) == 1) { ++corrected; ledger_.erase(it); }
            else { uncorrectable = true; if (is_store) ledger_.erase(it); }
        }
    }
    std::map<u64, u64> ledger_;
};

struct Machine {
    Bus                  bus;
    EccDram*             dram = nullptr;
    Clint*               clint = nullptr;
    std::unique_ptr<Cpu> cpu;
    std::string          error;

    bool build(const std::vector<u8>& image) {
        auto dram_owned  = std::make_unique<EccDram>(kDramBytes);
        auto clint_owned = std::make_unique<Clint>();
        auto sys_owned   = std::make_unique<Syscon>();
        dram  = dram_owned.get();
        clint = clint_owned.get();
        Syscon* syscon = sys_owned.get();
        if (!bus.attach(std::move(dram_owned)) || !bus.attach(std::move(clint_owned)) ||
            !bus.attach(std::move(sys_owned))) {
            error = "cannot build the address map";
            return false;
        }
        clint->instructions_per_tick = 1;   // the timer ticks once per retired instruction
        const LoadedImage img = load_elf(image, bus);
        if (!img.ok) { error = img.error; return false; }
        cpu = std::make_unique<Cpu>(bus);
        cpu->pc               = img.entry;
        cpu->clint            = clint;
        cpu->syscon           = syscon;
        cpu->htif_tohost_addr = img.tohost;
        return true;
    }

    std::array<u64, kResultWords> result() const {
        std::array<u64, kResultWords> r{};
        for (unsigned i = 0; i < kResultWords; ++i) {
            const Result<u64> v = bus.load(kResultBase + 8ull * i, 8, AccessType::Load);
            r[i] = v ? v.value : ~0ull;
        }
        return r;
    }
};

bool writes_rd(u32 opcode) {
    switch (opcode) {
        case opcodes::LUI: case opcodes::AUIPC: case opcodes::JAL: case opcodes::JALR:
        case opcodes::LOAD: case opcodes::OP_IMM: case opcodes::OP: case opcodes::OP_IMM_32:
        case opcodes::OP_32: case opcodes::SYSTEM:
            return true;
        default:
            return false;
    }
}

u8 rd_of_next(Cpu& cpu) {
    const Result<DecodedInst> d = cpu.fetch_inst();
    return (d && writes_rd(d.value.opcode) && d.value.rd != 0) ? static_cast<u8>(d.value.rd) : 0;
}

double exp_draw(Rng& rng, double rate) {
    double u = rng.uniform();
    if (u < 1e-300) u = 1e-300;
    return -std::log(u) / rate;
}

}  // namespace

Workload::Workload(const std::string& elf_path) : path_(elf_path) {
    std::ifstream f(elf_path, std::ios::binary | std::ios::ate);
    if (!f) { error_ = "cannot open " + elf_path; return; }
    const std::streamsize len = f.tellg();
    f.seekg(0);
    image_.resize(static_cast<std::size_t>(len));
    if (len > 0 && !f.read(reinterpret_cast<char*>(image_.data()), len)) {
        error_ = "cannot read " + elf_path;
        return;
    }
    ok_ = is_elf(image_);
    if (!ok_) error_ = elf_path + " is not an ELF file";
}

Golden Workload::run_golden(u64 max_steps) const {
    Golden g;
    Machine m;
    if (!m.build(image_)) { g.error = m.error; return g; }
    Cpu& cpu = *m.cpu;
    m.dram->track = true;
    while (g.instret < max_steps) {
        const u8 rd = rd_of_next(cpu);
        u64 n = 0;
        const Status st = cpu.run(1, &n);
        if (!st) { g.error = "golden run trapped"; return g; }
        ++g.instret;
        g.trace.push_back({cpu.pc, rd ? cpu.read_reg(rd) : 0, rd});
        if (cpu.halted && cpu.htif_tohost_value != 0) {
            g.exit_code = cpu.htif_tohost_value >> 1;
            g.result    = m.result();
            g.mem_words_touched = m.dram->touched.size();
            for (u64 w : m.dram->touched) g.touched_offsets.push_back(w);
            g.ok        = (g.exit_code == 0);
            if (!g.ok) g.error = "golden run exited with a failure code";
            return g;
        }
    }
    g.error = "golden run did not finish";
    return g;
}

RunResult Workload::run(const Golden& golden, const Protection& prot, const FaultRates& rates,
                        const ChipSpec& chip, const ForcedFault& forced, Rng& rng) const {
    RunResult res;
    Machine m;
    if (!m.build(image_)) { res.outcome = Outcome::HangCrash; res.crash = true; return res; }
    Cpu& cpu = *m.cpu;

    const bool       infield = rates.infield_per_slice_per_cycle > 0.0 || forced.kind == ForcedKind::Defect;
    ProtectedRegFile rf(prot.rf, (chip.defects || infield) ? chip.rf_spares : 0);
    TransientAlu     alu(64, chip.k, chip.defects ? chip.alu_spares : 0, rng.next());
    RedundantAlu     red(alu, prot.alu);

    // Manufacturing: sample permanent defects into every register-file copy and the
    // ALU, then run the boot self-test and remap on each. A chip that cannot be
    // repaired never runs.
    if (chip.defects) {
        ChipConfig cc;
        cc.p = chip.p; cc.k = chip.k; cc.rf_spares = chip.rf_spares; cc.alu_spares = chip.alu_spares;
        ChipResult scratch;
        bool repaired = true;
        for (unsigned c = 0; c < rf.copies(); ++c) {
            rf.copy(c).seed_defects(rng.next());
            inject_regfile_defects(rf.copy(c), cc, rng, scratch);
        }
        inject_alu_defects(alu, cc, rng, scratch);
        for (unsigned c = 0; c < rf.copies(); ++c) repaired = regfile_self_test(rf.copy(c), 1).ok && repaired;
        repaired = alu_self_test(alu, 1).ok && repaired;
        if (!repaired) { res.outcome = Outcome::BootFailure; return res; }
    }
    alu.reset_stats();
    alu.reset_call_counter();
    alu.set_rate(rates.alu_per_bit_per_beat);

    cpu.regfile     = &rf;
    cpu.alu_backend = &red;

    const double   rf_rate = rates.rf_per_bit_per_cycle * static_cast<double>(rf.exposed_bits());
    // A rate of zero draws nothing from the generator, so campaigns that do not use the G15 fault
    // classes give the same numbers as before they existed.
    auto clock = [&](double rate) { return rate > 0 ? exp_draw(rng, rate) : static_cast<double>(INFINITY); };
    double         next_rf_event = clock(rf_rate);
    const double   pc_rate  = rates.pc_per_bit_per_cycle * 64.0;
    const double   ir_rate  = rates.ir_per_bit_per_cycle * 32.0;
    const double   csr_rate = rates.csr_per_bit_per_cycle * 5.0 * 64.0;
    // Memory upsets land on the words the golden run touched (its footprint), at the stated rate per bit:
    // upsets in memory the program never uses cannot matter, so they are not simulated.
    const double   mem_rate = rates.mem_per_bit_per_cycle * 64.0 * static_cast<double>(golden.touched_offsets.size());
    const double   inf_rate = rates.infield_per_slice_per_cycle * static_cast<double>(rf.total_slices_all_copies());
    double next_pc = clock(pc_rate), next_ir = clock(ir_rate), next_csr = clock(csr_rate),
           next_mem = clock(mem_rate), next_inf = clock(inf_rate);
    const unsigned burst = rates.burst ? rates.burst : 1;
    const u64      budget = 3 * golden.instret + 1000;
    u64            cycles = 0;
    bool           finished = false, fail_stop = false;
    m.dram->ecc = prot.mem_ecc;

    // Flip `count` adjacent bits of the 64-bit word at DRAM offset `offset`, starting at word bit
    // `bit` and clipped at the top of the word (see EccDram for what SECDED does with the flips).
    auto mem_flip = [&](u64 offset, unsigned bit, unsigned count) {
        u64 mask = 0;
        for (unsigned i = 0; i < count && bit + i < 64; ++i) mask |= 1ull << (bit + i);
        ++res.mem_upsets;
        m.dram->upset(offset & ~7ull, mask);
    };
    auto pc_flip = [&](unsigned bit) {
        ++res.pc_upsets;
        if (prot.control_tmr) { ++res.control_masked; return; }
        cpu.pc ^= 1ull << (bit % 64);
    };
    bool     ir_arm = forced.active && forced.kind == ForcedKind::Ir;      // armed for instruction forced.at_instret
    unsigned ir_bit = forced.bit % 32;
    static const u32 kCsrs[5] = {csr::MSTATUS, csr::MIE, csr::MTVEC, csr::MSCRATCH, csr::MEPC};

    while (res.instret < budget) {
        // An upset in the fetched instruction word: flip the bit in memory for this one fetch.
        bool     ir_hit = false;
        u32      ir_saved = 0;
        u64      ir_addr = cpu.pc;
        bool     ir_due = false;
        if (cycles >= next_ir) { ir_due = true; ir_bit = static_cast<unsigned>(rng.next() % 32); next_ir += clock(ir_rate); }
        if (ir_arm && res.instret + 1 == forced.at_instret) { ir_due = true; ir_arm = false; }
        if (ir_due) {
            ++res.ir_upsets;
            if (prot.control_tmr) {
                ++res.control_masked;
            } else {
                // The flip goes straight into the stored bytes for the one fetch, bypassing the
                // ECC ledger: it is a fault in the fetch path, not in the memory array.
                std::vector<u8>& mem = m.dram->bytes();
                const u64 off = ir_addr - DRAM_BASE;
                if (ir_addr >= DRAM_BASE && off + 4 <= mem.size()) {
                    for (unsigned i = 0; i < 4; ++i) ir_saved |= static_cast<u32>(mem[off + i]) << (8 * i);
                    for (unsigned i = 0; i < 4; ++i) mem[off + i] ^= static_cast<u8>((1u << ir_bit) >> (8 * i));
                    ir_hit = true;
                }
            }
        }
        const u8  rd = rd_of_next(cpu);
        const u64 beats0 = alu.stats().beats;
        u64 n = 0;
        const Status st = cpu.run(1, &n);
        if (ir_hit) {
            std::vector<u8>& mem = m.dram->bytes();
            for (unsigned i = 0; i < 4; ++i) mem[ir_addr - DRAM_BASE + i] = static_cast<u8>(ir_saved >> (8 * i));
        }
        ++res.instret;
        cycles += 1 + (alu.stats().beats - beats0);

        // A parity or vote failure on a register read is flagged by the hardware as
        // the value is read, so it takes priority over whatever that corrupted value
        // then did to the instruction (such as a faulting access).
        if (rf.fail_stop() || m.dram->uncorrectable) { fail_stop = true; break; }
        if (!st) {                                  // a crash is by definition a divergence
            res.crash = true;
            if (res.first_divergence < 0) res.first_divergence = static_cast<long long>(res.instret - 1);
            break;
        }

        // Lockstep with the golden trace.
        if (res.first_divergence < 0) {
            const u64 i = res.instret - 1;
            if (i >= golden.trace.size()) {
                res.first_divergence = static_cast<long long>(i);
            } else {
                const TraceEntry& e = golden.trace[i];
                const bool same = cpu.pc == e.pc_after && rd == e.rd &&
                                  (rd == 0 || rf.peek(rd) == e.rd_value);
                if (!same) res.first_divergence = static_cast<long long>(i);
            }
        }

        if (cpu.halted && cpu.htif_tohost_value != 0) { finished = true; break; }
        if (rf.fail_stop()) { fail_stop = true; break; }

        if (prot.scrub_period && res.instret % prot.scrub_period == 0) {
            rf.scrub_one();
            ++cycles;                                     // a scrub read costs one beat
            if (rf.fail_stop()) { fail_stop = true; break; }
        }

        if (forced.active && res.instret == forced.at_instret) {
            switch (forced.kind) {
                case ForcedKind::RegFile:
                    for (unsigned i = 0; i < forced.burst && forced.bit + i < 64; ++i)
                        rf.flip_data(forced.copy, forced.word, forced.bit + i);
                    ++res.rf_upsets;
                    break;
                case ForcedKind::Pc:  pc_flip(forced.bit); break;
                case ForcedKind::Mem: mem_flip(forced.mem_offset, forced.bit, forced.burst); break;
                case ForcedKind::Defect: {
                    SliceDefect d;
                    d.kind = DefectKind::StuckBit; d.bit = forced.bit % 8; d.value = true;
                    rf.copy(forced.copy).set_defect(forced.word, d);
                    ++res.infield_defects;
                    break;
                }
                case ForcedKind::Ir: break;     // handled at the top of the loop
            }
        }
        while (cycles >= next_rf_event) {
            rf.inject_burst(rng.next() % rf.exposed_bits(), burst);
            ++res.rf_upsets;
            next_rf_event += exp_draw(rng, rf_rate);
        }
        while (cycles >= next_pc)  { pc_flip(static_cast<unsigned>(rng.next() % 64)); next_pc += exp_draw(rng, pc_rate); }
        while (cycles >= next_csr) {
            ++res.csr_upsets;
            const u32 reg = kCsrs[rng.next() % 5];
            const unsigned bit = static_cast<unsigned>(rng.next() % 64);
            if (prot.control_tmr) ++res.control_masked;
            else cpu.csrs.write(reg, cpu.csrs.read(reg) ^ (1ull << bit));
            next_csr += exp_draw(rng, csr_rate);
        }
        while (cycles >= next_mem) {
            const u64 off = golden.touched_offsets[rng.next() % golden.touched_offsets.size()];
            mem_flip(off, static_cast<unsigned>(rng.next() % 64), burst);
            next_mem += exp_draw(rng, mem_rate);
        }
        if (fail_stop) break;
        while (cycles >= next_inf) {
            const unsigned c = static_cast<unsigned>(rng.next() % rf.copies());
            const unsigned p = static_cast<unsigned>(rng.next() % rf.copy(c).total_slices());
            rf.copy(c).set_defect(p, random_defect(rng, 8, false));
            ++res.infield_defects;
            next_inf += exp_draw(rng, inf_rate);
        }
        if (prot.mem_ecc && prot.mem_scrub_period && res.instret % prot.mem_scrub_period == 0) m.dram->scrub();
        if (m.dram->uncorrectable) { fail_stop = true; break; }
        if (prot.retest_period && res.instret % prot.retest_period == 0) {
            ++res.retests;
            cycles += kRetestCyclesPerSlice * rf.total_slices_all_copies();
            if (!rf.retest()) {
                // Out of spares. A scheme that can mask a bad copy carries on, degraded; one that cannot
                // has just learned that its register file is broken and stops.
                ++res.retest_failures;
                const bool masks = prot.rf == RfProtection::Tmr || prot.rf == RfProtection::ParityDup;
                if (!masks) { fail_stop = true; break; }
            }
        }
    }

    res.alu_beats         = alu.stats().beats;
    res.model_cycles      = cycles;
    res.alu_upsets        = alu.upsets();
    res.rf_corrected      = rf.stats().corrected;
    res.alu_corrected     = red.stats().corrected;
    res.scrub_corrections = rf.stats().scrub_corrections;
    res.result            = m.result();    // the output is read back through the ECC, like any other read
    if (m.dram->uncorrectable) fail_stop = true;
    res.mem_corrected     = m.dram->corrected;

    const bool corrected_something = res.rf_corrected + res.alu_corrected + res.control_masked + res.mem_corrected > 0;
    if (fail_stop) {
        res.outcome = Outcome::DetectedUnrecovered;
    } else if (finished) {
        const u64 code = cpu.htif_tohost_value >> 1;
        if (code != 0) {
            res.outcome = Outcome::HangCrash;
            res.crash   = true;
        } else if (res.result != golden.result) {
            res.outcome = Outcome::Sdc;
        } else {
            res.outcome = corrected_something ? Outcome::DetectedRecovered : Outcome::Correct;
        }
    } else {
        res.outcome = Outcome::HangCrash;
        if (!res.crash) res.hang = true;
    }
    return res;
}

}  // namespace radslice
