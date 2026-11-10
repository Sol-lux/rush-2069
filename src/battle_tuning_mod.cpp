// Battle Tuning mod options. Battle Mode Alt's tuning values (rush2049::Tuning, include/rush2049_game.h) are the
// options of the game's own Battle Tuning mod (battle_tuning_mod/), so they are sliders in the Mods tab, beside the
// mod. Once a Battle frame (from the fire routine's player loop, src/battle_tuning.cpp) this reads each option by the
// mod's id; with the mod missing or off, the values go back to their defaults. Kept apart from battle_tuning.cpp so
// that file's test builds without the runtime.

#include "librecomp/mods.hpp"
#include "rush2049_game.h"

namespace {
constexpr const char* mod_id = "rush2049_battle_tuning";
}

void rush2049::poll_battle_tuning() {
    bool on = recomp::mods::is_mod_enabled(mod_id);
    for (int i = 0; i < tuning_count; i++) {
        float value = tuning_defaults[i];
        if (on) {
            recomp::config::ConfigValueVariant v = recomp::mods::get_mod_config_value(mod_id, tuning_ids[i]);
            if (const double* d = std::get_if<double>(&v)) {
                value = (float)*d;
            }
        }
        set_tuning(static_cast<Tuning>(i), value);
    }
}
