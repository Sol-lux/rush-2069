// Battle Mode Alt (src/battle_tuning.cpp) checks, run by CI: at Original the damage and the machine gun's reload are
// left alone; Alt halves damage, rounding up, pushes a rammed car flat away from the rammer, lets the pea shooter fire
// while held and hit a third harder, scales the Sonic Boom's push and spin, and the machine gun fires 5 rounds a second from rest,
// reaches 30 after 1.5 s of fire held, keeps its spin through a let-go of up to 20 frames and loses it over 1.5 s after.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "rush2049_game.h"
#include "rush2049_hooks.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        failures++;
    }
}

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-3f;
}

constexpr uint32_t record = 0x80152818;

float reload(uint8_t* rdram) {
    int32_t bits = MEM_W(0x3AC, (gpr)(int32_t)record);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

void set_reload(uint8_t* rdram, float f) {
    int32_t bits;
    std::memcpy(&bits, &f, 4);
    MEM_W(0x3AC, (gpr)(int32_t)record) = bits;
}

} // namespace

// battle_tuning.cpp's powerup bars read the frame's list; the test never draws them.
uint32_t rush2049::frame_list_current(uint8_t*) {
    return 0;
}
// And reads the mod's options (src/battle_tuning_mod.cpp), which the test has none of.
void rush2049::poll_battle_tuning() {}

