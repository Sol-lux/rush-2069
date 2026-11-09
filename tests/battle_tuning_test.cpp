// Battle Mode Alt (src/battle_tuning.cpp) checks, run by CI: at Original the damage and the machine gun's reload are
// left alone; Alt halves damage, rounding up, and the machine gun fires 5 rounds a second from rest,
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

    std::printf(failures == 0 ? "battle tuning: all passed\n" : "battle tuning: %d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
