#include <memory>
#include <random>
#include <string>
#include <vector>

#include "bus.hpp"
#include "cpu.hpp"
#include "dram.hpp"
#include "radslice/sliced_alu.hpp"
#include "radslice/sliced_regfile.hpp"
#include "test_util.hpp"

// ---------------------------------------------------------------------------
// Sliced register file on its own, and the whole hardware model in lockstep
// with the golden core on random programs.
//
// "Lockstep" here means: the same instruction words run on a plain Cpu and on a
// Cpu with SlicedAlu + SlicedRegFile installed, and after every step the program
// counter and all 32 registers must match. Zero faults are injected in M2, so any
// difference is a bug in the hardware model.
// ---------------------------------------------------------------------------

using namespace radslice;

namespace {

// ---- minimal encoders (independent of the emulator's decoder) --------------

u32 rtype(u32 op, u32 rd, u32 f3, u32 rs1, u32 rs2, u32 f7) {
    return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
u32 itype(u32 op, u32 rd, u32 f3, u32 rs1, i32 imm) {
    return ((static_cast<u32>(imm) & 0xfff) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
u32 stype(u32 op, u32 f3, u32 rs1, u32 rs2, i32 imm) {
    const u32 i = static_cast<u32>(imm) & 0xfff;
    return ((i >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((i & 0x1f) << 7) | op;
}
u32 btype(u32 f3, u32 rs1, u32 rs2, i32 imm) {  // imm is an even byte offset
    const u32 i = static_cast<u32>(imm) & 0x1fff;
    return (((i >> 12) & 1) << 31) | (((i >> 5) & 0x3f) << 25) | (rs2 << 20) | (rs1 << 15) |
           (f3 << 12) | (((i >> 1) & 0xf) << 8) | (((i >> 11) & 1) << 7) | 0x63;
}
u32 lui(u32 rd, u32 imm20) { return (imm20 << 12) | (rd << 7) | 0x37; }

struct Rig {
    Bus                  bus;
    Dram*                dram = nullptr;
    std::unique_ptr<Cpu> cpu;
    explicit Rig(const std::vector<u32>& prog) {
        auto d = std::make_unique<Dram>(1024 * 1024);
        dram   = d.get();
        bus.attach(std::move(d));
        std::vector<u8> bytes;
        for (u32 w : prog)
            for (int b = 0; b < 4; ++b) bytes.push_back(static_cast<u8>(w >> (8 * b)));
        dram->load_image(DRAM_BASE, bytes);
        cpu = std::make_unique<Cpu>(bus);
    }
};

// A random program: seed registers with arbitrary 64-bit values, then a mix of
// ALU ops (all RV64I forms), loads, stores, and forward branches over one
// instruction. x30 holds a base address into DRAM and is never overwritten.
std::vector<u32> make_program(std::mt19937_64& rng, int n) {
    std::vector<u32> p;
    // x30 = scratch base. lui sign-extends bit 31, so clear the upper half again.
    p.push_back(lui(30, static_cast<u32>((DRAM_BASE + 0x8000) >> 12)));
    p.push_back(itype(0x13, 30, 1, 30, 32));    // slli x30, x30, 32
    p.push_back(itype(0x13, 30, 5, 30, 32));    // srli x30, x30, 32
    // Fill x1..x29 with random values using lui/addi/slli sequences.
    for (u32 r = 1; r <= 29; ++r) {
        p.push_back(lui(r, static_cast<u32>(rng() & 0xfffff)));
        for (int k = 0; k < 3; ++k) {
            p.push_back(itype(0x13, r, 1, r, 11 + 8 * k));                     // slli
            p.push_back(itype(0x13, r, 0, r, static_cast<i32>(rng() % 2048)));  // addi
        }
    }
    struct F { u32 op, f3, f7; };
    const F rops[] = {{0x33, 0, 0}, {0x33, 0, 0x20}, {0x33, 1, 0}, {0x33, 2, 0}, {0x33, 3, 0},
                      {0x33, 4, 0}, {0x33, 5, 0}, {0x33, 5, 0x20}, {0x33, 6, 0}, {0x33, 7, 0},
                      {0x3b, 0, 0}, {0x3b, 0, 0x20}, {0x3b, 1, 0}, {0x3b, 5, 0}, {0x3b, 5, 0x20}};
    for (int i = 0; i < n; ++i) {
        const u32 rd = 1 + rng() % 29, rs1 = 1 + rng() % 29, rs2 = 1 + rng() % 29;
        switch (rng() % 8) {
            case 0: case 1: case 2: {
                const F& f = rops[rng() % 15];
                p.push_back(rtype(f.op, rd, f.f3, rs1, rs2, f.f7));
                break;
            }
            case 3: {
                const u32 f3s[] = {0, 2, 3, 4, 6, 7};
                p.push_back(itype(0x13, rd, f3s[rng() % 6], rs1, static_cast<i32>(rng() % 4096) - 2048));
                break;
            }
            case 4: {  // slli / srli / srai / and the W immediates
                const u32 sh = rng() % 64;
                switch (rng() % 5) {
                    case 0: p.push_back(itype(0x13, rd, 1, rs1, static_cast<i32>(sh))); break;
                    case 1: p.push_back(itype(0x13, rd, 5, rs1, static_cast<i32>(sh))); break;
                    case 2: p.push_back(itype(0x13, rd, 5, rs1, static_cast<i32>(0x400 | sh))); break;
                    case 3: p.push_back(itype(0x1b, rd, 0, rs1, static_cast<i32>(rng() % 4096) - 2048)); break;
                    default: p.push_back(itype(0x1b, rd, 5, rs1, static_cast<i32>((rng() % 2 ? 0x400 : 0) | (sh & 31)))); break;
                }
                break;
            }
            case 5: {  // load from the scratch area (width f3: lb lh lw ld lbu lhu lwu)
                const u32 f3s[] = {0, 1, 2, 3, 4, 5, 6};
                const u32 f3 = f3s[rng() % 7];
                const i32 off = static_cast<i32>((rng() % 64) * 8);
                p.push_back(itype(0x03, rd, f3, 30, off));
                break;
            }
            case 6: {  // store
                const u32 f3 = rng() % 4;
                p.push_back(stype(0x23, f3, 30, rs2, static_cast<i32>((rng() % 64) * 8)));
                break;
            }
            default: {  // branch forward over exactly one instruction
                const u32 f3s[] = {0, 1, 4, 5, 6, 7};
                p.push_back(btype(f3s[rng() % 6], rs1, rs2, 8));
                p.push_back(itype(0x13, 1 + rng() % 29, 0, 1 + rng() % 29, static_cast<i32>(rng() % 100)));
                ++i;
                break;
            }
        }
    }
    return p;
}

void test_lockstep_vs_golden() {
    for (unsigned K : {8u, 16u, 32u}) {
        std::mt19937_64 rng(777 + K);
        for (int trial = 0; trial < 30; ++trial) {
            const std::vector<u32> prog = make_program(rng, 600);
            Rig golden(prog), hw(prog);
            SlicedAlu     alu(64, K);
            SlicedRegFile rf(0);
            hw.cpu->alu_backend = &alu;
            hw.cpu->regfile     = &rf;

            // Run to the end of the program (taken branches skip instructions, so
            // the step count is not simply the program length).
            const u64 end = DRAM_BASE + 4 * prog.size();
            u64 executed = 0;
            bool ok = true;
            while (ok && golden.cpu->pc < end) {
                const Status a = golden.cpu->step();
                const Status b = hw.cpu->step();
                ++executed;
                ok = (a.ok == b.ok) && golden.cpu->pc == hw.cpu->pc;
                for (u32 r = 0; r < 32 && ok; ++r) ok = golden.cpu->read_reg(r) == hw.cpu->read_reg(r);
                CHECK_MSG(ok, "K=" + std::to_string(K) + " trial=" + std::to_string(trial) +
                                  " diverged at step " + std::to_string(executed) + " pc=" +
                                  tu::hex(golden.cpu->pc));
                CHECK_MSG(a.ok, "the golden core trapped, so this test program is invalid: step " +
                                    std::to_string(executed));
                if (!a.ok) break;
            }
            CHECK(executed > 700);   // it really ran the program, not a prefix
            CHECK(rf.stats().parity_errors == 0);   // zero faults, zero parity events
            CHECK(alu.stats().ops > 0 && alu.stats().beats >= alu.stats().ops);
        }
    }
}

// ---- the register file by itself -------------------------------------------

void test_regfile_roundtrip_and_parity() {
    SlicedRegFile rf(16);
    std::mt19937_64 rng(5);
    std::array<u64, 32> shadow{};
    for (int i = 0; i < 20000; ++i) {
        const u32 r = 1 + rng() % 31;
        if (rng() % 2) {
            shadow[r] = (rng() % 4 == 0) ? (rng() % 2 ? ~0ull : 0) : rng();
            rf.write(r, shadow[r]);
        } else {
            CHECK(rf.read(r) == shadow[r]);
        }
    }
    CHECK(rf.stats().parity_errors == 0);
    CHECK(rf.total_slices() == 256 + 16);
}

void test_parity_detects_a_flipped_bit() {
    SlicedRegFile rf(0);
    rf.write(5, 0x0123'4567'89ab'cdefull);
    CHECK(rf.read(5) == 0x0123'4567'89ab'cdefull);
    CHECK(rf.stats().parity_errors == 0);
    rf.physical(5 * 8 + 3) ^= 0x10;                  // flip one stored bit
    (void)rf.read(5);
    CHECK(rf.stats().parity_errors == 1);            // detected, not corrected
    rf.write(5, 0x0123'4567'89ab'cdefull);           // a rewrite restores consistency
    (void)rf.read(5);
    CHECK(rf.stats().parity_errors == 1);
    // A double flip in one word is NOT detected by single-bit parity.
    rf.physical(5 * 8 + 0) ^= 0x01;
    rf.physical(5 * 8 + 7) ^= 0x80;
    (void)rf.read(5);
    CHECK(rf.stats().parity_errors == 1);
}

void test_remap_redirects_a_slice_to_a_spare() {
    SlicedRegFile rf(8);
    rf.set_remap(7 * 8 + 2, 256 + 3);               // word 7, byte 2 -> spare 3
    rf.write(7, 0x1122'3344'5566'7788ull);
    CHECK(rf.read(7) == 0x1122'3344'5566'7788ull);
    CHECK(rf.physical(256 + 3) == 0x66);            // byte 2 lives in the spare
    rf.physical(7 * 8 + 2) = 0xee;                  // the abandoned primary is ignored
    CHECK(rf.read(7) == 0x1122'3344'5566'7788ull);
    CHECK(rf.stats().parity_errors == 0);
}

}  // namespace

int main() {
    test_regfile_roundtrip_and_parity();
    test_parity_detects_a_flipped_bit();
    test_remap_redirects_a_slice_to_a_spare();
    test_lockstep_vs_golden();
    return tu::summary("hw_lockstep");
}
