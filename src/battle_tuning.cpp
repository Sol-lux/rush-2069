// Battle Mode Alt (General tab): the Battle gameplay mods, all on with Alt and all off with Original, which is Battle as
// it shipped. docs/mods.md lists them. They are in the Battle overlay (ovl2):
//   - double health: every hit does half its damage;
//   - machine gun spin-up: the machine gun's refire winds up like a chain gun's.
//
// Each Battle player has a record at 0x80152818 + n * 0x3B8 (n 0 for player 1): +0x35B its car's index (the car is
// 0x8014A250 + index * 0x808), +0x384 the weapon (s8: 8 none, 1 the machine gun), +0x385 its ammo (s8, -1 with no
// weapon; the machine gun comes with 100), +0x386 its health (s16, 800 at spawn, func_800D1004, and after a health
// pickup, func_8010D3C0), +0x388 the last damage taken, +0x3AC the refire timer (float seconds).
//
// Health. All damage goes through ovl2_func_8038D3A4 (a3 the victim's record, a2 the damage, already scaled by the
// victim's armour flag): it stores a2 at +0x388, takes it off the health and, at 0 or below, scores the kill. Double
// Alt halves a2 there, rounding up, which is exactly double health (a machine gun round does 114 of 800, 57 with Double)
// and leaves the health bar right, as the HUD draws it as a share of 800.
//
// Machine gun. The fire routine ovl2_func_8038FCE0 counts the refire timer down by the frame's time (0x8002EB94) and
// fires when it reaches 0 with ammo left; it then reloads it by the weapon (a jump table at 0x80394EBC on the weapon:
// 0, 2, 6 and 8 0.1667 s, 4 and 7 1 s, 1 the machine gun 0.0667 s from 0x80394EE4, so 15 rounds a second). With
// Alt the machine gun's reload follows a spin level s from 0 to 1, 1 / (5 + 25 s): 5 rounds a second from rest and
// 30 at full spin. The level is worked out at each shot from the time since the last: a gap no longer than the last
// reload plus the hold time (20 frames at 60 fps) counts as fire held and spins up by the gap over 1.5 s; a longer
// one spins down by what passed the hold over 1.5 s. Its time is the host's, so a pause spins the gun down.
// spin_after_shot is pure; tests/battle_tuning_test.cpp checks it.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>

#include "recomp.h"
#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

std::atomic<bool> alt{ false };

constexpr uint32_t players_base = 0x80152818;
constexpr uint32_t player_size = 0x3B8;
constexpr int max_players = 4;

struct Spin {
    double last_shot = -1.0;  // host seconds, -1 never
    float level = 0.0f;
    float interval = 0.0f;    // the reload given at the last shot
};
Spin spins[max_players];

double now_seconds() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

} // namespace

float rush2049::spin_after_shot(float level, float gap, float last_interval) {
    if (gap < 0.0f) {
        return 0.0f;  // the first shot
    }
    float idle = gap - last_interval;
    if (idle <= spin_hold_seconds) {
        return std::min(1.0f, level + gap / spin_up_seconds);
    }
    return std::max(0.0f, level - (idle - spin_hold_seconds) / spin_down_seconds);
}

float rush2049::spin_interval(float level) {
    return 1.0f / (spin_min_rate + (spin_max_rate - spin_min_rate) * level);
}

void rush2049::set_battle_mode_alt(bool on) {
    alt.store(on);
}

extern "C" void rush2049_battle_damage(recomp_context* ctx) {
    if (alt.load()) {
        int32_t damage = (int32_t)ctx->r6;
        ctx->r6 = (gpr)(int64_t)((damage + 1) / 2);
    }
}

extern "C" void rush2049_machine_gun_reload(uint8_t* rdram, recomp_context* ctx) {
    if (!alt.load()) {
        return;
    }
    uint32_t record = (uint32_t)ctx->r23;
    if (record < players_base || (record - players_base) / player_size >= (uint32_t)max_players) {
        return;
    }
    int player = (int)((record - players_base) / player_size);
    Spin& spin = spins[player];
    double now = now_seconds();
    float gap = spin.last_shot < 0.0 ? -1.0f : (float)(now - spin.last_shot);
    spin.level = rush2049::spin_after_shot(spin.level, gap, spin.interval);
    spin.interval = rush2049::spin_interval(spin.level);
    spin.last_shot = now;
    int32_t bits;
    std::memcpy(&bits, &spin.interval, 4);
    MEM_W(0x3AC, (gpr)(int32_t)record) = bits;
}
