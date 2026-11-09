// Tests of the --memprobe memory search (src/mem_search.cpp).

#include "mem_search.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using rush2049::MemSearch;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    }
}

// Writes a big-endian N64 value the way the recomp stores RDRAM (each 32-bit word in host order).
void put(std::vector<uint8_t>& ram, uint32_t offset, int width, uint32_t value) {
    for (int i = 0; i < width; i++) {
        ram[(offset + i) ^ 3] = uint8_t(value >> (8 * (width - 1 - i)));
    }
}

} // namespace

int main() {
    std::vector<uint8_t> ram(4096 + 12, 0);  // not a multiple of 64 candidates at any width

    // Reading back what put wrote, at each width.
    put(ram, 0x10, 4, 0x11223344);
    check(MemSearch::read(ram.data(), 0x10, 4) == 0x11223344, "read word");
    check(MemSearch::read(ram.data(), 0x10, 2) == 0x1122, "read high halfword");
    check(MemSearch::read(ram.data(), 0x12, 2) == 0x3344, "read low halfword");
    check(MemSearch::read(ram.data(), 0x11, 1) == 0x22, "read byte");
    put(ram, 0x10, 4, 0);

    for (int width : { 1, 2, 4 }) {
        MemSearch s;
        s.start(ram.data(), ram.size(), width);
        check(s.count() == ram.size() / width, "every aligned address starts as a candidate");
        check(s.first(3).size() == 3 && s.first(3)[1] == uint32_t(width), "first lists offsets in order");

        put(ram, 0x40, width, 5);
        put(ram, 0x80, width, 9);
        s.filter(ram.data(), MemSearch::Cmp::Changed);
        check(s.count() == 2, "changed keeps the two written");
        check(s.first(10).size() == 2 && s.first(10)[0] == 0x40 && s.first(10)[1] == 0x80, "changed: which two");
        check(s.previous(0x80) == 9, "previous is the last step's value");

        put(ram, 0x40, width, 6);  // up
        put(ram, 0x80, width, 2);  // down
        MemSearch up = s, down = s, same = s;
        up.filter(ram.data(), MemSearch::Cmp::Increased);
        check(up.count() == 1 && up.first(1)[0] == 0x40, "increased");
        down.filter(ram.data(), MemSearch::Cmp::Decreased);
        check(down.count() == 1 && down.first(1)[0] == 0x80, "decreased");
        same.filter(ram.data(), MemSearch::Cmp::Unchanged);
        check(same.count() == 0, "unchanged drops both");

        // Unchanged from a fresh start keeps everything not written.
        MemSearch fresh;
        fresh.start(ram.data(), ram.size(), width);
        put(ram, 0x100, width, 1);
        fresh.filter(ram.data(), MemSearch::Cmp::Unchanged);
        check(fresh.count() == ram.size() / width - 1, "unchanged keeps all but the written one");
        check(fresh.steps() == 1, "steps counts filters");

        std::fill(ram.begin(), ram.end(), uint8_t(0));
    }

    if (failures == 0) {
        printf("mem_search: all tests passed\n");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
