// Config tabs: the stock tabs, plus the controller accessory, Controller Pak size, rewind, mouse cursor and Battle Mode
// Alt options on the General tab, the game frame rate, hi-res font, split-screen aspect ratio and copying frames to RAM on the Graphics
// tab, and MusyX mixing on the Sound tab.

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <chrono>
#include <thread>

#include "debug_probe.h"
#include "rush2049_game.h"
#include "rush2049_hooks.h"
#include "musyx_hle.h"
#include "librecomp/config.hpp"
#include "recompinput/input_mapping.h"
#include "recompinput/players.h"
#include "recompui/config.h"
#include "recompui/recompui.h"
#include "recompui/renderer.h"
#include "util/file.h"
#include "ultramodern/ultramodern.hpp"

// runtime-patches/rt64-0009: see-through surfaces drawn this many display lists deep go after the rest of their
// view, 0 for none. rt64-0012: bloom before the HUD, an intensity of 0 for none. rt64-0013: brightness, contrast and
// gamma over the whole picture.
namespace RT64 {
    void setTranslucentSort(uint32_t minListDepth);
    void setBloom(float intensity, float threshold, float radius);
    // rt64-0015: the textures that glow, by RT64's texture hash, each with a gain.
    void setGlowTextures(const uint64_t *hashes, const float *gains, uint32_t count);
    void setPictureAdjust(float brightness, float contrast, float gamma);
    // rt64-0014: HDR output, read when the swap chain is made.
    void setHdrOutput(bool enabled, float paperWhiteNits, float peakNits, float highlights);
    // rt64-0016: shadows from a sun, a strength of 0 for none (the same struct as RT64's rt64_state.h).
    struct ShadowSettings {
        float strength = 0.0f;
        float direction[3] = { 0.3f, -1.0f, 0.2f };
        float range = 5000.0f;
        float tint[3] = { 0.5f, 0.55f, 0.7f };
        bool debugView = false; // in shadow red, lit green, outside the shadow map blue
    };
    void setShadows(const ShadowSettings &settings);
}

namespace {
    enum class Accessory {
        ControllerPak,
        RumblePak,
        Both,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> accessory_options = {
        {Accessory::ControllerPak, "Controller Pak"},
        {Accessory::RumblePak, "Rumble Pak"},
        {Accessory::Both, "Both"},
    };
    const std::string accessory_option = "accessory";

    enum class Controllers {
        PerPlayer,
        OnePlayer,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> controllers_options = {
        {Controllers::PerPlayer, "PerPlayer", "Per Player"},
        {Controllers::OnePlayer, "OnePlayer", "One Player"},
    };
    const std::string controllers_option = "controllers";

    enum class PakSize {
        Original,
        Large,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> pak_size_options = {
        {PakSize::Original, "Original", "Original (32 KiB)"},
        {PakSize::Large, "Large", "Large (1.9 MB)"},
    };
    const std::string pak_size_option = "pak_size";

    enum class Rewind {
        Off,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> rewind_options = {
        {Rewind::Off, "Off", "Off"},
        {Rewind::On, "On", "On"},
    };
    const std::string rewind_option = "rewind";

    enum class SaveGhosts {
        Original,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> save_ghosts_options = {
        {SaveGhosts::Original, "Original", "Original"},
        {SaveGhosts::On, "On", "On"},
    };
    const std::string save_ghosts_option = "save_ghosts";

    enum class MouseCursor {
        Original,
        Shown,
        Moving,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> mouse_cursor_options = {
        {MouseCursor::Original, "Original", "Original"},
        {MouseCursor::Shown, "Shown", "Always Shown"},
        {MouseCursor::Moving, "Moving", "Shown While Moving"},
    };
    const std::string mouse_cursor_option = "mouse_cursor";

    enum class GameFrameRate {
        Original,
        Sixty,
        MatchFramerate,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> game_frame_rate_options = {
        {GameFrameRate::Original, "Original", "30 FPS (Original)"},
        {GameFrameRate::Sixty, "60", "60 FPS"},
        {GameFrameRate::MatchFramerate, "MatchFramerate", "Match Framerate"},
    };
    const std::string game_frame_rate_option = "game_frame_rate";

    enum class InputLag {
        Reduced,
        Original,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> input_lag_options = {
        {InputLag::Reduced, "Reduced", "Reduced"},
        {InputLag::Original, "Original", "Original"},
    };
    const std::string input_lag_option = "input_lag";

    enum class HiresFont {
        Off,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> hires_font_options = {
        {HiresFont::Off, "Off", "Off"},
        {HiresFont::On, "On", "On"},
    };
    const std::string hires_font_option = "hires_font";

    enum class HiresTitle {
        Original,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> hires_title_options = {
        {HiresTitle::Original, "Original", "Original"},
        {HiresTitle::On, "On", "On"},
    };
    const std::string hires_title_option = "hires_title";

    // src/draw_distance.cpp says what each limit is.
    enum class DrawDistance {
        Original,
        Far,
        Farthest,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> draw_distance_options = {
        {DrawDistance::Original, "Original", "Original"},
        {DrawDistance::Far, "Far", "4x"},
        {DrawDistance::Farthest, "Farthest", "16x"},
    };
    const std::string draw_distance_option = "draw_distance";

    enum class LevelOfDetail {
        Original,
        High,
        Highest,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> level_of_detail_options = {
        {LevelOfDetail::Original, "Original", "Original"},
        {LevelOfDetail::High, "High", "High"},
        {LevelOfDetail::Highest, "Highest", "Highest"},
    };
    const std::string level_of_detail_option = "level_of_detail";

