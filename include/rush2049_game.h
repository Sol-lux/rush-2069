#ifndef RUSH2049_GAME_H
#define RUSH2049_GAME_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rush2049 {
    inline const std::u8string program_id = u8"Rush2049Recompiled";
    inline const std::string program_name = "San Francisco Rush 2049: Recompiled";

    void register_overlays();
    // Mods (README, "Mods"): the patches library (src/register_patches.cpp), the functions the
    // executable exports to mods (src/mod_exports.cpp), and the recomp ROM rebuilt for hooking (src/code_loading.cpp).
    void register_patches();
    void register_mod_exports();
    std::vector<uint8_t> build_recomp_rom(std::span<const uint8_t> rom);
    void init_config();
    // The accessory in every controller: a Controller Pak (saves), a Rumble Pak (rumble, no saves), or both at once.
    bool rumble_pak_selected();
    bool both_paks_selected();
    // The "Controller Pak Size" setting: 62 banks (about 1.9 MB) rather than the original 32 KiB.
    bool large_controller_pak();
    // The "Save Ghosts" setting: the SAVE GHOST prompt after every finished Ghost Race, not only a developer's.
    bool save_ghosts();
    // Frames a second the game draws at the "Game Frame Rate" setting: 30, 60, or more with Match Framerate.
    // The frame rate the Game Frame Rate option asks for; 0 is Match Framerate (src/frame_rate.cpp).
    void set_game_frame_rate(uint32_t fps);
    uint32_t game_frame_rate();
    // The "Input Lag" setting: a frame is drawn as soon as the game queues it (Reduced), not on the next retrace.
    void set_reduced_input_lag(bool reduced);
    // Whether 2- and 4-player views widen with the window ("Split-Screen Aspect Ratio" at Expand).
    bool split_screen_expand();
    // Whether the menus' background is mirrored out to a wide window's edges ("Menu Aspect Ratio" at Expand).
    bool menu_expand();
    // Where the game's own commands start in a graphics task's list: after the two commands the frame's list starts with
    // (src/frame_list.cpp).
    uint32_t frame_list_game_start(uint32_t task_list);
    // --dump-dl: write the first display list the game submits after the given number of seconds (src/dl_dump.cpp).
    void start_dl_dump(const char* path, double after_seconds);
    // --dump-ram: write all of RDRAM (big-endian) to <prefix>N.bin at each of the comma-separated times in seconds.
    void start_ram_dump(const char* prefix, const char* times);
    // --input: whether the input script holds Rewind right now (src/main.cpp).
    bool scripted_rewind();
    // Rewind (src/rewind.cpp): the General tab's Rewind option.
    void set_rewind(bool enabled);
    // Whether the last frame the game drew was split screen (src/widescreen.cpp).
    bool split_screen();

    // Hi-res font (src/hires_font.cpp): set up the generated texture pack in the app folder, then turn it on or off.
    void init_hires_font();
    void set_hires_font(bool on);
    // Hi-res title screen (src/hires_font.cpp, set up by init_hires_font): the title cut from assets/title.patch.
    void set_hires_title(bool on);
    // Draw Distance and Level of Detail (src/draw_distance.cpp): how far the distance limits reach, and the LOD switch
    // distances, as multiples of the game's own; 1 is Original.
    void set_draw_distance(float scale);
    void set_level_of_detail(float scale);
    // Scenery Culling (src/draw_distance.cpp): 0 is Original, each stretch of road's own visibility mask; a reach
    // above 0 also lets through what every stretch within that many units of the camera lets through (Reduced); a
    // negative one draws every scenery object from every stretch (Off).
    void set_scenery_culling(float reach);
    // Ghost Arrows (src/ghost_arrows.cpp): whether the ghosts' arrows are drawn after the scenery, each whole or not at
    // all by whether its tip can be seen (Whole), rather than amid it and covered pixel by pixel (Original).
    void set_ghost_arrows_whole(bool on);
    bool ghost_arrows_whole();
    // Battle Mode Alt (src/battle_tuning.cpp): every Battle gameplay mod on (Alt) or Battle as it shipped (Original).
    void set_battle_mode_alt(bool on);
    constexpr float spin_min_rate = 5.0f;      // rounds a second from rest
    constexpr float spin_max_rate = 30.0f;     // at full spin
    constexpr float spin_up_seconds = 1.5f;    // held, from rest to full
    constexpr float spin_hold_seconds = 20.0f / 60.0f;  // released, before it spins down
    constexpr float spin_down_seconds = 1.5f;  // from full to rest
    // The spin level after a shot, from the level at the last one, the seconds since it (negative: the first) and the
    // reload given then; and the reload for a level.
    float spin_after_shot(float level, float gap, float last_interval);
    float spin_interval(float level);
    // Battle Mode Alt's Rammer: its refire, and the push a ram gives: rammer_push_force (the force an impact gives is
    // 330000) times the rammer's speed over the reference, clamped, flat along the ground away from the rammer.
    constexpr float rammer_refire_seconds = 1.0f;
    constexpr float rammer_push_force = 800000.0f;
    constexpr float rammer_push_reference_speed = 80.0f;
    constexpr float rammer_push_min_scale = 0.25f;
    constexpr float rammer_push_max_scale = 1.25f;
    // A ram's damage, before double health halves it: 200 + 5 x speed (as shipped 160 + 7 x speed).
    constexpr float rammer_damage_base = 200.0f;
    constexpr float rammer_damage_per_speed = 5.0f;
    float rammer_damage(float speed);
    // The push on the target's car, in its frame: target and hitter are record positions (+0x8), matrix the target
    // record's (+0x2C), speed the rammer car's (+0x3F0).
    void rammer_push(const float target[3], const float hitter[3], const float matrix[9], float speed, float out[3]);
}

#endif
