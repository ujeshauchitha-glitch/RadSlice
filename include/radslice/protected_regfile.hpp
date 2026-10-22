#pragma once

#include <memory>
#include <vector>

#include "radslice/sliced_regfile.hpp"

namespace radslice {

// ---------------------------------------------------------------------------
// Register-file protection against transient upsets (M4).
//
// Every copy is a complete SlicedRegFile, so the M3 permanent-defect machinery (stuck
// slices, spares, remap) composes with this: each copy is tested and repaired
// separately. The scheme decides how many copies there are and what a read does.
//
//   None       1 copy. Parity ignored. The unprotected sliced core.
//   Parity     1 copy + parity. A parity mismatch on a read is detected but cannot
//              be corrected: the machine fail-stops.
//   ParityDup  2 copies + parity each. A copy with bad parity is repaired from the
//              other. Both bad, or both parity-clean but different (an even-weight
//              upset), is detected and fail-stops.
//   Tmr        3 copies, bitwise majority vote on every read; the dissenting copy is
//              rewritten. No parity.
//
// A background scrubber reads one word at a time, round-robin, through the same
// path and repairs what it finds, so single upsets do not accumulate into
// uncorrectable ones. Scrubbing a Parity-only file can only detect, which halts the
// machine on an upset in a register that may never have been read again; that
// cost is real and is reported, not hidden.
//
// Assumptions (tier C): the vote, compare and parity logic is fault-free, detection
// and correction take no model cycles, and x0 is not stored.
// ---------------------------------------------------------------------------

enum class RfProtection { None, Parity, ParityDup, Tmr };

const char* to_string(RfProtection m);

struct RfProtStats {
    u64 reads = 0, writes = 0;
    u64 detected = 0;           // reads/scrubs that found a disagreement or bad parity
    u64 corrected = 0;          // ... and repaired it
    u64 uncorrectable = 0;      // ... and could not (the machine fail-stops)
    u64 scrubbed_words = 0;
    u64 scrub_corrections = 0;  // corrections made by the scrubber rather than a read
};

class ProtectedRegFile : public RegFileBackend {
public:
    explicit ProtectedRegFile(RfProtection mode, unsigned spare_slices_per_copy = 0);

    u64  read(u32 index) override;
    void write(u32 index, u64 value) override;

    RfProtection mode() const { return mode_; }
    unsigned     copies() const { return static_cast<unsigned>(copies_.size()); }
    SlicedRegFile& copy(unsigned c) { return *copies_.at(c); }

    // What an ordinary read would return, with no side effects on state or statistics.
    u64 peek(u32 index);

    // --- transient faults ---
    // Bits exposed to upsets: every stored data bit of words 1..31 in every copy,
    // plus the parity bit where the scheme uses one.
    u64  exposed_bits() const;
    void inject(u64 exposed_bit_index);          // flip the bit with this index
    // A multi-bit upset: `burst` adjacent bits of the same word and copy, starting at this index and
    // clipped at the end of the word (data bits then the parity bit, where there is one).
    void inject_burst(u64 exposed_bit_index, unsigned burst);
    void flip_data(unsigned copy, u32 word, unsigned bit) { copy_(copy).flip_bit(word, bit); }
    void flip_parity(unsigned copy, u32 word) { copy_(copy).flip_parity(word); }

    // --- scrubbing ---
    void scrub_one();                            // scrub the next word (1..31, round-robin)

    // --- in-field repair (G15) ---
    // Re-run the boot self-test and remap on every copy while the machine is running. The word
    // contents are saved first (through the protection scheme, so a vote or a parity-based repair
    // happens as part of it), the remap is rebuilt from the identity, and the words are written back.
    // Returns false if some copy no longer has enough good slices; the machine then fail-stops.
    bool retest();
    u64  total_slices_all_copies() const;

    bool fail_stop() const { return fail_stop_; }
    const RfProtStats& stats() const { return stats_; }

private:
    SlicedRegFile& copy_(unsigned c) { return *copies_.at(c); }
    bool uses_parity() const { return mode_ == RfProtection::Parity || mode_ == RfProtection::ParityDup; }
    u64  checked_read(u32 index, bool scrubbing);

    RfProtection mode_;
    std::vector<std::unique_ptr<SlicedRegFile>> copies_;
    RfProtStats stats_;
    bool        fail_stop_ = false;
    u32         scrub_next_ = 1;
};

}  // namespace radslice