int main() {
    // Damage.
    recomp_context ctx{};
    rush2049::set_battle_mode_alt(false);
    ctx.r6 = 114;
    rush2049_battle_damage(&ctx);
    check(ctx.r6 == 114, "Original leaves damage alone");
    rush2049::set_battle_mode_alt(true);
    ctx.r6 = 114;
    rush2049_battle_damage(&ctx);
    check(ctx.r6 == 57, "Alt halves 114 to 57");
    ctx.r6 = 1;
    rush2049_battle_damage(&ctx);
    check(ctx.r6 == 1, "Alt rounds 1 up");

    // The spin level.
    check(near(rush2049::spin_interval(0.0f), 0.2f), "5 rounds a second from rest");
    check(near(rush2049::spin_interval(1.0f), 1.0f / 30.0f), "30 rounds a second at full spin");
    check(rush2049::spin_after_shot(0.7f, -1.0f, 0.0f) == 0.0f, "the first shot is from rest");
    float level = 0.0f, interval = rush2049::spin_interval(0.0f), held = 0.0f;
    while (level < 1.0f && held < 3.0f) {
        level = rush2049::spin_after_shot(level, interval, interval);
        held += interval;
        interval = rush2049::spin_interval(level);
    }
    check(std::fabs(held - 1.5f) < 0.25f, "full spin after about 1.5 s held");
    check(near(rush2049::spin_after_shot(1.0f, 1.0f / 30.0f + 20.0f / 60.0f, 1.0f / 30.0f), 1.0f),
        "a let-go of 20 frames keeps full spin");
    check(near(rush2049::spin_after_shot(1.0f, 1.0f / 30.0f + 20.0f / 60.0f + 0.75f, 1.0f / 30.0f), 0.5f),
        "0.75 s past the hold loses half the spin");
    check(rush2049::spin_after_shot(1.0f, 10.0f, 1.0f / 30.0f) == 0.0f, "a long rest spins down fully");

    // The hook writes the reload only with Spin-Up, and only for a player's record.
    std::vector<uint8_t> ram(0x800000);
    uint8_t* rdram = ram.data();
    ctx = {};
    ctx.r23 = (gpr)(int32_t)record;
    rush2049::set_battle_mode_alt(false);
    set_reload(rdram, 0.0666667f);
    rush2049_machine_gun_reload(rdram, &ctx);
    check(near(reload(rdram), 0.0666667f), "Original leaves the reload alone");
    rush2049::set_battle_mode_alt(true);
    rush2049_machine_gun_reload(rdram, &ctx);
    check(near(reload(rdram), 0.2f), "Alt's first shot reloads 0.2 s");

    // The Rammer's push: flat, away from the rammer, scaled by its speed and clamped, then into the target's frame.
    {
        const float identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
        const float hitter[3] = { 0, 0, 0 };
        const float ahead_up[3] = { 10, 50, 0 };
        float out[3];
        rush2049::rammer_push(ahead_up, hitter, identity, rush2049::rammer_push_reference_speed, 800000.0f, out);
        check(near(out[0], 800000.0f) && out[1] == 0.0f && out[2] == 0.0f, "ram push is flat, full at the reference speed");
        rush2049::rammer_push(ahead_up, hitter, identity, 0.0f, 800000.0f, out);
        check(near(out[0], 800000.0f * rush2049::rammer_push_min_scale), "ram push at rest is the minimum");
        rush2049::rammer_push(ahead_up, hitter, identity, 1000.0f, 800000.0f, out);
        check(near(out[0], 800000.0f * rush2049::rammer_push_max_scale), "ram push is capped");
        const float turn[9] = { 0, 0, 1, 0, 1, 0, -1, 0, 0 };  // out_i = row_i . v
        rush2049::rammer_push(ahead_up, hitter, turn, rush2049::rammer_push_reference_speed, 800000.0f, out);
        check(near(out[0], 0.0f) && near(out[2], -800000.0f), "ram push is rotated into the target's frame");
        check(near(rush2049::rammer_damage(80.0f), 600.0f), "ram damage 200 + 5 x speed");
        const float same[3] = { 0, 5, 0 };
        rush2049::rammer_push(same, hitter, identity, 80.0f, 800000.0f, out);
        check(out[0] == 0.0f && out[1] == 0.0f && out[2] == 0.0f, "no push with no ground distance");
    }

    // The pea shooter: auto fire and a third more damage, for weapon 8 only.
    {
        rush2049::set_battle_mode_alt(true);
        recomp_context c{};
        c.r2 = 8;
        c.r1 = 1;
        rush2049_pea_auto_fire(&c);
        check(c.r1 == 8, "Alt lets the pea shooter fire while held");
        c.r2 = 4;
        c.r1 = 1;
        rush2049_pea_auto_fire(&c);
        check(c.r1 == 1, "other weapons still need a press");
        check(rush2049::pea_damage(80) == 107, "pea shooter 80 to 107");
        constexpr uint32_t projectile = 0x80200000;
        MEM_B(6, (gpr)(int32_t)projectile) = 8;
        c.r21 = (gpr)(int32_t)projectile;
        c.r16 = 80;
        rush2049_pea_damage(rdram, &c);
        check(c.r16 == 107, "pea shooter hit hook");
        MEM_B(6, (gpr)(int32_t)projectile) = 2;
        c.r16 = 80;
        rush2049_pea_damage(rdram, &c);
        check(c.r16 == 80, "other projectiles keep their damage");
        rush2049::set_battle_mode_alt(false);
        c.r2 = 8;
        c.r1 = 1;
        rush2049_pea_auto_fire(&c);
        check(c.r1 == 1, "Original leaves the pea shooter alone");
    }

    // The Sonic Boom: forces and spin scaled with Alt only.
    {
        recomp_context c{};
        c.f22.fl = 330000.0f;
        c.f28.fl = 66000.0f;
        rush2049::set_battle_mode_alt(false);
        rush2049_sonic_boom_forces(&c);
        check(c.f22.fl == 330000.0f && c.f28.fl == 66000.0f, "Original Sonic Boom forces");
        rush2049::set_battle_mode_alt(true);
        rush2049_sonic_boom_forces(&c);
        check(std::fabs(c.f22.fl - 495000.0f) < 1.0f && std::fabs(c.f28.fl - 26400.0f) < 1.0f, "Alt Sonic Boom forces");
        c.r29 = (gpr)(int32_t)0x80300000;
        float ten = 10.0f;
        int32_t bits;
        std::memcpy(&bits, &ten, 4);
        MEM_W(0x48, c.r29) = bits;
        MEM_W(0x50, c.r29) = bits;
        rush2049_sonic_boom_spin(rdram, &c);
        float x, z;
        bits = MEM_W(0x48, c.r29);
        std::memcpy(&x, &bits, 4);
        bits = MEM_W(0x50, c.r29);
        std::memcpy(&z, &bits, 4);
        check(near(x, 3.0f) && near(z, 3.0f), "Alt Sonic Boom spin share");
    }

    {
        float aim[3] = { 1.0f, 0.0f, 0.0f }, out[3];
        rush2049::tilt_up(aim, 0.0f, out);
        check(out[0] == 1.0f && out[1] == 0.0f, "no tilt keeps the aim");
        rush2049::tilt_up(aim, 30.0f, out);
        check(near(out[0], 0.8660254f) && near(out[1], 0.5f) && near(out[2], 0.0f), "30 degrees up");
        float identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
        rush2049::flat_push(3.0f, 4.0f, identity, 10.0f, out);
        check(near(out[0], 6.0f) && out[1] == 0.0f && near(out[2], 8.0f), "flat push along the shot");
    }
    {
        float f[rush2049::powerup_count];
        rush2049::powerup_fills(0, 30.0f, 30.0f, f);
        check(f[0] < 0.0f && f[1] < 0.0f, "no powerups: no bars");
        rush2049::powerup_fills(0x1, 7.5f, 0.0f, f);
        check(near(f[0], 0.25f) && f[1] < 0.0f, "invisible, a quarter left");
        rush2049::powerup_fills(0x2 | 0x8, 0.0f, 0.0333f, f);
        check(f[0] < 0.0f && f[1] == 1.0f, "shield growing: full");
        rush2049::powerup_fills(0x2 | 0x4, 0.0f, 15.0f, f);
        check(near(f[1], 0.5f), "shield, half left");
        rush2049::powerup_fills(0x2 | 0x10, 0.0f, 0.0f, f);
        check(f[1] < 0.0f, "shield shrinking: gone");
    }

    std::printf(failures == 0 ? "battle tuning: all passed\n" : "battle tuning: %d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
