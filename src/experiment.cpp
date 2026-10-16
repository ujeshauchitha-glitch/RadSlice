#include "radslice/experiment.hpp"

#include <cmath>
#include <fstream>
#include <memory>

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

struct Machine {
    Bus                  bus;
    Dram*                dram = nullptr;
    Clint*               clint = nullptr;
    std::unique_ptr<Cpu> cpu;
    std::string          error;

    bool build(const std::vector<u8>& image) {
        auto dram_owned  = std::make_unique<Dram>(kDramBytes);
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

    ProtectedRegFile rf(prot.rf, chip.defects ? chip.rf_spares : 0);
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
    double         next_rf_event = rf_rate > 0 ? exp_draw(rng, rf_rate) : INFINITY;
    const u64      budget = 3 * golden.instret + 1000;
    u64            cycles = 0;
    bool           finished = false, fail_stop = false;

    while (res.instret < budget) {
        const u8  rd = rd_of_next(cpu);
        const u64 beats0 = alu.stats().beats;
        u64 n = 0;
        const Status st = cpu.run(1, &n);
        ++res.instret;
        cycles += 1 + (alu.stats().beats - beats0);

        // A parity or vote failure on a register read is flagged by the hardware as
        // the value is read, so it takes priority over whatever that corrupted value
        // then did to the instruction (such as a faulting access).
        if (rf.fail_stop()) { fail_stop = true; break; }
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
            rf.flip_data(forced.copy, forced.word, forced.bit);
            ++res.rf_upsets;
        }
        while (cycles >= next_rf_event) {
            rf.inject(rng.next() % rf.exposed_bits());
            ++res.rf_upsets;
            next_rf_event += exp_draw(rng, rf_rate);
        }
    }

    res.alu_beats         = alu.stats().beats;
    res.model_cycles      = cycles;
    res.alu_upsets        = alu.upsets();
    res.rf_corrected      = rf.stats().corrected;
    res.alu_corrected     = red.stats().corrected;
    res.scrub_corrections = rf.stats().scrub_corrections;
    res.result            = m.result();

    const bool corrected_something = res.rf_corrected + res.alu_corrected > 0;
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
