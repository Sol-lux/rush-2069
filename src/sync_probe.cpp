// The sync probe: whether the game plays the same from the same input, which playing it over a network needs.
//
// Thread 5 (func_800E7808) waits for a retrace, reads the four controllers (func_800E73D8: osContGetReadData into
// four 6-byte OSContPads on its stack) and, in a race, runs the tick (func_800E6AF8). The tick is a fixed step: the
// race clock is its count of ticks (0x80143FF4) times a constant, and each car's step is the ticks since its last.
// So the pads of each read, by the read's number, are all the input the race has, and the hook just after the read
// (us.toml, 0x800E7500) is where they are taken or replaced:
//
// --sync-record <file>   the four pads of every read, 24 bytes a read, as the game has them in memory
// --sync-replay <file>   every read gets that file's pads instead of the controllers', and the pads left alone (no
//                        buttons, stick centred) past its end
// --sync-hash <file>     a line each read while a race runs: the read's number, the tick count, the timer's count of
//                        60ths (0x8002EB64) and a hash of the race's memory, which is what Rewind saves
//                        (rush2049::race_memory, src/rewind.cpp)
// --sync-dump <prefix> <reads>   all of RDRAM, as it is in memory, to <prefix><read>.bin at each of those reads
//                        (comma-separated numbers), to see which words two runs differ in
//
// Two replays of one recording that write the same hash file played the same race. Where they part, the first line
// that differs is the read to look at.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "recomp.h"
#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

constexpr uint32_t game_state_addr = 0x801174B4;  // the frame loop's state bits; 0x00400000 in a race
constexpr uint32_t tick_count_addr = 0x80143FF4;  // the race's ticks so far
constexpr uint32_t timer_addr = 0x8002EB64;       // the timer's count of 60ths of a second
constexpr size_t pads_bytes = 4 * 6;              // four OSContPads

FILE* record_file = nullptr;
FILE* hash_file = nullptr;
std::vector<uint8_t> replay;
std::string dump_prefix;
std::vector<uint32_t> dump_reads;
bool replaying = false;
uint32_t reads = 0;

uint32_t read_word(const uint8_t* rdram, uint32_t addr) {
    return *(const uint32_t*)(rdram + (addr - 0x80000000u));
}

// The race's memory, a 64-bit word at a time (every range starts and ends on a word).
uint64_t hash_race(const uint8_t* rdram) {
    uint64_t h = 0x9E3779B97F4A7C15ull;
    for (const rush2049::MemoryRange& r : rush2049::race_memory()) {
        const uint8_t* p = rdram + r.lo;
        const uint8_t* end = rdram + r.hi;
        for (; p + 8 <= end; p += 8) {
            uint64_t w;
            memcpy(&w, p, 8);
            h = (h ^ w) * 0xFF51AFD7ED558CCDull;
            h ^= h >> 32;
        }
        for (; p < end; p++) {
            h = (h ^ *p) * 0x100000001B3ull;
        }
    }
    return h;
}

}

void rush2049::sync_record(const char* path) {
    record_file = fopen(path, "wb");
    if (record_file == nullptr) {
        fprintf(stderr, "--sync-record: can't open %s\n", path);
    }
}

void rush2049::sync_replay(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        fprintf(stderr, "--sync-replay: can't open %s\n", path);
        return;
    }
    uint8_t buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        replay.insert(replay.end(), buffer, buffer + n);
    }
    fclose(f);
    replaying = true;
}

void rush2049::sync_hash(const char* path) {
    hash_file = fopen(path, "w");
    if (hash_file == nullptr) {
        fprintf(stderr, "--sync-hash: can't open %s\n", path);
    }
}

void rush2049::sync_dump(const char* prefix, const char* reads_list) {
    dump_prefix = prefix;
    for (const char* p = reads_list; *p != 0;) {
        char* end;
        dump_reads.push_back((uint32_t)strtoul(p, &end, 10));
        p = *end == ',' ? end + 1 : end;
        if (end == p && *end != 0) {
            break;
        }
    }
}

extern "C" void rush2049_sync_pads(uint8_t* rdram, gpr pads_addr) {
    uint8_t* pads = rdram + ((uint32_t)pads_addr - 0x80000000u);
    if (replaying) {
        size_t at = (size_t)reads * pads_bytes;
        if (at + pads_bytes <= replay.size()) {
            memcpy(pads, replay.data() + at, pads_bytes);
        }
        else {
            memset(pads, 0, pads_bytes);
        }
    }
    if (record_file != nullptr) {
        fwrite(pads, 1, pads_bytes, record_file);
        if (reads % 60 == 0) {
            fflush(record_file);
        }
    }
    if (hash_file != nullptr && (read_word(rdram, game_state_addr) & 0x00400000) != 0) {
        fprintf(hash_file, "%u %u %u %016llx\n", reads, read_word(rdram, tick_count_addr), read_word(rdram, timer_addr),
            (unsigned long long)hash_race(rdram));
        if (reads % 60 == 0) {
            fflush(hash_file);
        }
    }
    for (uint32_t at : dump_reads) {
        if (at == reads) {
            FILE* f = fopen((dump_prefix + std::to_string(reads) + ".bin").c_str(), "wb");
            if (f != nullptr) {
                fwrite(rdram, 1, 0x800000, f);
                fclose(f);
            }
        }
    }
    reads++;
}
