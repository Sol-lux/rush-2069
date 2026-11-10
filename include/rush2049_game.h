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
    // Where the game's own commands start in the frame's list being built.
    uint32_t frame_list_current(uint8_t* rdram);
    // --dump-dl: write the first display list the game submits after the given number of seconds (src/dl_dump.cpp).
    void start_dl_dump(const char* path, double after_seconds);
    // --dump-ram: write all of RDRAM (big-endian) to <prefix>N.bin at each of the comma-separated times in seconds.
    void start_ram_dump(const char* prefix, const char* times);
    // --input: whether the input script holds Rewind right now (src/main.cpp).
    bool scripted_rewind();
    // Rewind (src/rewind.cpp): the General tab's Rewind option.
    void set_rewind(bool enabled);
    // The race's memory, as offsets into RDRAM: what Rewind saves (src/rewind.cpp).
    struct MemoryRange {
        uint32_t lo, hi;
    };
    const std::vector<MemoryRange>& race_memory();
    // The sync probe (src/sync_probe.cpp): --sync-record, --sync-replay, --sync-hash, --sync-dump, --sync-random, --sync-hw and --sync-seed.
    void sync_record(const char* path);
    void sync_replay(const char* path);
    void sync_hash(const char* path);
    void sync_dump(const char* prefix, const char* reads);
    void sync_random(const char* path);
    void sync_seed(uint32_t seed);
    // --sync-hw: where the game meets the machine's timing. kind 'S' thread 5 steps, 'F' thread 7 at a wait for a
    // frame (value: the hook's address), 'R' the retraces a frame really counted; sync_hw_spin a spin yield.
    void sync_hw(const char* path);
    void sync_hw_event(uint8_t* rdram, char kind, uint32_t value);
    void sync_hw_spin(uint8_t* rdram);
    // The frame lock (src/frame_lock.cpp): --frame-lock, one frame of the frame loop for each step, 1/60 s each.
    void set_frame_lock(bool on);
    bool frame_lock();
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
    // Shadows' developer flags (src/config.cpp): the sun's direction (null to keep it) and the debug view (once on, on).
    void set_shadow_tuning(const float *sun_direction, bool debug_view);
    // Shadows only while a track is loaded, none in the menus (src/config.cpp).
    void start_shadow_gate();
    // The shadow debug view on or off (--shadow-tuner's Delete key).
    void set_shadow_debug(bool debug_view);
    // Ghost Arrows (src/ghost_arrows.cpp): whether the ghosts' arrows are drawn after the scenery, each whole or not at
    // all by whether its tip can be seen (Whole), rather than amid it and covered pixel by pixel (Original).
    void set_ghost_arrows_whole(bool on);
    bool ghost_arrows_whole();
    // Battle Mode Alt (src/battle_tuning.cpp): every Battle gameplay mod on (Alt) or Battle as it shipped (Original).
    void set_battle_mode_alt(bool on);
    // Cheat Menu (src/cheat_menu.cpp): CHEATS and every cheat in it available from the start (Always Shown), or
    // behind their button combos as the game shipped (Original). cheat_menu_retrace runs on every retrace.
    void set_cheat_menu_always_shown(bool on);
    void cheat_menu_retrace(uint8_t* rdram);
    constexpr float spin_min_rate = 5.0f;      // rounds a second from rest
    constexpr float spin_max_rate = 30.0f;     // at full spin
    constexpr float spin_up_seconds = 3.0f;    // held, from rest to full
    constexpr float spin_hold_seconds = 20.0f / 60.0f;  // released, before it spins down
    constexpr float spin_down_seconds = 1.5f;  // from full to rest
    // The spin level after a shot, from the level at the last one, the seconds since it (negative: the first) and the
    // reload given then; and the reload for a level.
    float spin_after_shot(float level, float gap, float last_interval);
    float spin_interval(float level);
    // Battle Mode Alt's Rammer: its refire, and the push a ram gives: the Rammer push tuning (the force an impact gives is
    // 330000) times the rammer's speed over the reference, clamped, flat along the ground away from the rammer.
    constexpr float rammer_refire_seconds = 1.0f;
    constexpr float rammer_push_reference_speed = 80.0f;
    constexpr float rammer_push_min_scale = 0.25f;
    constexpr float rammer_push_max_scale = 1.25f;
    // A ram's damage, before double health halves it: 200 + 5 x speed (as shipped 160 + 7 x speed).
    constexpr float rammer_damage_base = 200.0f;
    constexpr float rammer_damage_per_speed = 5.0f;
    float rammer_damage(float speed);
    // The push on the target's car, in its frame: target and hitter are record positions (+0x8), matrix the target
    // record's (+0x2C), speed the rammer car's (+0x3F0).
    void rammer_push(const float target[3], const float hitter[3], const float matrix[9], float speed, float force,
                     float out[3]);
    // Battle Mode Alt's pea shooter (weapon 8, held with no pickup): a direct hit's damage times this (as shipped 80).
    constexpr int pea_damage_num = 4;
    constexpr int pea_damage_den = 3;
    int pea_damage(int damage);
    // Battle Mode Alt's damage by projectile type (+6), before double health halves it: a direct hit's and a splash's
    // base, as shipped 1 the machine gun 114, 2 the bouncy ball 800 (splash 800), 4 the heavy rocket 960 (splash 960),
    // 8 the pea shooter 80 (pea_damage). The game's damage for the type (after its armour scaling) goes in; it comes
    // out scaled to these, or unchanged for a type not listed.
    constexpr int machine_gun_damage = 80;
    constexpr int ball_damage = 600;
    constexpr int ball_splash = 500;
    constexpr int heavy_damage = 1320;
    constexpr int heavy_splash = 1000;
    int direct_damage(int type, int damage);
    int splash_damage(int type, int damage);
    // A mine's 800 (not the 280 to a Rammer meeting it head-on), and the plasma blaster's refire (as shipped 0.1667 s).
    constexpr int mine_damage = 1600;
    constexpr float plasma_refire_seconds = 60.0f / 420.0f;
    // Battle Mode Alt's Sonic Boom: its impact's push away from the blast (330000), up (66000), and the share of the
    // push that goes into the car's spin, which is what flips it, each times these.
    constexpr float sonic_out_scale = 1.5f;
    constexpr float sonic_up_scale = 0.4f;
    constexpr float sonic_spin_scale = 0.3f;
    // Battle Mode Alt's tuning: values the Battle Tuning mod's options set (its sliders in the Mods tab,
    // battle_tuning_mod/, read by src/battle_tuning_mod.cpp), at these defaults without it. The bouncy ball (projectile
    // type 2) leaves at its speed along the aim, tilted up by its angle in degrees, plus the car's own velocity, and
    // lives its lifetime in seconds; in flight its fall speed grows by the world's gravity (0x80142764) times its
    // gravity; on a bounce, in the surface's frame, the speed into the surface turns round and keeps its bounce share,
    // and the speed along it its grip share (75, 3.17 s, 2.5, 0.9 and 1 as shipped, angle 0). A plasma hit pushes the
    // car hit along the shot, flat, by its force (none as shipped); a ram pushes its target by its force at
    // rammer_push_reference_speed.
    enum Tuning { ball_launch_speed, ball_lifetime, ball_gravity_scale, ball_bounce_up, ball_bounce_along, ball_angle,
                  plasma_push, rammer_push_force, tuning_count };
    // The mod's option ids, in Tuning order, and the defaults.
    constexpr const char* tuning_ids[tuning_count] = { "ball_launch_speed", "ball_lifetime", "ball_gravity",
        "ball_bounce_up", "ball_bounce_along", "ball_angle", "plasma_push", "rammer_push" };
    constexpr float tuning_defaults[tuning_count] = { 75.0f, 3.1666667f, 2.5f, 0.9f, 1.0f, 0.0f, 400000.0f,
                                                      800000.0f };
    void set_tuning(Tuning which, float value);
    float tuning(Tuning which);
    // Reads the Battle Tuning mod's options into set_tuning (src/battle_tuning_mod.cpp); once a Battle frame.
    void poll_battle_tuning();
    // A flat push along (dx, dz), the force's size, rotated into a car's frame by matrix (a record's +0x2C).
    void flat_push(float dx, float dz, const float matrix[9], float force, float out[3]);
    // The aim (unit) tilted up by degrees, in the vertical plane through it.
    void tilt_up(const float aim[3], float degrees, float out[3]);
    // Battle Mode Alt's powerup bar: at the top of each player's view while it holds a timed powerup, in the powerup's
    // colour (RGB, 0-31 each); with both, each takes half the bar. Size and place in pixels.
    enum Powerup { powerup_invisibility, powerup_shield, powerup_count };
    constexpr float powerup_seconds = 30.0f;
    constexpr int powerup_colours[powerup_count][3] = { { 31, 31, 31 }, { 6, 20, 31 } };  // white, blue
    constexpr int cooldown_bar_width = 64;
    constexpr int cooldown_bar_height = 3;
    constexpr int cooldown_bar_top_gap = 6;  // from the view's top edge
    // Each powerup's share left (1 full, 0 out), or -1 when not held, from the record's flags (+0x38C) and the
    // invisibility (+0x390) and shield (+0x394) time left.
    void powerup_fills(uint32_t flags, float invisible_left, float shield_left, float out[powerup_count]);
}

#endif
