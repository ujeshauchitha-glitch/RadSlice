// radslice_rf_vectors: random operation sequences for the Verilog register file (rtl/sliced_regfile.v),
// with the answers taken from the model's SlicedRegFile.
//
//   radslice_rf_vectors SPARES COUNT SEED > vectors.txt
//
// One line per operation: "op a b c" (hex). Ops:
//   0 write      a=word b=value
//   1 read       a=word            -> b=value read, c=1 if the parity matched
//   2 defect     a=physical b=bit c=value (stuck bit)      3 remap  a=logical b=physical
//   4 flip       a=word b=bit                              5 phys read  a=physical -> b=value
//   6 phys write a=physical b=value                        7 clear defect a=physical
#include <cstdio>
#include <cstdlib>

#include "radslice/sliced_regfile.hpp"

int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "usage: radslice_rf_vectors SPARES COUNT SEED\n"); return 2; }
    const unsigned spares = static_cast<unsigned>(std::atoi(argv[1]));
    const unsigned count  = static_cast<unsigned>(std::atoi(argv[2]));
    u64 state             = std::strtoull(argv[3], nullptr, 0) | 1;
    const auto rnd = [&state] { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };

    radslice::SlicedRegFile rf(spares);
    const unsigned np = radslice::SlicedRegFile::kPrimarySlices + spares;
    const auto pr = [](unsigned op, u64 a, u64 b, u64 c) {
        std::printf("%x %llx %llx %llx\n", op, (unsigned long long)a, (unsigned long long)b, (unsigned long long)c);
    };
    for (unsigned i = 0; i < count; ++i) {
        const unsigned kind = static_cast<unsigned>(rnd() % 100);
        const u32 w         = static_cast<u32>(rnd() % 32);
        if (kind < 30) { const u64 v = rnd(); rf.write(w, v); pr(0, w, v, 0); }
        else if (kind < 60) { bool ok = false; const u64 v = rf.read_raw(w, ok); pr(1, w, v, ok ? 1 : 0); }
        else if (kind < 66) {
            const unsigned p = static_cast<unsigned>(rnd() % np), bit = static_cast<unsigned>(rnd() % 8);
            radslice::SliceDefect d; d.kind = radslice::DefectKind::StuckBit; d.bit = bit; d.value = rnd() & 1;
            rf.set_defect(p, d); pr(2, p, bit, d.value ? 1 : 0);
        } else if (kind < 72) {
            const unsigned l = static_cast<unsigned>(rnd() % radslice::SlicedRegFile::kPrimarySlices);
            const unsigned p = static_cast<unsigned>(rnd() % np);
            rf.set_remap(l, p); pr(3, l, p, 0);
        } else if (kind < 80) { const unsigned bit = static_cast<unsigned>(rnd() % 64); rf.flip_bit(w, bit); pr(4, w, bit, 0); }
        else if (kind < 88) { const unsigned p = static_cast<unsigned>(rnd() % np); pr(5, p, rf.read_physical(p), 0); }
        else if (kind < 94) { const unsigned p = static_cast<unsigned>(rnd() % np); const u8 v = static_cast<u8>(rnd()); rf.write_physical(p, v); pr(6, p, v, 0); }
        else { const unsigned p = static_cast<unsigned>(rnd() % np); rf.set_defect(p, radslice::SliceDefect{}); pr(7, p, 0, 0); }
    }
    return 0;
}
