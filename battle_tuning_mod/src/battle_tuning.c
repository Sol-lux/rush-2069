// Battle Tuning: the game's own mod for tuning Battle Mode Alt in the Mods tab. It has no gameplay code: its options
// (mod.toml) are read by the game (src/battle_tuning.cpp, by this mod's id) while Battle Mode Alt is on. With the mod
// off or missing, the game uses the values in mod.toml's defaults.
#include "rush2049_api.h"

RECOMP_HOOK("boot_main") void battle_tuning_on_boot(void) {
    recomp_printf("[battle tuning] options in the Mods tab\n");
}
