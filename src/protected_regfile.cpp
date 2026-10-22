#include "radslice/protected_regfile.hpp"

#include "radslice/selftest.hpp"

namespace radslice {

const char* to_string(RfProtection m) {
    switch (m) {
        case RfProtection::None:      return "none";
        case RfProtection::Parity:    return "parity";
        case RfProtection::ParityDup: return "parity-dup";
        case RfProtection::Tmr:       return "tmr";
    }
    return "?";
}

ProtectedRegFile::ProtectedRegFile(RfProtection mode, unsigned spare_slices_per_copy)
    : mode_(mode) {
    unsigned n = 1;
    if (mode == RfProtection::ParityDup) n = 2;
    if (mode == RfProtection::Tmr) n = 3;
    for (unsigned i = 0; i < n; ++i)
        copies_.push_back(std::make_unique<SlicedRegFile>(spare_slices_per_copy));
}

void ProtectedRegFile::write(u32 index, u64 value) {
    ++stats_.writes;
    for (auto& c : copies_) c->write(index, value);
}

u64 ProtectedRegFile::read(u32 index) {
    ++stats_.reads;
    return checked_read(index, false);
}

// One read through the protection scheme. `scrubbing` only changes which counters
// move; the repair logic is identical, as it is in hardware.
u64 ProtectedRegFile::checked_read(u32 index, bool scrubbing) {
    auto note_detect = [&] { ++stats_.detected; };
    auto note_fix    = [&] { ++stats_.corrected; if (scrubbing) ++stats_.scrub_corrections; };
    auto note_fail   = [&] { ++stats_.uncorrectable; fail_stop_ = true; };

    switch (mode_) {
        case RfProtection::None: {
            bool ok;
            return copy_(0).read_raw(index, ok);
        }
        case RfProtection::Parity: {
            bool ok;
            const u64 v = copy_(0).read_raw(index, ok);
            if (!ok) { note_detect(); note_fail(); }
            return v;
        }
        case RfProtection::ParityDup: {
            bool oka, okb;
            const u64 a = copy_(0).read_raw(index, oka);
            const u64 b = copy_(1).read_raw(index, okb);
            if (oka && okb) {
                if (a == b) return a;
                note_detect(); note_fail();            // even-weight upset in one copy
                return a;
            }
            if (oka) {                                  // copy 1 is bad: repair it from copy 0
                note_detect(); note_fix();
                copy_(1).write(index, a);
                return a;
            }
            if (okb) {
                note_detect(); note_fix();
                copy_(0).write(index, b);
                return b;
            }
            note_detect(); note_fail();                 // both copies bad
            return a;
        }
        case RfProtection::Tmr: {
            bool ok;
            const u64 a = copy_(0).read_raw(index, ok);
            const u64 b = copy_(1).read_raw(index, ok);
            const u64 c = copy_(2).read_raw(index, ok);
            const u64 m = (a & b) | (a & c) | (b & c);
            if (a != m || b != m || c != m) {
                note_detect(); note_fix();
                if (a != m) copy_(0).write(index, m);
                if (b != m) copy_(1).write(index, m);
                if (c != m) copy_(2).write(index, m);
            }
            return m;
        }
    }
    return 0;
}

u64 ProtectedRegFile::peek(u32 index) {
    switch (mode_) {
        case RfProtection::None:
        case RfProtection::Parity:
            return copy_(0).peek(index);
        case RfProtection::ParityDup:
            return copy_(0).parity_ok(index) ? copy_(0).peek(index) : copy_(1).peek(index);
        case RfProtection::Tmr: {
            const u64 a = copy_(0).peek(index), b = copy_(1).peek(index), c = copy_(2).peek(index);
            return (a & b) | (a & c) | (b & c);
        }
    }
    return 0;
}

u64 ProtectedRegFile::exposed_bits() const {
    const u64 per_word = 64 + (uses_parity() ? 1 : 0);
    return static_cast<u64>(copies_.size()) * 31 * per_word;
}

void ProtectedRegFile::inject(u64 bit_index) {
    const u64 per_word = 64 + (uses_parity() ? 1 : 0);
    const u64 per_copy = 31 * per_word;
    const unsigned c   = static_cast<unsigned>(bit_index / per_copy);
    const u64 within   = bit_index % per_copy;
    const u32 word     = static_cast<u32>(1 + within / per_word);
    const unsigned bit = static_cast<unsigned>(within % per_word);
    if (bit < 64) copy_(c).flip_bit(word, bit);
    else copy_(c).flip_parity(word);
}

void ProtectedRegFile::inject_burst(u64 bit_index, unsigned burst) {
    const u64 per_word = 64 + (uses_parity() ? 1 : 0);
    const u64 within   = bit_index % per_word;
    for (unsigned i = 0; i < burst && within + i < per_word; ++i) inject(bit_index + i);
}

u64 ProtectedRegFile::total_slices_all_copies() const {
    u64 n = 0;
    for (const auto& c : copies_) n += c->total_slices();
    return n;
}

bool ProtectedRegFile::retest() {
    u64 saved[SlicedRegFile::kWords];
    for (u32 w = 0; w < SlicedRegFile::kWords; ++w) saved[w] = peek(w);
    bool ok = true;
    for (auto& c : copies_) {
        for (unsigned l = 0; l < SlicedRegFile::kPrimarySlices; ++l) c->set_remap(l, l);
        ok = regfile_self_test(*c, 1).ok && ok;
        for (u32 w = 0; w < SlicedRegFile::kWords; ++w) c->write(w, saved[w]);
    }
    return ok;
}

void ProtectedRegFile::scrub_one() {
    if (mode_ == RfProtection::None) return;
    ++stats_.scrubbed_words;
    (void)checked_read(scrub_next_, true);
    scrub_next_ = (scrub_next_ % 31) + 1;
}

}  // namespace radslice
