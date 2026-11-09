// The memory search behind --memprobe (include/mem_search.h). Pure code: tests/mem_search_test.cpp.
//
// The candidates are a bitset over the aligned addresses (1 MB for 8 MB of RDRAM a byte at a time), and the
// previous step's values a full copy of RDRAM, so a step costs one pass over memory whatever is left.
// RDRAM holds each big-endian 32-bit word in host (little-endian) order, so a byte is at offset ^ 3 and a halfword at
// offset ^ 2, read as a host halfword.

#include "mem_search.h"

#include <bit>
#include <cstring>

namespace rush2049 {

uint32_t MemSearch::read(const uint8_t* rdram, uint32_t offset, int width) {
    switch (width) {
        case 1:
            return rdram[offset ^ 3];
        case 2: {
            uint16_t v;
            std::memcpy(&v, rdram + (offset ^ 2), 2);
            return v;
        }
        default: {
            uint32_t v;
            std::memcpy(&v, rdram + offset, 4);
            return v;
        }
    }
}

void MemSearch::start(const uint8_t* rdram, size_t size, int width) {
    width_ = (width == 1 || width == 2) ? width : 4;
    steps_ = 0;
    size &= ~size_t(3);
    snapshot_.assign(rdram, rdram + size);
    count_ = size / width_;
    bits_.assign((count_ + 63) / 64, ~uint64_t(0));
    if (count_ % 64 != 0) {
        bits_.back() = (uint64_t(1) << (count_ % 64)) - 1;
    }
}

void MemSearch::filter(const uint8_t* rdram, Cmp cmp) {
    if (!started()) {
        return;
    }
    size_t kept = 0;
    for (size_t w = 0; w < bits_.size(); w++) {
        uint64_t word = bits_[w];
        uint64_t out = word;
        while (word != 0) {
            int bit = std::countr_zero(word);
            word &= word - 1;
            uint32_t offset = uint32_t((w * 64 + bit) * width_);
            uint32_t before = read(snapshot_.data(), offset, width_);
            uint32_t now = read(rdram, offset, width_);
            bool keep = false;
            switch (cmp) {
                case Cmp::Changed: keep = now != before; break;
                case Cmp::Unchanged: keep = now == before; break;
                case Cmp::Increased: keep = now > before; break;
                case Cmp::Decreased: keep = now < before; break;
            }
            if (!keep) {
                out &= ~(uint64_t(1) << bit);
            }
        }
        bits_[w] = out;
        kept += std::popcount(out);
    }
    count_ = kept;
    std::memcpy(snapshot_.data(), rdram, snapshot_.size());
    steps_++;
}

std::vector<uint32_t> MemSearch::first(size_t n) const {
    std::vector<uint32_t> out;
    for (size_t w = 0; w < bits_.size() && out.size() < n; w++) {
        uint64_t word = bits_[w];
        while (word != 0 && out.size() < n) {
            int bit = std::countr_zero(word);
            word &= word - 1;
            out.push_back(uint32_t((w * 64 + bit) * width_));
        }
    }
    return out;
}

} // namespace rush2049
