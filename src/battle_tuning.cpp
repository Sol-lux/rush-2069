// Battle Mode Alt (General tab): the Battle gameplay mods, all on with Alt and all off with Original, which is Battle as
// it shipped. docs/mods.md lists them. They are in the Battle overlay (ovl2):
//   - double health: every hit does half its damage;
//   - machine gun spin-up: the machine gun's refire winds up like a chain gun's;
//   - Rammer cooldown and push: a longer refire, and a ram shoves its target away along the ground;
//   - pea shooter: the weapon held with no pickup fires while fire is held and hits a third harder;
//   - Sonic Boom: more push away from the blast, less lift and less flip;
//   - bouncy ball: its launch speed and angle, lifetime, gravity and bounce, tunable;
//   - plasma push: a plasma hit shoves the car away from the shooter;
//   - powerup bars: a bar at the top of each view for each timed powerup, and invisibility hides the arrow.
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
// Bouncy ball. Projectile type 2. The fire routine's case for it (from the jump table at 0x80394EBC on the type) sets
// its lifetime (+0x8) to 3.1667 s (0x80394EE0), its radius (+0x10) to 15 and its velocity (+0x14) to the aim
// (+0x50) times 75 (f26) plus the car record's velocity (+0x14). Each frame the projectile routine ovl2_func_8038E114
// (its flight jump table at 0x80394E20) takes the world's gravity (0x80142764) times the frame time times 2.5 off its
// fall speed (+0x18) and moves it, with no drag. When it meets the ground, the velocity is turned into the surface's
// frame (func_800A61B0, to sp+0x64), the speed into it (sp+0x68) turned round and times 0.9 (0x80394E54), and turned
// back (func_8009E820); the speed along the surface is kept whole. With Alt each of those is a constant to tune
// a slider on the General tab (config.cpp), at its value as shipped until it is moved.
//
// Plasma push. A direct hit of a projectile (ovl2_func_8038E114, s5 the projectile, s2 the car record hit, s6 the
// shooter's, as the damage routine takes them in a1 and a0) of type 0, the plasma blaster, with Alt adds a flat push
// from the shooter to the victim (the shot's velocity is zeroed by then) to the hit car's force, as the Rammer's push does.
//
// Tuning. The values above come from the Battle Tuning mod's options (battle_tuning_mod/, its sliders in the Mods
// tab), read once a Battle frame (src/battle_tuning_mod.cpp), and are the defaults in rush2049_game.h without it.
//
// Powerup bar. With Alt, a player holding a timed powerup has a bar at the top of its view, in the powerup's colour,
// emptying as it runs out (both last 30 s; with both, each has half the bar): invisibility (record +0x38C bit 0x1,
// time left +0x390) and the shield (bit 0x8 while its bubble grows, then 0x4 with time left at +0x394; the
// timers run in ovl2_func_8038F938). Battle counts as running while the fire routine's loop over the players (count
// at 0x801543CA) has started within the last quarter second, so the bars never show outside it. Each player's view is
// found in the frame's top-level list as the game wrote it: a viewport, a pipe sync and the view's scissor, the views
// in player order. The bars are fill rectangles appended at the end of the frame's list, after the HUD's call and
// before its full sync and end (func_800EDACC), with their own fill mode and a full-screen scissor; the frame's next
// list sets its own modes. powerup_fills is pure; the test checks it.
//
// Invisible arrow. With Alt, an invisible player's arrow is hidden outright, not left at the game's faint floor.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
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

std::atomic<double> battle_seen{ -1.0 };

std::atomic<float> tunings[rush2049::tuning_count];

struct TuningInit {
    TuningInit() {
        for (int i = 0; i < rush2049::tuning_count; i++) {
            tunings[i].store(rush2049::tuning_defaults[i]);
        }
    }
} tuning_init;

