#pragma once

#include <array>
#include <vector>

#include "backends.hpp"  // riscv-emu: RegFileBackend, u8/u32/u64
#include "radslice/defects.hpp"

namespace radslice {

// ---------------------------------------------------------------------------
// The register file as physical byte slices.
//
// 32 words x 8 byte-slices = 256 primary slices, plus a shared pool of spares.
// A logical slice (word w, byte b) reaches its physical slice through a remap
// table, which starts as the identity. Each word carries one parity bit,
// written with the word and checked on every read.
//
// In M2 there are no faults, so the remap stays the identity and parity never
// fails; both exist so that M3 (permanent defects, remap) and M4 (transient
// faults, parity) have something to act on. A parity mismatch is counted and
// reported, never silently repaired: recovery is a later milestone.
//
// x0 is never stored. The CPU hardwires it, so the model allocates its word only
// to keep the slice count at the 256 used by the yield model (an overstatement of
// one word, recorded in config/params.toml).
// ---------------------------------------------------------------------------

class SlicedRegFile : public RegFileBackend {
public:
    static constexpr unsigned kWords          = 32;
    static constexpr unsigned kSlicesPerWord  = 8;   // byte slices
    static constexpr unsigned kPrimarySlices  = kWords * kSlicesPerWord;

    explicit SlicedRegFile(unsigned spare_slices = 0);

    u64  read(u32 index) override;
    void write(u32 index, u64 value) override;

    struct Stats {
        u64 reads = 0, writes = 0;
        u64 parity_errors = 0;   // reads whose stored parity disagreed with the data
    };
    const Stats& stats() const { return stats_; }

    // Access for fault injection and self-test (M3/M4); unused with zero faults.
    unsigned total_slices() const { return static_cast<unsigned>(phys_.size()); }
    u8&      physical(unsigned p) { return phys_.at(p); }   // raw cell, bypasses defects
    unsigned remap(unsigned logical) const { return remap_.at(logical); }
    void     set_remap(unsigned logical, unsigned physical) { remap_.at(logical) = physical; }

    // Permanent defects (M3). A defective physical slice returns data with a stuck
    // bit, whatever was written. read_physical/write_physical bypass the remap and
    // are what the boot self-test uses to examine one slice at a time.
    void           set_defect(unsigned physical, const SliceDefect& d);
    const SliceDefect& defect(unsigned physical) const { return defects_.at(physical); }
    void           seed_defects(u64 seed) { rng_ = Rng(seed); }
    u8             read_physical(unsigned p);
    void           write_physical(unsigned p, u8 value) { phys_.at(p) = value; }

private:
    static u8 parity64(u64 v);

    std::vector<u8>       phys_;     // physical slices: primaries, then spares
    std::vector<SliceDefect> defects_;
    bool                  any_defect_ = false;
    Rng                   rng_{1};
    std::vector<unsigned> remap_;    // logical slice -> physical slice
    std::array<u8, kWords> parity_{};
    Stats                 stats_;
};

// The fault-free baseline for the hybrid comparison: an ordinary register file
// (silicon, in the hybrid story) beneath MoS2-style logic.
class PlainRegFile : public RegFileBackend {
public:
    u64  read(u32 index) override { return r_[index % 32]; }
    void write(u32 index, u64 value) override { r_[index % 32] = value; }

private:
    std::array<u64, 32> r_{};
};

}  // namespace radslice
