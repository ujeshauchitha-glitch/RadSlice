// radslice_run: run a bare-metal ELF on the golden core with the RadSlice
// hardware model (sliced ALU, sliced register file) installed beneath it.
//
// It prints the same PASS/FAIL lines as riscv-emu's own CLI, so riscv-emu's
// scripts/run-riscv-tests.sh can drive it unchanged:
//
//   EMU=/path/to/radslice_run RADSLICE_K=16 scripts/run-riscv-tests.sh rv64ui rv64mi
//
// Configuration (environment, so the script needs no changes):
//   RADSLICE_ALU   sliced | plain          default sliced
//   RADSLICE_K     8 | 16 | 32             default 16
//   RADSLICE_RF    sliced | plain | none   default sliced
//   RADSLICE_BASE_CYCLES  cycles per instruction outside the ALU    default 1
//
// The cycle numbers are MODEL ESTIMATES (see docs/results.md), not measurements.

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
#include "radslice/sliced_regfile.hpp"
#include "syscon.hpp"
#include "uart.hpp"

namespace {

std::string env_or(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : fallback;
}

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
    u64         max_steps = 100'000'000;
    std::string image;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--max-steps" && i + 1 < argc) max_steps = std::strtoull(argv[++i], nullptr, 0);
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "usage: radslice_run [--max-steps N] image.elf\n";
            return 2;
        } else image = a;
    }
    if (image.empty()) {
        std::cerr << "usage: radslice_run [--max-steps N] image.elf\n";
        return 2;
    }

    const std::string alu_kind = env_or("RADSLICE_ALU", "sliced");
    const std::string rf_kind  = env_or("RADSLICE_RF", "sliced");
    const unsigned    k        = static_cast<unsigned>(std::atoi(env_or("RADSLICE_K", "16").c_str()));
    const u64 base_cycles      = std::strtoull(env_or("RADSLICE_BASE_CYCLES", "1").c_str(), nullptr, 0);

    // The same address map as riscv-emu's CLI, minus the block device.
    Bus bus;
    auto dram_owned   = std::make_unique<Dram>(128ull * 1024 * 1024);
    auto uart_owned   = std::make_unique<Uart>(std::cout);
    auto clint_owned  = std::make_unique<Clint>();
    auto syscon_owned = std::make_unique<Syscon>();
    auto plic_owned   = std::make_unique<Plic>();
    Uart*   uart   = uart_owned.get();
    Clint*  clint  = clint_owned.get();
    Syscon* syscon = syscon_owned.get();
    Plic*   plic   = plic_owned.get();
    if (!bus.attach(std::move(dram_owned)) || !bus.attach(std::move(uart_owned)) ||
        !bus.attach(std::move(clint_owned)) || !bus.attach(std::move(syscon_owned)) ||
        !bus.attach(std::move(plic_owned))) {
        std::cerr << "error: failed to build the address map\n";
        return 1;
    }
    clint->instructions_per_tick = 1;

    std::vector<u8> bytes;
    if (!read_file(image, bytes) || !is_elf(bytes)) {
        std::cerr << "error: cannot read an ELF image from '" << image << "'\n";
        return 1;
    }
    const LoadedImage img = load_elf(bytes, bus);
    if (!img.ok) {
        std::cerr << "error: " << img.error << "\n";
        return 1;
    }

    Cpu cpu(bus);
    cpu.pc               = img.entry;
    cpu.clint            = clint;
    cpu.syscon           = syscon;
    cpu.htif_tohost_addr = img.tohost;
    cpu.plic             = plic;
    cpu.uart             = uart;
    cpu.uart_irq         = UART0_IRQ;

    std::unique_ptr<radslice::SlicedAlu>     salu;
    std::unique_ptr<radslice::SlicedRegFile> srf;
    std::unique_ptr<radslice::PlainRegFile>  prf;
    try {
        if (alu_kind == "sliced") {
            salu = std::make_unique<radslice::SlicedAlu>(64, k);
            cpu.alu_backend = salu.get();
        } else if (alu_kind != "plain") {
            std::cerr << "error: RADSLICE_ALU must be sliced or plain\n";
            return 2;
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
    if (rf_kind == "sliced") {
        srf = std::make_unique<radslice::SlicedRegFile>(0);
        cpu.regfile = srf.get();
    } else if (rf_kind == "plain") {
        prf = std::make_unique<radslice::PlainRegFile>();
        cpu.regfile = prf.get();
    } else if (rf_kind != "none") {
        std::cerr << "error: RADSLICE_RF must be sliced, plain or none\n";
        return 2;
    }

    u64    retired = 0;
    Status st      = cpu.run(max_steps, &retired);

    int rc = 0;
    if (cpu.htif_tohost_value != 0) {
        const u64 code = cpu.htif_tohost_value >> 1;
        if (code == 0) std::cerr << "\nPASS after " << retired << " instruction(s)\n";
        else { std::cerr << "\nFAIL: test " << code << " failed, after " << retired << " instruction(s)\n"; rc = 1; }
    } else if (cpu.halted) {
        std::cerr << "\nmachine powered off after " << retired << " instruction(s)\n";
    } else if (!st) {
        std::cerr << "\nstopped after " << retired << " instruction(s): "
                  << exception_name(st.trap.cause) << " at pc 0x" << std::hex << cpu.pc
                  << std::dec << "\n";
        rc = 1;
    } else {
        std::cerr << "\nstep budget exhausted after " << retired << " instruction(s)\n";
        rc = 1;
    }

    // One machine-readable line. Model estimates, not measurements.
    const u64 alu_ops = salu ? salu->stats().ops : 0;
    const u64 beats   = salu ? salu->stats().beats : 0;
    std::cerr << "RADSLICE_STATS alu=" << alu_kind << " k=" << (salu ? k : 64)
              << " rf=" << rf_kind << " instret=" << retired << " alu_ops=" << alu_ops
              << " beats=" << beats << " base_cycles=" << base_cycles;
    if (srf) std::cerr << " rf_parity_errors=" << srf->stats().parity_errors;
    std::cerr << "\n";
    return rc;
}
