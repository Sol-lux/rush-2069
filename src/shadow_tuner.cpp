// --shadow-tuner: set Shadows' sun for each track by eye, with the game running. An overlay (Keypad 5 shows and hides
// it) shows the sun as two angles fixed to the track's world: its compass direction (azimuth, 0-360 degrees, from the
// world's +z axis towards +x) and its height above the horizon (elevation, 0-90). Keypad 4 and 6 turn it round (5
// degrees a press, held to repeat), 8 and 2 raise and lower it (2 degrees), and the shadows move as it changes.
// Players never see it: it exists only with the flag.
//
// Each track is known by a hash of its scenery boxes (the list at 0x80149B80 that Scenery Culling walks,
// src/draw_distance.cpp), which differ from track to track. Every change is saved at once to shadow_suns.txt in the
// working directory, one line per track ("<hash> <azimuth> <elevation>"), and a track's saved sun comes back whenever
// that track is loaded again. Those lines are what the game's own per-track suns are then made from.
//
// The keys come from an SDL event watch and are queued for the tuner's own thread, as --memprobe's are
// (src/mem_probe.cpp); the overlay neither captures input nor the mouse, so the race goes on underneath.

#include "debug_probe.h"
#include "rush2049_game.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

#include "recompui/recompui.h"
#include "elements/ui_container.h"
#include "elements/ui_label.h"

namespace {

constexpr const char* suns_path = "shadow_suns.txt";
constexpr float pi = 3.14159265f;

enum class Cmd { Toggle, Left, Right, Up, Down };

std::mutex cmd_mutex;
std::deque<Cmd> cmds;

int SDLCALL key_watch(void*, SDL_Event* event) {
    if (event->type != SDL_KEYDOWN) {
        return 0;
    }
    Cmd cmd;
    switch (event->key.keysym.scancode) {
        case SDL_SCANCODE_KP_5:
            if (event->key.repeat) {
                return 0;
            }
            cmd = Cmd::Toggle;
            break;
        case SDL_SCANCODE_KP_4: cmd = Cmd::Left; break;
        case SDL_SCANCODE_KP_6: cmd = Cmd::Right; break;
        case SDL_SCANCODE_KP_8: cmd = Cmd::Up; break;
        case SDL_SCANCODE_KP_2: cmd = Cmd::Down; break;
        default: return 0;
    }
    std::lock_guard lock{ cmd_mutex };
    cmds.push_back(cmd);
    return 0;
}

struct Sun {
    float azimuth;
    float elevation;
};

// The default sun (src/config.cpp's 0.3,-1,0.2) as angles.
Sun default_sun() {
    float x = 0.3f, y = -1.0f, z = 0.2f;
    // The sun stands opposite the way its light travels.
    return { std::fmod(std::atan2(-x, -z) * 180.0f / pi + 360.0f, 360.0f),
             std::atan2(-y, std::sqrt(x * x + z * z)) * 180.0f / pi };
}

void apply(const Sun& sun) {
    float a = sun.azimuth * pi / 180.0f, e = sun.elevation * pi / 180.0f;
    float direction[3] = { -std::cos(e) * std::sin(a), -std::sin(e), -std::cos(e) * std::cos(a) };
    rush2049::set_shadow_tuning(direction, false);
}

std::map<uint32_t, Sun> load() {
    std::map<uint32_t, Sun> suns;
    if (FILE* f = fopen(suns_path, "r")) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            uint32_t key;
            Sun sun;
            if (sscanf(line, "%x %f %f", &key, &sun.azimuth, &sun.elevation) == 3) {
                suns[key] = sun;
            }
        }
        fclose(f);
    }
    return suns;
}

void save(const std::map<uint32_t, Sun>& suns) {
    if (FILE* f = fopen(suns_path, "w")) {
        fprintf(f, "# Shadows' sun per track (--shadow-tuner): track hash, azimuth, elevation in degrees\n");
        for (const auto& [key, sun] : suns) {
            fprintf(f, "%08X %.0f %.0f\n", key, sun.azimuth, sun.elevation);
        }
        fclose(f);
    }
}

uint32_t word(const uint8_t* rdram, uint32_t addr) {
    return *reinterpret_cast<const uint32_t*>(rdram + (addr - 0x80000000));
}