float ball_value(rush2049::Tuning which) {
    return tunings[which].load(std::memory_order_relaxed);
}

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
                           float force, float out[3]) {
    float scale = std::clamp(speed / rammer_push_reference_speed, rammer_push_min_scale, rammer_push_max_scale);
    flat_push(target[0] - hitter[0], target[2] - hitter[2], matrix, force * scale, out);
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
    rush2049::rammer_push(target_pos, hitter_pos, matrix, read_float(rdram, hitter_car + 0x3F0),
                          rush2049::tuning(rush2049::rammer_push_force), push);
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

extern "C" void rush2049_plasma_push(uint8_t* rdram, recomp_context* ctx) {
    uint32_t victim = (uint32_t)ctx->r18, shooter = (uint32_t)ctx->r22;
    if (!alt.load() || (int8_t)MEM_B(6, ctx->r21) != 0 || !is_record(victim) || victim == shooter) {
        return;
    }
    float matrix[9];
    for (int i = 0; i < 9; i++) {
        matrix[i] = read_float(rdram, victim + 0x2C + i * 4);
    }
    float push[3];
    // The shot's velocity is already zeroed at the hit, so the push runs from the shooter to the victim.
    rush2049::flat_push(read_float(rdram, victim + 0x8) - read_float(rdram, shooter + 0x8),
                        read_float(rdram, victim + 0x10) - read_float(rdram, shooter + 0x10), matrix,
                        rush2049::tuning(rush2049::plasma_push), push);
    int8_t car_index = (int8_t)MEM_B(0x35B, (gpr)(int32_t)victim);
    if (car_index < 0 || car_index >= max_players) {
        return;
    }
    uint32_t car = 0x8014A250 + (uint32_t)car_index * 0x808;
    for (int i = 0; i < 3; i++) {
        write_float(rdram, car + 0x124 + i * 4, read_float(rdram, car + 0x124 + i * 4) + push[i]);
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

void rush2049::powerup_fills(uint32_t flags, float invisible_left, float shield_left, float out[powerup_count]) {
    out[powerup_invisibility] = (flags & 0x1) ? std::clamp(invisible_left / powerup_seconds, 0.0f, 1.0f) : -1.0f;
    out[powerup_shield] = (flags & 0x8)   ? 1.0f
                          : (flags & 0x4) ? std::clamp(shield_left / powerup_seconds, 0.0f, 1.0f)
                                          : -1.0f;
}

extern "C" void rush2049_battle_frame(void) {
    battle_seen.store(now_seconds());
    rush2049::poll_battle_tuning();
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
    float fills[max_players][rush2049::powerup_count];
    bool any = false;
    for (int i = 0; i < players; i++) {
        uint32_t record = players_base + (uint32_t)i * player_size;
        rush2049::powerup_fills((uint32_t)MEM_W(0, (gpr)(int32_t)(record + 0x38C)), read_float(rdram, record + 0x390),
                                read_float(rdram, record + 0x394), fills[i]);
        for (int k = 0; k < rush2049::powerup_count; k++) {
            any |= fills[i][k] >= 0.0f;
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
        const Rect& v = views[i];
        int x0 = (v.x0 + v.x1 - rush2049::cooldown_bar_width) / 2;
        int y0 = v.y0 + rush2049::cooldown_bar_top_gap;
        int held = 0;
        for (int k = 0; k < rush2049::powerup_count; k++) {
            held += fills[i][k] >= 0.0f;
        }
        if (held == 0) {
            continue;
        }
        int width = (rush2049::cooldown_bar_width - (held - 1) * 2) / held;  // a 2 px gap between halves
        for (int k = 0; k < rush2049::powerup_count; k++) {
            if (fills[i][k] < 0.0f) {
                continue;
            }
            Rect back{ x0 - 1, y0 - 1, x0 + width + 1, y0 + rush2049::cooldown_bar_height + 1 };
            Rect bar{ x0, y0, x0 + (int)(width * fills[i][k] + 0.5f), y0 + rush2049::cooldown_bar_height };
            const int* c = rush2049::powerup_colours[k];
            fill(rdram, p, back, fill_colour(2, 2, 2));
            fill(rdram, p, bar, fill_colour(c[0], c[1], c[2]));
            x0 += width + 2;
        }
    }
    MEM_W(0, (gpr)(int32_t)dl_head_addr) = (int32_t)p;
}

// Invisibility hides the arrow: the arrow's colour takes the record's fade alpha (+0x3A1) but never below 0x30
// (ovl2_func_8038AA8C, sp+0x73); with Alt it is 0 while the record is invisible.
extern "C" void rush2049_invisible_arrow(uint8_t* rdram, recomp_context* ctx) {
    if (alt.load()) {
        MEM_B(0x73, ctx->r29) = 0;
    }
}

void rush2049::set_tuning(Tuning which, float value) {
    if (which >= 0 && which < tuning_count) {
        tunings[which].store(value, std::memory_order_relaxed);
    }
}

float rush2049::tuning(Tuning which) {
    return tunings[which].load(std::memory_order_relaxed);
}

void rush2049::flat_push(float dx, float dz, const float matrix[9], float force, float out[3]) {
    float length = std::sqrt(dx * dx + dz * dz);
    if (length <= 0.0f) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    float v[3] = { dx * force / length, 0.0f, dz * force / length };
    for (int i = 0; i < 3; i++) {
        out[i] = matrix[i * 3] * v[0] + matrix[i * 3 + 1] * v[1] + matrix[i * 3 + 2] * v[2];
    }
}

void rush2049::tilt_up(const float aim[3], float degrees, float out[3]) {
    float up[3] = { -aim[1] * aim[0], 1.0f - aim[1] * aim[1], -aim[1] * aim[2] };  // y less its part along the aim
    float length = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    if (degrees == 0.0f || length < 1e-4f) {
        out[0] = aim[0], out[1] = aim[1], out[2] = aim[2];
        return;
    }
    float a = degrees * 3.14159265f / 180.0f, c = std::cos(a), sn = std::sin(a) / length;
    for (int i = 0; i < 3; i++) {
        out[i] = aim[i] * c + up[i] * sn;
    }
}

extern "C" void rush2049_ball_launch(uint8_t* rdram, recomp_context* ctx) {
    if (!alt.load()) {
        return;
    }
    uint32_t ball = (uint32_t)ctx->r17, record = (uint32_t)ctx->r23;
    write_float(rdram, ball + 0x8, ball_value(rush2049::ball_lifetime));
    float aim[3], dir[3];
    for (int i = 0; i < 3; i++) {
        aim[i] = read_float(rdram, ball + 0x50 + i * 4);
    }
    rush2049::tilt_up(aim, ball_value(rush2049::ball_angle), dir);
    float speed = ball_value(rush2049::ball_launch_speed);
    write_float(rdram, ball + 0x14, dir[0] * speed + read_float(rdram, record + 0x14));
    write_float(rdram, ball + 0x18, dir[1] * speed + read_float(rdram, record + 0x18));
    ctx->f8.fl = dir[2] * speed + read_float(rdram, record + 0x1C);
}

extern "C" void rush2049_ball_gravity(recomp_context* ctx) {
    if (alt.load()) {
        ctx->f10.fl = ball_value(rush2049::ball_gravity_scale);
    }
}

extern "C" void rush2049_ball_bounce(uint8_t* rdram, recomp_context* ctx) {
    if (!alt.load()) {
        return;
    }
    ctx->f8.fl = ball_value(rush2049::ball_bounce_up);
    uint32_t sp = (uint32_t)ctx->r29;
    write_float(rdram, sp + 0x64, read_float(rdram, sp + 0x64) * ball_value(rush2049::ball_bounce_along));
    write_float(rdram, sp + 0x6C, read_float(rdram, sp + 0x6C) * ball_value(rush2049::ball_bounce_along));
}
