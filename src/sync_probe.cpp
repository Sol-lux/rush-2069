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
//                        (rush2049::race_memory, src/rewind.cpp), then the same hash without the words that hold
//                        a sound's ID (sound_id_words: the sound manager hands them out on the audio thread's clock)
// --sync-dump <prefix> <reads>   all of RDRAM, as it is in memory, to <prefix><read>.bin at each of those reads
//                        (comma-separated numbers), to see which words two runs differ in
// --sync-random <file>   a line each time a function that steps the random state (0x8011735C) is entered (us.toml
//                        hooks the entry of each: func_8008B2E4's callers and those with the generator written out
//                        in place): the read's number, the thread, the function, its caller and the state. Two
//                        replays' files part at the first call one of them made and the other did not
// --sync-hw <file>       where the game meets the machine's timing (src/frame_lock.cpp, src/spin_yield.cpp), a line
//                        each: the read's number, the thread, the kind (S: thread 5 steps, value its steps; F: thread
//                        7 at a wait for a frame, value the hook's address, 0x800FD510 the load's; R: the retraces the
//                        frame really counted; Y: that thread's spin yields since the last read), then the graphics
//                        task counter (0x8002EB70), the frame loop's state (0x801174B4) and the tick count. Logged
//                        with or without --frame-lock; the first line two replays part at names what split them
// --sync-seed <hex>      the random state set to that value at every read until a race runs, so that a race starts
//                        from the same state whatever the menus and the load drew from it
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
#include "ultramodern/ultramodern.hpp"
#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

constexpr uint32_t game_state_addr = 0x801174B4;  // the frame loop's state bits; 0x00400000 in a race
constexpr uint32_t tick_count_addr = 0x80143FF4;  // the race's ticks so far
constexpr uint32_t timer_addr = 0x8002EB64;       // the timer's count of 60ths of a second
constexpr size_t pads_bytes = 4 * 6;              // four OSContPads

FILE* record_file = nullptr;
FILE* hash_file = nullptr;
FILE* random_file = nullptr;
FILE* hw_file = nullptr;
uint32_t spins[16] = {};
constexpr uint32_t gfx_counter_addr = 0x8002EB70;  // graphics tasks in flight
constexpr uint32_t random_state_addr = 0x8011735C;
std::vector<uint8_t> replay;
std::string dump_prefix;
std::vector<uint32_t> dump_reads;
bool replaying = false;
bool seeding = false;
uint32_t seed = 0;
uint32_t reads = 0;

uint32_t read_word(const uint8_t* rdram, uint32_t addr) {
    return *(const uint32_t*)(rdram + (addr - 0x80000000u));
}

uint64_t mix(uint64_t h, uint64_t w) {
    h = (h ^ w) * 0xFF51AFD7ED558CCDull;
    return h ^ (h >> 32);
}

// Where the race keeps the IDs the sound manager hands out on the audio thread's clock (docs/netplay.md): a sound's ID
// is the manager's count of sounds started with the low bits of whichever node was free, so two runs of one race
// differ in them though the race is the same. As offsets into RDRAM, [lo, hi).
std::vector<std::pair<uint32_t, uint32_t>> sound_id_words() {
    std::vector<std::pair<uint32_t, uint32_t>> v = {
        { 0x00140420, 0x00140B14 },  // the cars' sound tables (src/rewind.cpp's held tables)
        { 0x00143CF8, 0x00143CFC },  // a word of the manager's own
        { 0x003BA1D0, 0x003BA1F0 },  // the Battle overlay's (ovl1_func_803A1EAC)
    };
    for (uint32_t n = 0; n < 32; n++) {  // the positional sounds' objects, 0x70 bytes from 0x8014E0F0, the ID at +0x60
        v.push_back({ 0x0014E0F0 + n * 0x70 + 0x60, 0x0014E0F0 + n * 0x70 + 0x64 });
    }
    for (uint32_t car = 0; car < 8; car++) {  // each car's +0x3FC (func_800924F4)
        v.push_back({ 0x0014A250 + car * 0x808 + 0x3FC, 0x0014A250 + car * 0x808 + 0x400 });
    }
    return v;
}

