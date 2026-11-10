// Cheat Menu (General tab). The game hides its CHEATS screen behind a button combo on the main menu, and each cheat
// in it behind a combo of its own, and none of that is remembered between sessions. The game keeps one halfword per
// cheat, eleven of them from 0x8011648C; 0x0101 in one makes it available (the menu shows it and it can be switched
// on and off there) without switching it on.
//
// At Always Shown every one of them is set to 0x0101 on each retrace, so CHEATS is on the main menu from the start
// and every cheat in it can be set without its combo. The game clears them while it boots, at a moment that varies
// from launch to launch, so a single write isn't enough; only a flag the game has changed is written. Original
// leaves them alone.

#include <atomic>
#include <cstdint>

#include "recomp.h"
#include "rush2049_game.h"

namespace {

// Set from the settings menu, read on the game thread. It starts at the option's default (config.cpp).
std::atomic<bool> always_shown{ true };

constexpr uint32_t cheat_flags = 0x8011648C;
constexpr int cheat_count = 11;
constexpr int16_t cheat_available = 0x0101;

}  // namespace

void rush2049::set_cheat_menu_always_shown(bool on) {
    always_shown = on;
}

void rush2049::cheat_menu_retrace(uint8_t* rdram) {
    if (!always_shown.load(std::memory_order_relaxed)) {
        return;
    }
    for (int i = 0; i < cheat_count; i++) {
        gpr addr = (gpr)(int32_t)(cheat_flags + 2 * i);
        if ((int16_t)MEM_H(0, addr) != cheat_available) {
            MEM_H(0, addr) = cheat_available;
        }
    }
}
