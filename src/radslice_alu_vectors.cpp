// radslice_alu_vectors: write test vectors for the Verilog ALU (rtl/sliced_alu.v).
//
//   radslice_alu_vectors K COUNT SEED > vectors.txt
//
// One line per vector: op a b expected beats, all hex except beats. `expected` comes from the model's
// SlicedAlu, which is itself checked here against the golden core's alu_reference, so a disagreement
// between the hardware and a vector is a disagreement between the hardware and the golden model.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "radslice/sliced_alu.hpp"

int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "usage: radslice_alu_vectors K COUNT SEED\n"); return 2; }
    const unsigned k     = static_cast<unsigned>(std::atoi(argv[1]));
    const unsigned count = static_cast<unsigned>(std::atoi(argv[2]));
    u64 state            = std::strtoull(argv[3], nullptr, 0) | 1;
    const auto rnd = [&state] { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };

    radslice::SlicedAlu alu(64, k);
    const std::vector<u64> edge = {0, 1, 2, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000, 0xffff, 0x7fffffff,
                                   0x80000000ull, 0xffffffffull, 0x100000000ull, 0x7fffffffffffffffull,
                                   0x8000000000000000ull, 0xffffffffffffffffull, 0x00000000ffff0000ull,
                                   0xfffffffe00000001ull, 63, 64, 31, 32};
    unsigned bad = 0;
    const auto emit = [&](unsigned op, u64 a, u64 b) {
        const AluOp o   = static_cast<AluOp>(op);
        const u64   got = alu.exec(o, a, b);
        if (got != alu_reference(o, a, b)) ++bad;
        std::printf("%x %016llx %016llx %016llx %u\n", op, static_cast<unsigned long long>(a),
                    static_cast<unsigned long long>(b), static_cast<unsigned long long>(got),
                    radslice::SlicedAlu::beats_for(o, 64, k));
    };
    for (unsigned op = 0; op < 15; ++op)
        for (u64 a : edge)
            for (u64 b : edge) emit(op, a, b);
    for (unsigned i = 0; i < count; ++i) emit(rnd() % 15, rnd(), (i & 3) == 0 ? rnd() % 70 : rnd());
    if (bad) { std::fprintf(stderr, "error: %u vectors differ from the golden alu_reference\n", bad); return 1; }
    return 0;
}