// Which 8-byte words of RDRAM hold a sound ID, a bit each.
const std::vector<uint8_t>& sound_id_map() {
    static const std::vector<uint8_t> map = [] {
        std::vector<uint8_t> m(0x800000 / 8 / 8);
        for (auto [lo, hi] : sound_id_words()) {
            for (uint32_t at = lo & ~7u; at < hi; at += 8) {
                m[at / 64] |= (uint8_t)(1u << ((at / 8) % 8));
            }
        }
        return m;
    }();
    return map;
}

// The race's memory, a 64-bit word at a time (every range starts and ends on a word); with skip_sounds, without the
// words that hold a sound ID.
uint64_t hash_race(const uint8_t* rdram, bool skip_sounds = false) {
    const std::vector<uint8_t>& skip = sound_id_map();
    uint64_t h = 0x9E3779B97F4A7C15ull;
    for (const rush2049::MemoryRange& r : rush2049::race_memory()) {
        uint32_t at = r.lo;
        for (; at + 8 <= r.hi; at += 8) {
            if (skip_sounds && (skip[at / 64] & (1u << ((at / 8) % 8))) != 0) {
                continue;
            }
            uint64_t w;
            memcpy(&w, rdram + at, 8);
            h = mix(h, w);
        }
        for (; at < r.hi; at++) {
            h = (h ^ rdram[at]) * 0x100000001B3ull;
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

void rush2049::sync_random(const char* path) {
    random_file = fopen(path, "w");
    if (random_file == nullptr) {
        fprintf(stderr, "--sync-random: can't open %s\n", path);
    }
}

extern "C" void rush2049_sync_random(uint8_t* rdram, uint32_t func, uint32_t caller) {
    if (random_file == nullptr) {
        return;
    }
    int thread = (int)TO_PTR(OSThread, ultramodern::this_thread())->id;
    fprintf(random_file, "%u %d %08x %08x %08x\n", reads, thread, func, caller, read_word(rdram, random_state_addr));
}

void rush2049::sync_seed(uint32_t value) {
    seeding = true;
    seed = value;
}

void rush2049::sync_hw(const char* path) {
    hw_file = fopen(path, "w");
    if (hw_file == nullptr) {
        fprintf(stderr, "--sync-hw: can't open %s\n", path);
    }
}

namespace {
void hw_line(uint8_t* rdram, int thread, char kind, uint32_t value) {
    fprintf(hw_file, "%u %d %c %08x %u %08x %d\n", reads, thread, kind, value, read_word(rdram, gfx_counter_addr),
        read_word(rdram, game_state_addr), (int32_t)read_word(rdram, tick_count_addr));
}
}

void rush2049::sync_hw_event(uint8_t* rdram, char kind, uint32_t value) {
    if (hw_file != nullptr) {
        hw_line(rdram, (int)TO_PTR(OSThread, ultramodern::this_thread())->id, kind, value);
    }
}

void rush2049::sync_hw_spin(uint8_t* rdram) {
    if (hw_file != nullptr) {
        uint32_t t = (uint32_t)TO_PTR(OSThread, ultramodern::this_thread())->id;
        spins[t < 16 ? t : 15]++;
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
    if (seeding && (read_word(rdram, game_state_addr) & 0x00400000) == 0) {
        *(uint32_t*)(rdram + (random_state_addr - 0x80000000u)) = seed;
    }
    if (hash_file != nullptr && (read_word(rdram, game_state_addr) & 0x00400000) != 0) {
        fprintf(hash_file, "%u %u %u %016llx %016llx\n", reads, read_word(rdram, tick_count_addr),
            read_word(rdram, timer_addr), (unsigned long long)hash_race(rdram),
            (unsigned long long)hash_race(rdram, true));
        if (reads % 60 == 0) {
            fflush(hash_file);
        }
    }
    if (hw_file != nullptr) {
        for (int t = 0; t < 16; t++) {
            if (spins[t] != 0) {
                hw_line(rdram, t, 'Y', spins[t]);
                spins[t] = 0;
            }
        }
        if (reads % 60 == 0) {
            fflush(hw_file);
        }
    }
    if (random_file != nullptr && reads % 60 == 0) {
        fflush(random_file);
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
