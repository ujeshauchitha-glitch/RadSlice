// radslice_vliw: run a bare-metal ELF on the golden core with the sliced ALU underneath, record every
// retired instruction's registers and slice-serial cost, and schedule that trace on W parallel
// K-bit units (G17). It prints one machine-readable line; the cycle numbers are MODEL ESTIMATES.
//
//   radslice_vliw [--max-steps N] [--k 8|16|32] [--base-cycles C] image.elf
//
// The program runs on one fault-free sliced ALU, so the trace is what the real instructions did.
// Unit failures are applied afterwards by the study script, which only needs the cycle count per
// number of good units.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bus.hpp"
#include "clint.hpp"
#include "cpu.hpp"
#include "dram.hpp"
#include "elf_loader.hpp"
#include "plic.hpp"
#include "radslice/sliced_alu.hpp"
#include "radslice/vliw.hpp"
#include "syscon.hpp"
#include "uart.hpp"

namespace {

bool read_file(const std::string& path, std::vector<u8>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize len = f.tellg();
    if (len < 0) return false;
    f.seekg(0);
    out.resize(static_cast<std::size_t>(len));
    return len == 0 || static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), len));
}

radslice::TraceOp make_op(const DecodedInst& d, u32 latency) {
    using namespace opcodes;
    radslice::TraceOp op;
    op.latency = latency;
    const auto reg = [](u32 r) { return static_cast<u8>(r & 31); };
    switch (d.opcode) {
        case OP: case OP_32:
            op.kind = radslice::OpKind::Alu; op.rd = reg(d.rd); op.rs1 = reg(d.rs1); op.rs2 = reg(d.rs2); break;
        case OP_IMM: case OP_IMM_32:
            op.kind = radslice::OpKind::Alu; op.rd = reg(d.rd); op.rs1 = reg(d.rs1); break;
        case LUI: case AUIPC:
            op.kind = radslice::OpKind::Alu; op.rd = reg(d.rd); break;
        case LOAD:
            op.kind = radslice::OpKind::Load; op.rd = reg(d.rd); op.rs1 = reg(d.rs1); break;
        case STORE:
            op.kind = radslice::OpKind::Store; op.rs1 = reg(d.rs1); op.rs2 = reg(d.rs2); break;
        case BRANCH:
            op.kind = radslice::OpKind::Control; op.rs1 = reg(d.rs1); op.rs2 = reg(d.rs2); break;
        case JAL:
            op.kind = radslice::OpKind::Control; op.rd = reg(d.rd); break;
        case JALR:
            op.kind = radslice::OpKind::Control; op.rd = reg(d.rd); op.rs1 = reg(d.rs1); break;
        default:
            op.kind = radslice::OpKind::Other; break;   // CSR, system, fence, atomics, floating point
    }
    return op;
}

}  // namespace

int main(int argc, char** argv) {
    u64         max_steps = 100'000'000, base = 1;
    unsigned    k = 16;
    std::string image;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--max-steps" && i + 1 < argc) max_steps = std::strtoull(argv[++i], nullptr, 0);
        else if (a == "--k" && i + 1 < argc) k = static_cast<unsigned>(std::atoi(argv[++i]));
        else if (a == "--base-cycles" && i + 1 < argc) base = std::strtoull(argv[++i], nullptr, 0);
        else if (!a.empty() && a[0] == '-') { std::cerr << "usage: radslice_vliw [--max-steps N] [--k K] [--base-cycles C] image.elf\n"; return 2; }
        else image = a;
    }
    if (image.empty()) { std::cerr << "usage: radslice_vliw [--max-steps N] [--k K] [--base-cycles C] image.elf\n"; return 2; }

    Bus bus;
    auto dram_owned   = std::make_unique<Dram>(128ull * 1024 * 1024);
    auto uart_owned   = std::make_unique<Uart>(std::cout);
    auto clint_owned  = std::make_unique<Clint>();
    auto syscon_owned = std::make_unique<Syscon>();
    auto plic_owned   = std::make_unique<Plic>();
    Clint* clint = clint_owned.get();
    if (!bus.attach(std::move(dram_owned)) || !bus.attach(std::move(uart_owned)) ||
        !bus.attach(std::move(clint_owned)) || !bus.attach(std::move(syscon_owned)) ||
        !bus.attach(std::move(plic_owned))) {
        std::cerr << "error: failed to build the address map\n";
        return 1;
    }
    clint->instructions_per_tick = 1;

    std::vector<u8> bytes;
    if (!read_file(image, bytes) || !is_elf(bytes)) { std::cerr << "error: cannot read an ELF image from '" << image << "'\n"; return 1; }
    const LoadedImage img = load_elf(bytes, bus);
    if (!img.ok) { std::cerr << "error: " << img.error << "\n"; return 1; }

    Cpu cpu(bus);
    cpu.pc = img.entry;
    cpu.clint = clint;
    cpu.htif_tohost_addr = img.tohost;

    std::unique_ptr<radslice::SlicedAlu> salu;
    try { salu = std::make_unique<radslice::SlicedAlu>(64, k); }
    catch (const std::exception& e) { std::cerr << "error: " << e.what() << "\n"; return 2; }
    cpu.alu_backend = salu.get();

    constexpr unsigned kMaxW = 4;
    std::vector<radslice::VliwScheduler> sched;   // index (w - 1) * 2 + (mode == Reorder)
    for (unsigned w = 1; w <= kMaxW; ++w) {
        sched.emplace_back(w, radslice::IssueMode::InOrder);
        sched.emplace_back(w, radslice::IssueMode::Reorder);
    }

    u64 retired = 0;
    while (retired < max_steps && cpu.htif_tohost_value == 0 && !cpu.halted) {
        const auto inst = cpu.fetch_inst();
        const u64  b0   = salu->stats().beats;
        const Status st = cpu.step();
        if (!st) break;
        ++retired;
        if (inst) {
            const radslice::TraceOp op =
                make_op(inst.value, static_cast<u32>(base + (salu->stats().beats - b0)));
            for (auto& s : sched) s.push(op);
        }
        // Cpu::run notices the test finishing by looking at tohost after each step; stepping by hand
        // has to do the same, or the program's idle spin after it finishes gets scheduled too.
        if (img.tohost != 0) {
            const auto r = bus.load(img.tohost, 8, AccessType::Load);
            if (r && r.value != 0) cpu.htif_tohost_value = r.value;
        }
    }
    for (auto& s : sched) s.finish();

    const bool passed = cpu.htif_tohost_value != 0 && (cpu.htif_tohost_value >> 1) == 0;
    std::cerr << (passed ? "PASS" : "STOPPED") << " after " << retired << " instruction(s)\n";
    std::cerr << "RADSLICE_VLIW k=" << k << " base_cycles=" << base << " instret=" << retired
              << " beats=" << salu->stats().beats << " blocks=" << sched[0].blocks();
    for (unsigned w = 1; w <= kMaxW; ++w)
        std::cerr << " w" << w << "_inorder=" << sched[(w - 1) * 2].cycles()
                  << " w" << w << "_reorder=" << sched[(w - 1) * 2 + 1].cycles();
    std::cerr << "\n";
    return passed ? 0 : 1;
}
