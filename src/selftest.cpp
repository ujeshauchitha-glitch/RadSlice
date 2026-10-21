#include "radslice/selftest.hpp"

#include <vector>

namespace radslice {

RepairReport regfile_self_test(SlicedRegFile& rf, unsigned rounds, unsigned domains) {
    RepairReport rep;
    const unsigned total = rf.total_slices();
    const unsigned prim  = SlicedRegFile::kPrimarySlices;
    std::vector<bool> bad(total, false);

    static const u8 kPatterns[] = {0x00, 0xff, 0xaa, 0x55};
    for (unsigned r = 0; r < rounds; ++r)
        for (u8 pat : kPatterns)
            for (unsigned p = 0; p < total; ++p) {
                rf.write_physical(p, pat);
                if (rf.read_physical(p) != pat) bad[p] = true;
            }
    for (unsigned p = 0; p < total; ++p) rf.write_physical(p, 0);

    const unsigned spares = total - prim;
    if (domains == 0 || prim % domains != 0 || (domains > 1 && (spares == 0 || spares % domains != 0))) {
        rep.ok = false;                                  // an invalid sparing layout cannot repair anything
        return rep;
    }
    const unsigned per_domain_primaries = prim / domains;
    const unsigned per_domain_spares    = domains > 1 ? spares / domains : spares;

    // good spares, grouped by the region they serve (one group for the ideal pool)
    std::vector<std::vector<unsigned>> good_spares(domains);
    for (unsigned p = prim; p < total; ++p) {
        if (bad[p]) {
            ++rep.bad_spare;
        } else {
            const unsigned dom = domains > 1 ? (p - prim) / per_domain_spares : 0;
            good_spares[dom].push_back(p);
        }
    }
    std::vector<std::size_t> next(domains, 0);
    for (unsigned p = 0; p < prim; ++p) {
        if (!bad[p]) continue;
        ++rep.bad_primary;
        const unsigned dom = domains > 1 ? p / per_domain_primaries : 0;
        if (next[dom] < good_spares[dom].size()) {
            rf.set_remap(p, good_spares[dom][next[dom]++]);
            ++rep.remapped;
        } else {
            rep.ok = false;
        }
    }
    return rep;
}

namespace {
struct Vec { SliceOp op; u64 x, y, aux; };
}

RepairReport alu_self_test(DefectiveSlicedAlu& alu, unsigned rounds) {
    RepairReport rep;
    const unsigned k = alu.slice_bits();
    const u64 m = (k >= 64) ? ~0ull : ((1ull << k) - 1);
    const u64 A = 0xaaaa'aaaa'aaaa'aaaaull & m, B = 0x5555'5555'5555'5555ull & m;
    const u64 sh = k > 1 ? 1 : 0;
    // Chosen so that, between them, every result bit and the carry take both
    // values on at least one vector.
    const std::vector<Vec> vecs = {
        {SliceOp::Add, 0, 0, 0},      {SliceOp::Add, m, 0, 0},   {SliceOp::Add, m, m, 1},
        {SliceOp::Add, 0, m, 1},      {SliceOp::Add, A, B, 0},   {SliceOp::Add, B, B, 0},
        {SliceOp::Xor, A, B, 0},      {SliceOp::And, m, m, 0},   {SliceOp::Or, 0, 0, 0},
        {SliceOp::FunnelL, m, m, sh}, {SliceOp::FunnelR, m, m, sh},
    };

    const unsigned total = alu.physical_units();
    const unsigned need  = alu.num_slices();
    std::vector<bool> bad(total, false);
    for (unsigned r = 0; r < rounds; ++r)
        for (unsigned u = 0; u < total; ++u)
            for (const Vec& v : vecs) {
                const SliceOut got = alu.unit_exec(u, v.op, v.x, v.y, v.aux);
                const SliceOut exp = alu.ideal_slice(v.op, v.x, v.y, v.aux);
                const bool carry_matters = v.op == SliceOp::Add;
                if (got.value != exp.value || (carry_matters && got.carry != exp.carry))
                    bad[u] = true;
            }

    std::vector<unsigned> good;
    for (unsigned u = 0; u < total; ++u) {
        if (bad[u]) {
            if (u < need) ++rep.bad_primary;
            else ++rep.bad_spare;
        } else {
            good.push_back(u);
        }
    }
    if (good.size() < need) {
        rep.ok = false;
        return rep;
    }
    for (unsigned i = 0; i < need; ++i) {
        if (alu.mapping(i) != good[i]) ++rep.remapped;
        alu.set_mapping(i, good[i]);
    }
    return rep;
}

}  // namespace radslice
