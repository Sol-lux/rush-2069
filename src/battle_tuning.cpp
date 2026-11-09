// Battle Mode Alt (General tab): the Battle gameplay mods, all on with Alt and all off with Original, which is Battle as
// it shipped. docs/mods.md lists them. They are in the Battle overlay (ovl2):
//   - double health: every hit does half its damage;
//   - machine gun spin-up: the machine gun's refire winds up like a chain gun's;
//   - Rammer cooldown and push: a longer refire, and a ram shoves its target away along the ground;
//   - pea shooter: the weapon held with no pickup fires while fire is held and hits a third harder;
//   - Sonic Boom: more push away from the blast, less lift and less flip.
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
//
// Rammer. In Battle, func_800CE358 checks each pair of touching cars both ways round: a car holding the Rammer (weapon
// 5) with its refire timer run out, and the other car more than 2.25 ahead, spends one ammo, sets its timer to 0.333 s
// (0x80124108, 0x8012410C for the second way round) and does 160 + 7 x its speed (car +0x3F0) of damage. A long shove
// fires it again 0.333 s later. With Alt the timer is rammer_refire_seconds instead, the damage (the float the hook
// sees, f16 or f8, before it is truncated into a2) is rammer_damage's 200 + 5 x speed, and then halved by
// double health like any hit; and the target gets a push like
// the one any projectile impact gives (ovl2_func_8038D498): the target record's position (+0x8) less the hitter's,
// normalised, times a force, rotated by the target record's matrix (+0x2C, out_i = row_i . v, as func_800A61B0) and
// added to the car's force (car +0x124, cleared each frame). The impact's force is 330000 (0x80394DD0) plus 66000 up
// (0x80394DCC), and it also adds to the car's spin (+0x13C), which is what flips cars. The ram's push is flat, with
// no lift and no spin, and scales with the rammer's speed. rammer_push is pure; the test checks it.
//
// Pea shooter. Weapon 8 is what a car holds with no pickup (ammo -1, so it never runs out; reload 0.1667 s). The fire
// routine fires on the fire button (the mask at controller +0x3C, the controller at record +0x380) pressed this frame
// (+0x4), or held (+0x8) only for weapon 1, the machine gun. With Alt weapon 8 counts as held too, so it fires 6 a
// second while held, and its direct hits (ovl2_func_8038E114, base damage by projectile type at +6, 80 for 8) do 4/3.
//
// Sonic Boom. Its impact, ovl2_func_8038D498 (only projectile type 7 calls it), pushes each car in its radius by the
// direction from the blast times 330000 (0x80394DD0, f22) plus 66000 up (0x80394DCC, f28), scaled by distance, into
// the car's force (+0x124), and the push's x and z into its spin (+0x13C, +0x144). With Alt the away force is 1.5x,
// the lift 0.4x and the spin's share 0.3x, so a car is thrown out more and flipped less.
//
// Cooldown bar. With Alt, a weapon with a long reload (cooldown_bar_min_reload and up: not the machine gun or the pea
// shooter) shows a small bar at the bottom of its player's view while the refire timer (+0x3AC) runs, filling until
// the weapon is ready, and gone then. The reload is the timer's value when it last went up (the fire routine and the
// Rammer both set it there). Battle counts as running while the fire routine's loop over the players (count at
// 0x801543CA) has started within the last quarter second, so the bars never show outside it. Each player's view is
// found in the frame's top-level list as the game wrote it: a viewport, a pipe sync and the view's scissor, the views
// in player order. The bars are fill rectangles appended at the end of the frame's list, after the HUD's call and
// before its full sync and end (func_800EDACC), with their own fill mode and a full-screen scissor; the frame's next
// list sets its own modes. cooldown_fill is pure; the test checks it.

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

struct Cooldown {
    float last_timer = 0.0f;
    float reload = 0.0f;
};
Cooldown cooldowns[max_players];
std::atomic<double> battle_seen{ -1.0 };

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

void rush2049::rammer_push(const float target[3], const float hitter[3], const float matrix[9], float speed,
                           float out[3]) {
    float v[3] = { target[0] - hitter[0], 0.0f, target[2] - hitter[2] };
    float length = std::sqrt(v[0] * v[0] + v[2] * v[2]);
    if (length <= 0.0f) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    float scale = std::clamp(speed / rammer_push_reference_speed, rammer_push_min_scale, rammer_push_max_scale);
    float force = rammer_push_force * scale / length;
    v[0] *= force;
    v[2] *= force;
    for (int i = 0; i < 3; i++) {
        out[i] = matrix[i * 3] * v[0] + matrix[i * 3 + 1] * v[1] + matrix[i * 3 + 2] * v[2];
    }
}