    // src/draw_distance.cpp says what the game culls by.
    enum class SceneryCulling {
        Original,
        Reduced,
        Off,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> scenery_culling_options = {
        {SceneryCulling::Original, "Original", "Original"},
        {SceneryCulling::Reduced, "Reduced", "Reduced"},
        {SceneryCulling::Off, "Off", "Off"},
    };
    const std::string scenery_culling_option = "scenery_culling";

    // RT64's see-through order (runtime-patches/rt64-0009 to 0011, sortTranslucentCalls in its rt64_state.cpp). Sorted
    // moves the see-through draws made two display lists deep to after the view's last solid draw that deep, and draws
    // them picture by picture, far to near: the game's root list calls a list per scene node, which calls the node's
    // model, while its effects (the player's car, its flame, smoke, the ghosts' arrows) are called from the root list
    // and stay after them, in the game's order.
    enum class SeeThroughOrder {
        Original,
        Sorted,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> see_through_order_options = {
        {SeeThroughOrder::Original, "Original", "Original"},
        {SeeThroughOrder::Sorted, "Sorted", "Sorted"},
    };
    const std::string see_through_order_option = "see_through_order";

    enum class Bloom {
        Original,
        Low,
        Medium,
        High,
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> bloom_options = {
        {Bloom::Original, "Original", "Original"},
        {Bloom::Low, "Low", "Low"},
        {Bloom::Medium, "Medium", "Medium"},
        {Bloom::High, "High", "High"},
    };
    const std::string bloom_option = "bloom";

    enum class Shadows {
        Original,
        Light,
        Dark,
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> shadows_options = {
        {Shadows::Original, "Original", "Original"},
        {Shadows::Light, "Light", "Light"},
        {Shadows::Dark, "Dark", "Dark"},
    };
    const std::string shadows_option = "shadows";