// The track's hash: FNV-1a over the centres of its first 16 scenery boxes (0x68 bytes each, centre at +0x34..+0x3F),
// 0 when there is no list (a menu).
uint32_t track_key(const uint8_t* rdram) {
    uint32_t boxes = word(rdram, 0x80149B80);
    if (boxes < 0x80000000 || boxes >= 0x80800000 - 16 * 0x68) {
        return 0;
    }
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 3; j++) {
            hash = (hash ^ word(rdram, boxes + i * 0x68 + 0x34 + 4 * j)) * 16777619u;
        }
    }
    return hash;
}

std::string bar(float value, float range) {
    constexpr int width = 36;
    int at = std::lround(value / range * (width - 1));
    std::string s(width, '-');
    s[std::clamp(at, 0, width - 1)] = '|';
    return "[" + s + "]";
}

void tuner_thread() {
    uint8_t* rdram;
    while ((rdram = rush2049::probe_get_rdram()) == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    SDL_AddEventWatch(key_watch, nullptr);
    fprintf(stderr, "[shadow tuner] ready: Keypad 5 shows it\n");

    std::map<uint32_t, Sun> suns = load();
    uint32_t key = 0;
    Sun sun = default_sun();
    bool visible = false;
    recompui::ContextId context = recompui::ContextId::null();
    recompui::Label* label = nullptr;

    while (true) {
        uint32_t now = track_key(rdram);
        if (now != key) {
            key = now;
            auto found = suns.find(key);
            sun = found != suns.end() ? found->second : default_sun();
            apply(sun);
            fprintf(stderr, "[shadow tuner] track %08X: azimuth %.0f, elevation %.0f%s\n", key, sun.azimuth,
                    sun.elevation, found != suns.end() ? "" : " (default)");
        }

        std::deque<Cmd> pending;
        {
            std::lock_guard lock{ cmd_mutex };
            pending.swap(cmds);
        }
        bool changed = false;
        for (Cmd cmd : pending) {
            if (cmd == Cmd::Toggle) {
                if (context == recompui::ContextId::null()) {
                    using namespace recompui;
                    context = create_context();
                    context.open();
                    Container* panel = context.create_element<Container>(context.get_root_element(),
                                                                         FlexDirection::Column, JustifyContent::FlexStart);
                    panel->set_position(Position::Absolute);
                    panel->set_top(16.0f);
                    panel->set_left(16.0f);
                    panel->set_padding(12.0f);
                    panel->set_border_radius(8.0f);
                    panel->set_background_color(Color{ 0, 0, 0, 200 });
                    label = context.create_element<Label>(panel, "", LabelStyle::Small);
                    label->set_white_space(WhiteSpace::Pre);
                    label->set_color(Color{ 235, 240, 255, 255 });
                    label->set_font_size(15.0f);
                    label->set_line_height(19.0f);
                    context.close();
                    context.set_captures_input(false);
                    context.set_captures_mouse(false);
                }
                visible = !visible;
                if (visible) {
                    recompui::show_context(context, "");
                }
                else {
                    recompui::hide_context(context);
                }
            }
            else if (visible && key != 0) {
                switch (cmd) {
                    case Cmd::Left: sun.azimuth = std::fmod(sun.azimuth + 355.0f, 360.0f); break;
                    case Cmd::Right: sun.azimuth = std::fmod(sun.azimuth + 5.0f, 360.0f); break;
                    case Cmd::Up: sun.elevation = std::min(sun.elevation + 2.0f, 90.0f); break;
                    case Cmd::Down: sun.elevation = std::max(sun.elevation - 2.0f, 2.0f); break;
                    default: break;
                }
                changed = true;
            }
        }
        if (changed) {
            sun.azimuth = std::round(sun.azimuth);
            sun.elevation = std::round(sun.elevation);
            apply(sun);
            suns[key] = sun;
            save(suns);
        }

        if (visible) {
            char text[512];
            if (key == 0) {
                snprintf(text, sizeof(text), "Shadow sun\n\nNo track loaded.");
            }
            else {
                snprintf(text, sizeof(text),
                         "Shadow sun, track %08X%s\n\n"
                         "Azimuth   %3.0f  %s   Keypad 4 / 6\n"
                         "Elevation %3.0f  %s   Keypad 2 / 8\n\n"
                         "Saved to %s as it changes. Keypad 5 hides this.",
                         key, suns.count(key) ? "" : " (default)", sun.azimuth, bar(sun.azimuth, 360.0f).c_str(),
                         sun.elevation, bar(sun.elevation, 90.0f).c_str(), suns_path);
            }
            context.open();
            label->set_text(text);
            context.close();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace

void rush2049::start_shadow_tuner() {
    std::thread(tuner_thread).detach();
}
