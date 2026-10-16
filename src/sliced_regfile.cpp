#include "radslice/sliced_regfile.hpp"

#include <numeric>

namespace radslice {

SlicedRegFile::SlicedRegFile(unsigned spare_slices)
    : phys_(kPrimarySlices + spare_slices, 0), remap_(kPrimarySlices) {
    std::iota(remap_.begin(), remap_.end(), 0u);   // identity until M3 remaps
}

u8 SlicedRegFile::parity64(u64 v) {
    v ^= v >> 32;
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return static_cast<u8>(v & 1);
}

u64 SlicedRegFile::read(u32 index) {
    ++stats_.reads;
    u64 v = 0;
    for (unsigned s = 0; s < kSlicesPerWord; ++s)
        v |= static_cast<u64>(phys_[remap_[index * kSlicesPerWord + s]]) << (8 * s);
    if (parity64(v) != parity_[index]) ++stats_.parity_errors;
    return v;
}

void SlicedRegFile::write(u32 index, u64 value) {
    ++stats_.writes;
    for (unsigned s = 0; s < kSlicesPerWord; ++s)
        phys_[remap_[index * kSlicesPerWord + s]] = static_cast<u8>(value >> (8 * s));
    parity_[index] = parity64(value);
}

}  // namespace radslice
