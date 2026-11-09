#ifndef RUSH2049_MEM_SEARCH_H
#define RUSH2049_MEM_SEARCH_H

// A Cheat Engine style search of RDRAM for an unknown value (src/mem_search.cpp): start with every aligned address
// as a candidate, then keep only those whose value changed, stayed, went up or went down since the last step.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rush2049 {

class MemSearch {
public:
    enum class Cmp { Changed, Unchanged, Increased, Decreased };

    // Every address aligned to width (1, 2 or 4 bytes) in the first size bytes of rdram becomes a candidate, and
    // the values now are what the next filter compares against.
    void start(const uint8_t* rdram, size_t size, int width);
    // Keeps the candidates whose value compares with the last step's as cmp says (unsigned), then takes the
    // values now as the next step's.
    void filter(const uint8_t* rdram, Cmp cmp);

    bool started() const { return width_ != 0; }
    int width() const { return width_; }
    int steps() const { return steps_; }
    size_t count() const { return count_; }
    // The first n candidates' offsets into RDRAM (vram - 0x80000000), lowest first.
    std::vector<uint32_t> first(size_t n) const;
    // The value at an offset as of the last step.
    uint32_t previous(uint32_t offset) const { return read(snapshot_.data(), offset, width_); }

    // A width-byte big-endian N64 value at offset in RDRAM as the recomp keeps it (each 32-bit word in host order).
    static uint32_t read(const uint8_t* rdram, uint32_t offset, int width);

private:
    int width_ = 0;
    int steps_ = 0;
    size_t count_ = 0;
    std::vector<uint8_t> snapshot_;
    std::vector<uint64_t> bits_;  // bit i: offset i * width is a candidate
};

} // namespace rush2049

#endif