float rush2049::rammer_damage(float speed) {
    return rammer_damage_base + rammer_damage_per_speed * speed;
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

namespace {

float read_float(uint8_t* rdram, uint32_t addr) {
    int32_t bits = MEM_W(0, (gpr)(int32_t)addr);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

void write_float(uint8_t* rdram, uint32_t addr, float f) {
    int32_t bits;
    std::memcpy(&bits, &f, 4);
    MEM_W(0, (gpr)(int32_t)addr) = bits;
}

bool is_record(uint32_t record) {
    return record >= players_base && (record - players_base) % player_size == 0 &&
           (record - players_base) / player_size < (uint32_t)max_players;
}

} // namespace

extern "C" void rush2049_rammer_hit(uint8_t* rdram, uint32_t hitter, uint32_t target, uint32_t hitter_car,
                                    float* damage) {
    if (!alt.load() || !is_record(hitter) || !is_record(target)) {
        return;
    }
    *damage = rush2049::rammer_damage(read_float(rdram, hitter_car + 0x3F0));
    write_float(rdram, hitter + 0x3AC, rush2049::rammer_refire_seconds);
    float target_pos[3], hitter_pos[3], matrix[9];
    for (int i = 0; i < 3; i++) {
        target_pos[i] = read_float(rdram, target + 0x8 + i * 4);
        hitter_pos[i] = read_float(rdram, hitter + 0x8 + i * 4);
    }
    for (int i = 0; i < 9; i++) {
        matrix[i] = read_float(rdram, target + 0x2C + i * 4);
    }
    float push[3];
    rush2049::rammer_push(target_pos, hitter_pos, matrix, read_float(rdram, hitter_car + 0x3F0), push);
    int8_t car_index = (int8_t)MEM_B(0x35B, (gpr)(int32_t)target);
    if (car_index < 0 || car_index >= max_players) {
        return;
    }
    uint32_t car = 0x8014A250 + (uint32_t)car_index * 0x808;
    for (int i = 0; i < 3; i++) {
        write_float(rdram, car + 0x124 + i * 4, read_float(rdram, car + 0x124 + i * 4) + push[i]);
    }
}

int rush2049::pea_damage(int damage) {
    return (damage * pea_damage_num + pea_damage_den / 2) / pea_damage_den;
}

extern "C" void rush2049_pea_auto_fire(recomp_context* ctx) {
    if (alt.load() && (int32_t)ctx->r2 == 8) {
        ctx->r1 = 8;  // the held check that follows is for weapon at, so weapon 8 passes it
    }
}

extern "C" void rush2049_pea_damage(uint8_t* rdram, recomp_context* ctx) {
    if (alt.load() && (int8_t)MEM_B(6, ctx->r21) == 8) {
        ctx->r16 = (gpr)(int64_t)rush2049::pea_damage((int32_t)ctx->r16);
    }
}

extern "C" void rush2049_sonic_boom_forces(recomp_context* ctx) {
    if (alt.load()) {
        ctx->f22.fl *= rush2049::sonic_out_scale;
        ctx->f28.fl *= rush2049::sonic_up_scale;
    }
}

extern "C" void rush2049_sonic_boom_spin(uint8_t* rdram, recomp_context* ctx) {
    if (alt.load()) {
        uint32_t sp = (uint32_t)ctx->r29;
        write_float(rdram, sp + 0x48, read_float(rdram, sp + 0x48) * rush2049::sonic_spin_scale);
        write_float(rdram, sp + 0x50, read_float(rdram, sp + 0x50) * rush2049::sonic_spin_scale);
    }
}

float rush2049::cooldown_fill(float timer, float reload) {
    if (reload <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(1.0f - timer / reload, 0.0f, 1.0f);
}

extern "C" void rush2049_battle_frame(void) {
    battle_seen.store(now_seconds());
}

namespace {

constexpr uint32_t dl_head_addr = 0x801497C8;
constexpr uint32_t player_count_addr = 0x801543CA;

struct Rect {
    int x0, y0, x1, y1;
};

void put(uint8_t* rdram, uint32_t& p, uint32_t w0, uint32_t w1) {
    MEM_W(0, (gpr)(int32_t)p) = (int32_t)w0;
    MEM_W(4, (gpr)(int32_t)p) = (int32_t)w1;
    p += 8;
}

// RGBA5551, twice, as a 16-bit fill colour.
uint32_t fill_colour(int r, int g, int b) {
    uint32_t c = (uint32_t)((r << 11) | (g << 6) | (b << 1) | 1);
    return (c << 16) | c;
}

void fill(uint8_t* rdram, uint32_t& p, const Rect& r, uint32_t colour) {
    if (r.x1 <= r.x0 || r.y1 <= r.y0) {
        return;
    }
    put(rdram, p, 0xE7000000, 0);       // gsDPPipeSync
    put(rdram, p, 0xF7000000, colour);  // gsDPSetFillColor
    put(rdram, p, 0xF6000000 | ((uint32_t)(r.x1 - 1) << 14) | ((uint32_t)(r.y1 - 1) << 2),
        ((uint32_t)r.x0 << 14) | ((uint32_t)r.y0 << 2));  // gsDPFillRectangle, inclusive in fill mode
}

// The views' scissors in the frame's top-level list, in the order drawn.
int find_views(uint8_t* rdram, uint32_t start, uint32_t end, Rect* views, int max) {
    int count = 0;
    for (uint32_t p = start; p + 24 <= end && count < max; p += 8) {
        if ((uint32_t)MEM_W(0, (gpr)(int32_t)p) != 0xDC080008 || (uint32_t)MEM_W(8, (gpr)(int32_t)p) != 0xE7000000 ||
            ((uint32_t)MEM_W(16, (gpr)(int32_t)p) >> 24) != 0xED) {
            continue;
        }
        uint32_t w0 = (uint32_t)MEM_W(16, (gpr)(int32_t)p), w1 = (uint32_t)MEM_W(20, (gpr)(int32_t)p);
        Rect r{ (int)((w0 >> 12) & 0xFFF) / 4, (int)(w0 & 0xFFF) / 4, (int)((w1 >> 12) & 0xFFF) / 4,
                (int)(w1 & 0xFFF) / 4 };
        bool seen = false;
        for (int i = 0; i < count; i++) {
            seen |= views[i].x0 == r.x0 && views[i].y0 == r.y0 && views[i].x1 == r.x1 && views[i].y1 == r.y1;
        }
        if (!seen && r.x1 - r.x0 >= 100 && r.y1 - r.y0 >= 60) {
            views[count++] = r;
        }
    }
    return count;
}

} // namespace

extern "C" void rush2049_cooldown_bars(uint8_t* rdram) {
    if (!alt.load() || now_seconds() - battle_seen.load() > 0.25) {
        return;
    }
    int players = std::clamp((int)(int16_t)MEM_H(0, (gpr)(int32_t)player_count_addr), 0, max_players);
    float fills[max_players];
    bool any = false;
    for (int i = 0; i < players; i++) {
        uint32_t record = players_base + (uint32_t)i * player_size;
        float timer = read_float(rdram, record + 0x3AC);
        Cooldown& c = cooldowns[i];
        if (timer > c.last_timer + 0.001f) {
            c.reload = timer;
        }
        c.last_timer = timer;
        fills[i] = -1.0f;
        if (timer > 0.0f && c.reload >= rush2049::cooldown_bar_min_reload) {
            fills[i] = rush2049::cooldown_fill(timer, c.reload);
            any = true;
        }
    }
    if (!any) {
        return;
    }
    uint32_t head = (uint32_t)MEM_W(0, (gpr)(int32_t)dl_head_addr);
    Rect views[max_players];
    int view_count = find_views(rdram, rush2049::frame_list_current(rdram), head, views, max_players);
    uint32_t p = head;
    put(rdram, p, 0xE7000000, 0);           // gsDPPipeSync
    put(rdram, p, 0xE3000A01, 0x00300000);  // gsDPSetCycleType(G_CYC_FILL)
    put(rdram, p, 0xE200001C, 0);           // gsDPSetRenderMode(0, 0)
    put(rdram, p, 0xED000000, (320u * 4 << 12) | 240u * 4);  // gsDPSetScissor, the whole screen
    for (int i = 0; i < players && i < view_count; i++) {
        if (fills[i] < 0.0f) {
            continue;
        }
        const Rect& v = views[i];
        int x0 = (v.x0 + v.x1 - rush2049::cooldown_bar_width) / 2;
        int y1 = v.y1 - rush2049::cooldown_bar_bottom_gap;
        Rect back{ x0 - 1, y1 - rush2049::cooldown_bar_height - 1, x0 + rush2049::cooldown_bar_width + 1, y1 + 1 };
        Rect bar{ x0, y1 - rush2049::cooldown_bar_height, x0 + (int)(rush2049::cooldown_bar_width * fills[i] + 0.5f),
                  y1 };
        fill(rdram, p, back, fill_colour(2, 2, 2));
        fill(rdram, p, bar, fill_colour(31, 20, 0));
    }
    MEM_W(0, (gpr)(int32_t)dl_head_addr) = (int32_t)p;
}
