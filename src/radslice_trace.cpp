// radslice_trace: run a bare-metal RV64I ELF on the golden core and print, for every retired instruction, what the
// hardware must also do. The Verilog core (rtl/rv64i_core.v) is compared with this line by line.
//
//   radslice_trace [--max-steps N] image.elf > trace.txt
//
// One line per instruction, hex: pc inst rd rdval st staddr stsize stval nextpc
//   rd/rdval   register written (0 if none, including writes to x0) and its new value
//   st         1 if the instruction is a store; staddr, stsize (log2 bytes) and stval (full register, the
//              reader masks it to the size)
// The run stops when the program writes a non-zero value to tohost; the last line is that store.
#include <cstdio>
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
}  // namespace

int main(int argc, char** argv) {
    u64 max_steps = 50'000'000;
    std::string image;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--max-steps" && i + 1 < argc) max_steps = std::strtoull(argv[++i], nullptr, 0);
        else if (!a.empty() && a[0] == '-') { std::cerr << "usage: radslice_trace [--max-steps N] image.elf\n"; return 2; }
        else image = a;
    }
    if (image.empty()) { std::cerr << "usage: radslice_trace [--max-steps N] image.elf\n"; return 2; }

    Bus bus;
    auto dram_owned = std::make_unique<Dram>(128ull * 1024 * 1024);
    auto uart_owned = std::make_unique<Uart>(std::cerr);
    auto clint_owned = std::make_unique<Clint>();
    auto syscon_owned = std::make_unique<Syscon>();
    auto plic_owned = std::make_unique<Plic>();
    Clint* clint = clint_owned.get();
    if (!bus.attach(std::move(dram_owned)) || !bus.attach(std::move(uart_owned)) || !bus.attach(std::move(clint_owned)) ||
        !bus.attach(std::move(syscon_owned)) || !bus.attach(std::move(plic_owned))) { std::cerr << "error: address map\n"; return 1; }
    clint->instructions_per_tick = 1;
    std::vector<u8> bytes;
    if (!read_file(image, bytes) || !is_elf(bytes)) { std::cerr << "error: cannot read an ELF image from '" << image << "'\n"; return 1; }
    const LoadedImage img = load_elf(bytes, bus);
    if (!img.ok) { std::cerr << "error: " << img.error << "\n"; return 1; }

    Cpu cpu(bus);
    cpu.pc = img.entry;
    cpu.clint = clint;
    cpu.htif_tohost_addr = img.tohost;

    u64 n = 0;
    bool finished = false;
    while (n < max_steps && !finished) {
        const auto inst = cpu.fetch_inst();
        if (!inst) { std::cerr << "error: fetch trapped at pc 0x" << std::hex << cpu.pc << "\n"; return 1; }
        const DecodedInst d = inst.value;
        const u64 pc0 = cpu.pc;
        const u64 a = cpu.read_reg(d.rs1), b = cpu.read_reg(d.rs2);
        const Status st = cpu.step();
        if (!st) { std::cerr << "error: trap at pc 0x" << std::hex << pc0 << "\n"; return 1; }
        ++n;
        using namespace opcodes;
        u32 rd = 0; u64 rdval = 0;
        switch (d.opcode) {
            case LUI: case AUIPC: case JAL: case JALR: case LOAD: case OP_IMM: case OP: case OP_IMM_32: case OP_32:
                if (d.rd != 0) { rd = d.rd; rdval = cpu.read_reg(d.rd); }
                break;
            default: break;
        }
        const bool store = d.opcode == STORE;
        std::printf("%llx %08x %x %llx %d %llx %u %llx %llx\n", (unsigned long long)pc0, d.raw, rd, (unsigned long long)rdval,
                    store ? 1 : 0, (unsigned long long)(store ? a + static_cast<u64>(d.imm) : 0), store ? (d.funct3 & 3) : 0,
                    (unsigned long long)(store ? b : 0), (unsigned long long)cpu.pc);
        if (img.tohost != 0) {
            const auto r = bus.load(img.tohost, 8, AccessType::Load);
            if (r && r.value != 0) finished = true;
        }
    }
    std::fprintf(stderr, "%s after %llu instruction(s), tohost at 0x%llx\n", finished ? "finished" : "step budget exhausted",
                 (unsigned long long)n, (unsigned long long)img.tohost);
    return finished ? 0 : 1;
}