    // Brightness, Contrast and Gamma: five steps each, Original in the middle (index 2).
    const std::vector<recomp::config::ConfigOptionEnumOption> brightness_options = {
        {0, "Darker2", "Darker 2"}, {1, "Darker", "Darker"}, {2, "Original", "Original"},
        {3, "Brighter", "Brighter"}, {4, "Brighter2", "Brighter 2"},
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> contrast_options = {
        {0, "Lower2", "Lower 2"}, {1, "Lower", "Lower"}, {2, "Original", "Original"},
        {3, "Higher", "Higher"}, {4, "Higher2", "Higher 2"},
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> gamma_options = {
        {0, "Darker2", "Darker 2"}, {1, "Darker", "Darker"}, {2, "Original", "Original"},
        {3, "Brighter", "Brighter"}, {4, "Brighter2", "Brighter 2"},
    };
    const std::string brightness_option = "brightness";
    const std::string contrast_option = "contrast";
    const std::string gamma_option = "gamma";
    uint32_t picture_brightness = 2, picture_contrast = 2, picture_gamma = 2;

    // Shadows (runtime-patches/rt64-0016-shadows.patch): the option's strength, and the developer flags' sun
    // direction and debug view (rush2049::set_shadow_tuning).
    uint32_t shadows_level = 0;
    float shadows_sun[3] = { 0.3f, -1.0f, 0.2f };
    bool shadows_debug = false;
    // Whether a track is loaded (rush2049::start_shadow_gate): the menus draw 3D models too (the spinning coin, the
    // items behind the text while they animate), which must cast nothing.
    std::atomic<bool> shadows_in_track{ false };
    void apply_shadows() {
        static constexpr float strength[] = { 0.0f, 0.5f, 0.8f };
        RT64::ShadowSettings settings;
        settings.strength = shadows_in_track.load() ? strength[shadows_level] : 0.0f;
        for (int i = 0; i < 3; i++) {
            settings.direction[i] = shadows_sun[i];
        }

        settings.debugView = shadows_debug;
        RT64::setShadows(settings);
    }
    void apply_picture() {
        static constexpr float brightness[] = { -0.08f, -0.04f, 0.0f, 0.04f, 0.08f };
        static constexpr float contrast[] = { 0.8f, 0.9f, 1.0f, 1.1f, 1.2f };
        static constexpr float gamma[] = { 1.25f, 1.12f, 1.0f, 0.89f, 0.8f };
        RT64::setPictureAdjust(brightness[picture_brightness], contrast[picture_contrast], gamma[picture_gamma]);
    }

    // HDR Output and its levels (runtime-patches/rt64-0014-hdr-output.patch, plume-0006, RecompFrontend-0009).
    const std::vector<recomp::config::ConfigOptionEnumOption> hdr_output_options = {
        {0, "Original", "Original"}, {1, "On", "On"},
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> hdr_paper_white_options = {
        {0, "100", "100 nits"}, {1, "150", "150 nits"}, {2, "200", "200 nits"}, {3, "250", "250 nits"}, {4, "300", "300 nits"},
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> hdr_peak_options = {
        {0, "400", "400 nits"}, {1, "600", "600 nits"}, {2, "800", "800 nits"}, {3, "1000", "1000 nits"}, {4, "1500", "1500 nits"},
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> hdr_highlights_options = {
        {0, "None", "None"}, {1, "Low", "Low"}, {2, "Medium", "Medium"}, {3, "High", "High"},
    };
    const std::string hdr_output_option = "hdr_output";
    const std::string hdr_paper_white_option = "hdr_paper_white";
    const std::string hdr_peak_option = "hdr_peak";
    const std::string hdr_highlights_option = "hdr_highlights";
    uint32_t hdr_output = 0, hdr_paper_white = 2, hdr_peak = 3, hdr_highlights = 2;
    void apply_hdr() {
        static constexpr float paper_white[] = { 100.0f, 150.0f, 200.0f, 250.0f, 300.0f };
        static constexpr float peak[] = { 400.0f, 600.0f, 800.0f, 1000.0f, 1500.0f };
        static constexpr float highlights[] = { 0.0f, 0.33f, 0.66f, 1.0f };
        RT64::setHdrOutput(hdr_output == 1, paper_white[hdr_paper_white], peak[hdr_peak], highlights[hdr_highlights]);
        recompui::renderer::set_hdr_ui_white(paper_white[hdr_paper_white]);
    }

    // src/battle_tuning.cpp.
    enum class BattleMode {
        Original,
        Alt,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> battle_mode_options = {
        {BattleMode::Original, "Original", "Original"},
        {BattleMode::Alt, "Alt", "Alt"},
    };
    const std::string battle_mode_option = "battle_mode";

    // src/cheat_menu.cpp.
    enum class CheatMenu {
        Original,
        AlwaysShown,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> cheat_menu_options = {
        {CheatMenu::Original, "Original", "Original"},
        {CheatMenu::AlwaysShown, "AlwaysShown", "Always Shown"},
    };
    const std::string cheat_menu_option = "cheat_menu";

    // src/ghost_arrows.cpp.
    enum class GhostArrows {
        Original,
        Whole,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> ghost_arrows_options = {
        {GhostArrows::Original, "Original", "Original"},
        {GhostArrows::Whole, "Whole", "Whole"},
    };
    const std::string ghost_arrows_option = "ghost_arrows";

    enum class SplitScreenRatio {
        Expand,
        Original,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> split_screen_ratio_options = {
        {SplitScreenRatio::Expand, "Expand", "Expand"},
        {SplitScreenRatio::Original, "Original", "Original"},
    };
    const std::string split_screen_ratio_option = "split_screen_ratio";
    // Read on the game thread with every display list.
    std::atomic<SplitScreenRatio> split_screen_ratio_setting{ SplitScreenRatio::Expand };

    enum class MenuRatio {
        Expand,
        Original,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> menu_ratio_options = {
        {MenuRatio::Expand, "Expand", "Expand"},
        {MenuRatio::Original, "Original", "Original"},
    };
    const std::string menu_ratio_option = "menu_ratio";
    // Read on the game thread with every display list.
    std::atomic<MenuRatio> menu_ratio_setting{ MenuRatio::Expand };

    enum class CopyFramesToRam {
        Off,
        Original,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> copy_frames_to_ram_options = {
        {CopyFramesToRam::Off, "Off", "Off"},
        {CopyFramesToRam::Original, "Original", "Original"},
    };
    const std::string copy_frames_to_ram_option = "copy_frames_to_ram";

    enum class MusyxMix {
        Original,
        HighQuality,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> musyx_mix_options = {
        {MusyxMix::Original, "Original", "Original"},
        {MusyxMix::HighQuality, "HighQuality", "High Quality"},
    };
    const std::string musyx_mix_option = "musyx_mix";

    // The Graphics tab asks for Apply before it closes (its stock options remake the renderer), so its callbacks
    // also hear every change the menu only previews, and the change undone again when the menu is closed with
    // Discard. The game's own options there act on load and on Apply only, like the stock ones, so the Apply prompt
    // is about changes that haven't happened yet.
    void on_apply(recomp::config::Config& graphics, const std::string& option_id, void (*apply)(uint32_t value)) {
        graphics.add_option_change_callback(option_id,
            [apply](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant,
                    recomp::config::OptionChangeContext change_context) {
                if (change_context != recomp::config::OptionChangeContext::Temporary) {
                    apply(std::get<uint32_t>(cur_value));
                }
            });
    }
}

bool rush2049::split_screen_expand() {
    return split_screen_ratio_setting.load(std::memory_order_relaxed) == SplitScreenRatio::Expand;
}

bool rush2049::menu_expand() {
    return menu_ratio_setting.load(std::memory_order_relaxed) == MenuRatio::Expand;
}

// --shadow-sun and --shadow-debug (docs/building.md): the sun's direction, and the shadow debug view.
void rush2049::set_shadow_tuning(const float *sun_direction, bool debug_view) {
    if (sun_direction != nullptr) {
        for (int i = 0; i < 3; i++) {
            shadows_sun[i] = sun_direction[i];
        }
    }

    shadows_debug = shadows_debug || debug_view;
    apply_shadows();
}

// Shadows only while a track is loaded: the list of scenery boxes Scenery Culling walks (the pointer at 0x80149B80,
// src/draw_distance.cpp) is set for a track and cleared in the menus, so it is checked ten times a second.
void rush2049::start_shadow_gate() {
    std::thread([]() {
        uint8_t* rdram;
        while ((rdram = rush2049::probe_get_rdram()) == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        while (true) {
            uint32_t boxes = *reinterpret_cast<const uint32_t*>(rdram + (0x80149B80 - 0x80000000));
            bool in_track = boxes >= 0x80000000 && boxes < 0x80800000;
            if (in_track != shadows_in_track.load()) {
                shadows_in_track = in_track;
                apply_shadows();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
}

static Accessory accessory() {
    auto value = recompui::config::get_general_config().get_option_value(accessory_option);
    return static_cast<Accessory>(std::get<uint32_t>(value));
}

bool rush2049::rumble_pak_selected() {
    return accessory() == Accessory::RumblePak;
}

bool rush2049::both_paks_selected() {
    return accessory() == Accessory::Both;
}

bool rush2049::save_ghosts() {
    auto value = recompui::config::get_general_config().get_option_value(save_ghosts_option);
    return static_cast<SaveGhosts>(std::get<uint32_t>(value)) == SaveGhosts::On;
}

bool rush2049::large_controller_pak() {
    auto value = recompui::config::get_general_config().get_option_value(pak_size_option);
    return static_cast<PakSize>(std::get<uint32_t>(value)) == PakSize::Large;
}

// Keyboard defaults for driving. The stock defaults put the C buttons on the arrow keys, where a PC player
// reaching for Up/Left would hit Abort (C-Up) and Reverse (C-Left). Here the arrows are a second stick (steering
// and menus, like WASD), gas and brake stay on Space and Left Shift, and the C buttons move to a T/F/G/H diamond.
// Only defaults: saved bindings and the game's own CONTROLS screen still apply.
static void set_keyboard_defaults() {
    using recompinput::GameInput;
    using recompinput::InputField;
    recompinput::set_default_mapping_for_keyboard(GameInput::X_AXIS_NEG,
        { InputField::keyboard(SDL_SCANCODE_A), InputField::keyboard(SDL_SCANCODE_LEFT) });
    recompinput::set_default_mapping_for_keyboard(GameInput::X_AXIS_POS,
        { InputField::keyboard(SDL_SCANCODE_D), InputField::keyboard(SDL_SCANCODE_RIGHT) });
    recompinput::set_default_mapping_for_keyboard(GameInput::Y_AXIS_POS,
        { InputField::keyboard(SDL_SCANCODE_W), InputField::keyboard(SDL_SCANCODE_UP) });
    recompinput::set_default_mapping_for_keyboard(GameInput::Y_AXIS_NEG,
        { InputField::keyboard(SDL_SCANCODE_S), InputField::keyboard(SDL_SCANCODE_DOWN) });
    recompinput::set_default_mapping_for_keyboard(GameInput::C_UP, { InputField::keyboard(SDL_SCANCODE_T) });
    recompinput::set_default_mapping_for_keyboard(GameInput::C_LEFT, { InputField::keyboard(SDL_SCANCODE_F) });
    recompinput::set_default_mapping_for_keyboard(GameInput::C_DOWN, { InputField::keyboard(SDL_SCANCODE_G) });
    recompinput::set_default_mapping_for_keyboard(GameInput::C_RIGHT, { InputField::keyboard(SDL_SCANCODE_H) });
}

// Gamepad defaults for driving on a modern pad (Xbox, PlayStation, Switch Pro; any pad SDL knows as a game
// controller). The stock defaults are an N64 layout: gas and brake on the A and X face buttons, the triggers on
// R (shift up) and Z, and shift down on RB. Here the triggers are gas and brake, the bumpers shift, view moves
// to Y and Z to B. The face buttons keep gas and brake for menus, and the right stick stays the C buttons.
// Only defaults: saved bindings and the game's own CONTROLS screen still apply.
static void set_controller_defaults() {
    using recompinput::GameInput;
    using recompinput::InputField;
    recompinput::set_default_mapping_for_controller(GameInput::A, {
        InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true),
        InputField::controller_digital(recompinput::SDL_CONTROLLER_BUTTON_SOUTH) });
    recompinput::set_default_mapping_for_controller(GameInput::B, {
        InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true),
        InputField::controller_digital(recompinput::SDL_CONTROLLER_BUTTON_WEST) });
    recompinput::set_default_mapping_for_controller(GameInput::R,
        { InputField::controller_digital(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) });
    recompinput::set_default_mapping_for_controller(GameInput::C_DOWN, {
        InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, true),
        InputField::controller_digital(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) });
    recompinput::set_default_mapping_for_controller(GameInput::L,
        { InputField::controller_digital(recompinput::SDL_CONTROLLER_BUTTON_NORTH) });
    recompinput::set_default_mapping_for_controller(GameInput::C_LEFT,
        { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, false) });
    recompinput::set_default_mapping_for_controller(GameInput::Z,
        { InputField::controller_digital(recompinput::SDL_CONTROLLER_BUTTON_EAST) });
    recompinput::set_default_mapping_for_controller(GameInput::C_RIGHT,
        { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, true) });
}

void rush2049::init_config() {
    set_keyboard_defaults();
    set_controller_defaults();

    std::filesystem::path app_dir = recompui::file::get_app_folder_path();
    if (!app_dir.empty()) {
        std::filesystem::create_directories(app_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    general_options.has_mouse_sensitivity = false;
    recomp::config::Config& general = recompui::config::create_general_tab(general_options);
    general.add_enum_option(
        accessory_option,
        "Controller Accessory",
        "What is plugged into each controller. A real controller has one accessory slot, so the game was made for "
        "either saving or rumble."
        "<br/>"
        "<b>Controller Pak:</b> saves players, records and ghosts. "
        "<b>Rumble Pak:</b> rumble, but the game can't save. "
        "<b>Both:</b> saves, and rumble in races.",
        accessory_options,
        Accessory::Both
    );
    // Per Player is RecompFrontend's multiplayer mode: each player has a port of their own and each controller a
    // profile of its own, and a controller becomes the next player the first time it presses a button
    // (RecompFrontend-0003), or players are put in order with Assign Players (Controls page). Nothing of it is saved,
    // so players join again each session. Until a controller joins it plays as One Player, which is its single-player
    // mode: every controller and the keyboard on port 1 through one shared profile, so the Controls page shows the
    // keyboard's keys then (RECOMPINPUT_SINGLE_PLAYER_UNTIL_ASSIGNED, CMakeLists.txt). Player 1 keeps those keyboard
    // keys after a controller joins, and the page's controller and keyboard toggle shows them; a player given the
    // keyboard starts from them (N64Recomp-browser's RecompFrontend patch, with the option). The Controls page reads
    // the mode each time it draws, so switching needs no restart.
    general.add_enum_option(
        controllers_option,
        "Controllers",
        "How controllers and the keyboard map to players."
        "<br/>"
        "<b>Per Player:</b> each controller is a player of its own, as on the console. The first controller to press a "
        "button is player 1, the next player 2, and so on, and the keyboard drives player 1. Assign Players on the "
        "Controls tab puts them in another order or gives the keyboard to another player. "
        "<b>One Player:</b> every controller and the keyboard drive player 1.",
        controllers_options,
        Controllers::PerPlayer
    );
    general.add_enum_option(
        pak_size_option,
        "Controller Pak Size",
        "How much each Controller Pak holds. A player profile takes 9 of a pak's pages, and a pak holds 16 files "
        "whatever its size."
        "<br/>"
        "<b>Original:</b> Nintendo's 32 KiB pak, 123 pages, about 13 profiles. "
        "<b>Large:</b> 62 banks, 7748 pages, the most libultra can address, so it never fills up. "
        "Emulators that only take 32 KiB paks can't read it."
        "<br/>"
        "Saves are moved to the new size the next time the game starts. A pak whose saves don't fit in 32 KiB "
        "stays large.",
        pak_size_options,
        PakSize::Large
    );
    general.add_enum_option(
        rewind_option,
        "Rewind",
        "Hold <recomp-color primary>Rewind</recomp-color> (Controls tab) in a single-player race and the race runs "
        "backwards: every car, the clock, the laps and the ghost. Let go to drive on from there. Goes back up to about "
        "a minute, and uses up to 128 MB of memory for it.",
        rewind_options,
        Rewind::On
    );
    general.add_enum_option(
        save_ghosts_option,
        "Save Ghosts",
        "Whether a Ghost Race you finish inside the time limit can be saved to the Controller Pak, to race against "
        "in later sessions as GHOST 2 to 4."
        "<br/>"
        "<b>On:</b> the game's own SAVE GHOST prompt comes up after every such race. "
        "<b>Original:</b> as the game shipped, a ghost lasts only until you quit, as GHOST 1 on the same track. "
        "The game saves one only for a developer's profile.",
        save_ghosts_options,
        SaveGhosts::On
    );
    general.add_enum_option(
        mouse_cursor_option,
        "Mouse Cursor",
        "Whether the mouse cursor stays on screen while you play with a controller or the keyboard."
        "<br/>"
        "<b>Original:</b> hidden as soon as a controller or a key is pressed, and shown again when the mouse moves. "
        "<b>Always Shown:</b> stays where you left it, for pointing at things in a recording. "
        "<b>Shown While Moving:</b> shown whenever the mouse moves, with a controller too, and hidden once it has "
        "been still for two seconds.",
        mouse_cursor_options,
        MouseCursor::Original
    );
    general.add_enum_option(
        battle_mode_option,
        "Battle Mode Alt",
        "Gameplay changes to Battle, all together."
        "<br/>"
        "<b>Original:</b> Battle as the game shipped. "
        "<b>Alt:</b> cars take half damage, so they last twice as long, and the machine gun winds up like a chain "
        "gun: 5 rounds a second from rest, up to 30 over a second and a half of firing, holding its speed for a "
        "moment after you let go, then winding down over a second and a half.",
        battle_mode_options,
        BattleMode::Alt
    );
    general.add_enum_option(
        cheat_menu_option,
        "Cheat Menu",
        "Whether the game's CHEATS menu needs its button combos."
        "<br/>"
        "<b>Original:</b> CHEATS appears on the main menu after its combo, and each cheat in it needs a combo of its "
        "own, every time the game starts. "
        "<b>Always Shown:</b> CHEATS is on the main menu from the start, and every cheat in it can be switched on "
        "and off there without a combo. No cheat is switched on by this.",
        cheat_menu_options,
        CheatMenu::AlwaysShown
    );
    // Called on load too.
    general.add_option_change_callback(controllers_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            recompinput::players::set_single_player_mode(
                static_cast<Controllers>(std::get<uint32_t>(cur_value)) == Controllers::OnePlayer);
        });
    general.add_option_change_callback(rewind_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_rewind(static_cast<Rewind>(std::get<uint32_t>(cur_value)) == Rewind::On);
        });
    general.add_option_change_callback(cheat_menu_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_cheat_menu_always_shown(static_cast<CheatMenu>(std::get<uint32_t>(cur_value)) == CheatMenu::AlwaysShown);
        });
    general.add_option_change_callback(battle_mode_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_battle_mode_alt(static_cast<BattleMode>(std::get<uint32_t>(cur_value)) == BattleMode::Alt);
        });
    // RecompFrontend-0007 and -0008.
    general.add_option_change_callback(mouse_cursor_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            MouseCursor cursor = static_cast<MouseCursor>(std::get<uint32_t>(cur_value));
            recompui::set_cursor_always_visible(cursor == MouseCursor::Shown);
            recompui::set_cursor_shown_while_moving(cursor == MouseCursor::Moving);
        });

    recomp::config::Config& graphics = recompui::config::create_graphics_tab();
    graphics.add_enum_option(
        game_frame_rate_option,
        "Game Frame Rate",
        "How often the game draws a new frame. The original draws on every other screen refresh, 30 times a second. "
        "Car physics and timers follow real time, so a race runs at the same speed at any rate."
#ifdef __EMSCRIPTEN__
        " In a browser, 60 FPS draws 30 while the device can't keep 60 up, and tries 60 again now and then."
#endif
        "<br/>"
        "<b>Match Framerate:</b> a new frame for every frame <recomp-color primary>Framerate</recomp-color> above "
        "presents, so Display draws at the monitor's refresh rate and Manual at its value (60 at least).",
        game_frame_rate_options,
        GameFrameRate::Sixty
    );
    // Called on load too.
    on_apply(graphics, game_frame_rate_option, [](uint32_t value) {
        static constexpr uint32_t fps[] = { 30, 60, 0 };  // Original, 60, Match Framerate
        rush2049::set_game_frame_rate(fps[value]);
    });

    graphics.add_enum_option(
        input_lag_option,
        "Input Lag",
        "<b>Reduced:</b> each frame is drawn as soon as the game has worked it out, so what you press shows on screen "
        "one frame sooner (17 ms at 60 fps). At 30 FPS (Original) Game Frame Rate it changes nothing."
        "<br/>"
        "<b>Original:</b> each frame waits for the next refresh before it is drawn, as on the console.",
        input_lag_options,
        InputLag::Reduced
    );
    on_apply(graphics, input_lag_option, [](uint32_t value) {
        rush2049::set_reduced_input_lag(static_cast<InputLag>(value) == InputLag::Reduced);
    });

    rush2049::init_hires_font();
    graphics.add_enum_option(
        hires_font_option,
        "Hi-res Font",
        "Draws the menu and HUD text from smooth, 4x upscaled copies of the game's fonts instead of the blocky "
        "originals. The copies are made from your ROM as each font first appears, and kept in the app folder."
        "<br/>"
        "Texture packs you enable take priority.",
        hires_font_options,
        HiresFont::On
    );
    on_apply(graphics, hires_font_option, [](uint32_t value) {
        rush2049::set_hires_font(static_cast<HiresFont>(value) == HiresFont::On);
    });

    graphics.add_enum_option(
        hires_title_option,
        "Hi-res Title Screen",
        "Draws the title screen from a 4x upscale of its picture (made with Real-ESRGAN) instead of the 240p original. "
        "The game rebuilds the upscale from its own picture and a patch that ships with it."
        "<br/>"
        "Texture packs you enable take priority.",
        hires_title_options,
        HiresTitle::On
    );
    on_apply(graphics, hires_title_option, [](uint32_t value) {
        rush2049::set_hires_title(static_cast<HiresTitle>(value) == HiresTitle::On);
    });

    graphics.add_enum_option(
        draw_distance_option,
        "Draw Distance",
        "How far away scenery is still drawn, as a multiple of the original distance. Farther scenery, like a building "
        "far down the road, appears sooner instead of popping in. Most of what the original leaves out is hidden "
        "behind something nearer anyway, so the difference shows only in places.",
        draw_distance_options,
        DrawDistance::Far
    );
    // Called on load too.
    on_apply(graphics, draw_distance_option, [](uint32_t value) {
        static constexpr float scale[] = { 1.0f, 4.0f, 16.0f };
        rush2049::set_draw_distance(scale[value]);
    });

    graphics.add_enum_option(
        level_of_detail_option,
        "Level of Detail",
        "How far away models keep their detailed versions before the game swaps in simpler ones. The original "
        "simplifies them close to the car: curves go blocky and small details like traffic lights disappear."
        "<br/>"
        "<b>High:</b> four times the original distances. <b>Highest:</b> always the most detailed version.",
        level_of_detail_options,
        LevelOfDetail::High
    );
    // Called on load too.
    on_apply(graphics, level_of_detail_option, [](uint32_t value) {
        static constexpr float scale[] = { 1.0f, 4.0f, 1e30f };
        rush2049::set_level_of_detail(scale[value]);
    });

    graphics.add_enum_option(
        scenery_culling_option,
        "Scenery Culling",
        "The original game leaves out the scenery it reckons can't be seen from the stretch of road you are on, so "
        "buildings, signs and roadside pieces pop in and out as you drive from one stretch into the next, some of "
        "them close to the car."
        "<br/>"
        "<b>Reduced:</b> what can be seen from the stretches of road around you is drawn too, and the tallest "
        "towers and any building that already looks more than a sliver tall from where you are, wherever they are in "
        "view, so scenery comes in while it is still small, for much less than Off costs. <b>Off:</b> all of the "
        "track's scenery is drawn wherever you are, and only <recomp-color primary>Draw Distance</recomp-color> "
        "decides how far.",
        scenery_culling_options,
#ifdef __EMSCRIPTEN__
        // Off costs a page about a third of its frames (docs/web.md), so a browser starts at Original.
        SceneryCulling::Original
#else
        SceneryCulling::Off
#endif
    );
    // Called on load too.
    on_apply(graphics, scenery_culling_option, [](uint32_t value) {
        static constexpr float reach[] = { 0.0f, 500.0f, -1.0f };
        rush2049::set_scenery_culling(reach[value]);
    });

    graphics.add_enum_option(
        see_through_order_option,
        "See-Through Order",
        "Trees, lamps, sails and glass are see-through pictures, and the original game draws each one wherever it "
        "comes in its list, so solid scenery behind one that is drawn after it covers it: a tree goes missing in front "
        "of some walls and not others."
        "<br/>"
        "<b>Sorted:</b> see-through scenery is drawn after the rest of the scene, farthest first, so it shows over "
        "whatever is behind it, a sail over the boats behind it as well as over a wall. Your car's flame, the smoke "
        "and the ghosts' arrows are drawn over it as they were.",
        see_through_order_options,
        SeeThroughOrder::Sorted
    );
    // Called on load too.
    on_apply(graphics, see_through_order_option, [](uint32_t value) {
        RT64::setTranslucentSort(value == (uint32_t)SeeThroughOrder::Sorted ? 2 : 0);
    });

    graphics.add_enum_option(
        bloom_option,
        "Bloom",
        "A soft glow around the bright parts of the picture: the sky, lights, flames and shine on the cars. The HUD "
        "doesn't glow."
        "<br/>"
        "<b>Original:</b> no glow, as the game shipped. <b>Low</b>, <b>Medium</b> and <b>High</b> add more of it.",
        bloom_options,
        Bloom::Original
    );
    // Called on load too. RT64's bloom (runtime-patches/rt64-0012-bloom.patch): intensity, threshold, blur radius.
    on_apply(graphics, bloom_option, [](uint32_t value) {
        static constexpr float intensity[] = { 0.0f, 0.3f, 0.5f, 0.8f };
        RT64::setBloom(intensity[value], 0.7f, 6.0f);

        // The textures that glow with bloom on: their draws are drawn again into a mask, and its bright parts glow
        // whatever the threshold (rt64-0015). By RT64's texture hash (--dump-textures names each file by it), picked
        // by eye in the attract race on Track 1: the sunset strip of the sky, the light strips of the arrow signs (one
        // texture per stage of their chase; only the lit parts are bright enough to glow), the green arrow lights,
        // the white light panel, the fire, and the white streaks of light.
        struct Glow { uint64_t hash; float gain; };
        static constexpr Glow glow[] = {
            { 0xff7702143e532e89, 1.0f }, { 0x4c54eb90cd773e21, 1.0f }, { 0xa835fce4d0a61897, 1.0f }, // sky strip
            { 0xf7939109599e735e, 1.0f }, { 0x040ea08fc05afc89, 1.0f }, { 0xa6fa1f8136a0ecf8, 1.0f },
            { 0x9631d1419b3d8b35, 1.0f }, { 0x6c9fd965090dfdc7, 1.0f }, { 0x32725ed8645b3a19, 1.0f },
            { 0x0cb4d09fa3aa5ab8, 1.0f },
            { 0xdef7de9ed80039f1, 1.5f }, { 0x8fc1367a77baa62d, 1.5f }, { 0xd094d785decb2c3d, 1.5f }, // arrow signs
            { 0x1cac697c8d17ff06, 1.5f }, // green arrow lights
            { 0x2e4027f6b59b53a5, 1.5f }, // light panel
            { 0x4f7db91da190b728, 1.5f }, // fire
            { 0x45176efd102ac815, 1.0f }, // streaks of light
        };
        uint64_t hashes[std::size(glow)];
        float gains[std::size(glow)];
        for (size_t i = 0; i < std::size(glow); i++) {
            hashes[i] = glow[i].hash;
            gains[i] = glow[i].gain;
        }

        RT64::setGlowTextures(hashes, gains, value == (uint32_t)Bloom::Original ? 0 : uint32_t(std::size(glow)));
    });

    graphics.add_enum_option(
        shadows_option,
        "Shadows",
        "Shadows from the sun: the cars, buildings, bridges and signs cast them onto the road and onto each other."
        "<br/>"
        "<b>Original:</b> no shadows but the cars' own, as the game shipped. <b>Light</b> and <b>Dark</b> are how "
        "deep they are.",
        shadows_options,
        Shadows::Original
    );
    // Called on load too. RT64's sun shadows (runtime-patches/rt64-0016-shadows.patch).
    on_apply(graphics, shadows_option, [](uint32_t value) { shadows_level = std::min(value, 2u); apply_shadows(); });

    graphics.add_enum_option(
        brightness_option,
        "Brightness",
        "Lighter or darker over the whole picture, the HUD too. <b>Original:</b> as the game shipped.",
        brightness_options,
        2
    );
    // Called on load too. RT64's VI pass (runtime-patches/rt64-0013-picture-adjust.patch).
    on_apply(graphics, brightness_option, [](uint32_t value) { picture_brightness = std::min(value, 4u); apply_picture(); });

    graphics.add_enum_option(
        contrast_option,
        "Contrast",
        "More or less difference between the dark and light parts of the picture, the HUD too. <b>Original:</b> as "
        "the game shipped.",
        contrast_options,
        2
    );
    on_apply(graphics, contrast_option, [](uint32_t value) { picture_contrast = std::min(value, 4u); apply_picture(); });

    graphics.add_enum_option(
        gamma_option,
        "Gamma",
        "Lighter or darker mid-tones, keeping black black and white white, the HUD too. <b>Original:</b> as the game "
        "shipped.",
        gamma_options,
        2
    );
    on_apply(graphics, gamma_option, [](uint32_t value) { picture_gamma = std::min(value, 4u); apply_picture(); });

    graphics.add_enum_option(
        hdr_output_option,
        "HDR Output",
        "For an HDR display with HDR turned on in Windows: the picture is sent as HDR, so the brightest parts (the sky, "
        "lights, flames) can go brighter than white. Takes effect the next time the game starts."
        "<br/>"
        "<b>Original:</b> an SDR picture, as the game shipped. <b>On:</b> HDR, set with the three options below.",
        hdr_output_options,
        0
    );
    // Called on load too, before the renderer starts. RT64 reads it once, when it makes the swap chain.
    on_apply(graphics, hdr_output_option, [](uint32_t value) { hdr_output = std::min(value, 1u); apply_hdr(); });

    graphics.add_enum_option(
        hdr_paper_white_option,
        "HDR Brightness",
        "With HDR Output on: how bright the game's white is, the HUD and menus too. Pick what matches the rest of the "
        "desktop.",
        hdr_paper_white_options,
        2
    );
    on_apply(graphics, hdr_paper_white_option, [](uint32_t value) { hdr_paper_white = std::min(value, 4u); apply_hdr(); });

    graphics.add_enum_option(
        hdr_peak_option,
        "HDR Peak Brightness",
        "With HDR Output on: the brightest the display can go. Nothing is sent brighter than this.",
        hdr_peak_options,
        3
    );
    on_apply(graphics, hdr_peak_option, [](uint32_t value) { hdr_peak = std::min(value, 4u); apply_hdr(); });

    graphics.add_enum_option(
        hdr_highlights_option,
        "HDR Highlights",
        "With HDR Output on: how far the brightest parts of the picture are lifted above white, towards the peak "
        "brightness. <b>None:</b> the picture as SDR shows it, at the HDR Brightness.",
        hdr_highlights_options,
        2
    );
    on_apply(graphics, hdr_highlights_option, [](uint32_t value) { hdr_highlights = std::min(value, 3u); apply_hdr(); });

    graphics.add_enum_option(
        ghost_arrows_option,
        "Ghost Arrows",
        "The arrow over a ghost is drawn before part of the scenery in the original game, so buildings far behind a "
        "ghost can cover its arrow while the ghost is in plain view."
        "<br/>"
        "<b>Whole:</b> the arrows are drawn after the scenery, each one whole, in front of everything, while its tip "
        "above the ghost can be seen, and not at all while something hides the tip.",
        ghost_arrows_options,
        GhostArrows::Whole
    );
    // Called on load too.
    on_apply(graphics, ghost_arrows_option, [](uint32_t value) {
        rush2049::set_ghost_arrows_whole(static_cast<GhostArrows>(value) == GhostArrows::Whole);
    });

    graphics.add_enum_option(
        split_screen_ratio_option,
        "Split-Screen Aspect Ratio",
        "How the 2- and 4-player views fit a wide window. The original game splits a 4:3 screen, so at 16:9 the "
        "views sit inside a 4:3 area with bars at the sides."
        "<br/>"
        "<b>Expand:</b> each view widens to fill its share of the window, and its HUD moves with it. Needs "
        "<recomp-color primary>Aspect Ratio</recomp-color> at Expand and "
        "<recomp-color primary>HUD Placement</recomp-color> at something other than Original.",
        split_screen_ratio_options,
        SplitScreenRatio::Expand
    );
    on_apply(graphics, split_screen_ratio_option, [](uint32_t value) {
        split_screen_ratio_setting = static_cast<SplitScreenRatio>(value);
    });

    graphics.add_enum_option(
        menu_ratio_option,
        "Menu Aspect Ratio",
        "How the menus fit a wide window. The original game draws them on a 4:3 picture, so at 16:9 there are bars "
        "at the sides."
        "<br/>"
        "<b>Expand:</b> the menu stays where it is and the background picture is mirrored out to the window's edges "
        "(up to about 21:9). Needs <recomp-color primary>Aspect Ratio</recomp-color> at Expand.",
        menu_ratio_options,
        MenuRatio::Expand
    );
    on_apply(graphics, menu_ratio_option, [](uint32_t value) {
        menu_ratio_setting = static_cast<MenuRatio>(value);
    });

    // RT64's render to RAM, which draws every frame a second time at the N64's resolution and copies it into RDRAM,
    // where the console's RDP leaves it. Nothing in the game was seen reading a frame on the CPU: with page guards on
    // the framebuffers and the Z-buffer through boot, the attract races, the menus, a race with pause, Rewind and
    // Quit, two-player split screen and a Ghost Race to its results, the game's only touches were the ROM inflate's
    // staging buffer in a framebuffer it isn't showing, and no screen samples a framebuffer as a texture. Without the
    // copy a race list on an RTX 2070 at 960x720 took 3.9 ms instead of 5.0 and the GPU drew 41-49 W instead of
    // 60-62, and in a browser a list waited ~6 ms less for the GPU process (13-14 ms a list with it, 5-6 without).
    // Still, a game that never reads its frames can behave differently without them (a mod, or a path nobody
    // guarded), so the copy is an option. The render context reads it when it is made, at startup after this
    // loads, so a change takes effect on the next start. Its default is set here first: a first start has no
    // graphics.json, and then loading calls no callback, which would leave RT64's own default (on).
    recompui::renderer::set_render_to_ram(false);
    graphics.add_enum_option(
        copy_frames_to_ram_option,
        "Copy Frames to RAM",
        "Whether every frame is also copied back into the console's memory, as the N64 leaves it there for the game "
        "to read. The copy is a second draw of each frame at 320x240, and in a browser it holds each frame back by "
        "about 6 ms."
        "<br/>"
        "<b>Off:</b> no copy. The game isn't known to read its frames back. "
        "<b>Original:</b> every frame is copied, as on the console. Try it if something looks or behaves wrong."
        "<br/>"
        "Takes effect the next time the game is started.",
        copy_frames_to_ram_options,
        CopyFramesToRam::Off
    );
    // Called on load too, before the render context is made.
    on_apply(graphics, copy_frames_to_ram_option, [](uint32_t value) {
        recompui::renderer::set_render_to_ram(
            static_cast<CopyFramesToRam>(value) == CopyFramesToRam::Original);
    });

    recompui::config::create_controls_tab();

    recomp::config::Config& sound = recompui::config::create_sound_tab();
    sound.add_enum_option(
        musyx_mix_option,
        "MusyX Mixing",
        "<b>High Quality</b> mixes the game's music and sound effects at 44.1 kHz with a sharper interpolator, so "
        "sounds played above their recorded pitch keep their treble and loud passages don't clip. "
        "<b>Original</b> plays the game's own 22 kHz mix.",
        musyx_mix_options,
        MusyxMix::HighQuality
    );
    // Called on load too.
    sound.add_option_change_callback(musyx_mix_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_musyx_hires(static_cast<MusyxMix>(std::get<uint32_t>(cur_value)) == MusyxMix::HighQuality);
        });

    recompui::config::create_mods_tab();

    recompui::config::finalize();
}
