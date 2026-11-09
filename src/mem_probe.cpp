// --memprobe: a memory-write probe for finding what the game keeps where (a weapon's ammo, a car's damage), shown in
// an overlay over the running game that F1 shows and hides. Players never see it: it exists only with the flag.
//
// The search (src/mem_search.cpp) narrows RDRAM down step by step, Cheat Engine style: F2 starts one with every
// address of the chosen size, and F3-F6 keep only those that changed, stayed, went up or went down since the last
// step, so firing a weapon between presses of F6 (down) homes in on its ammo. F9 picks one of the candidates listed
// and F10 watches it: a write-only hardware breakpoint (rush2049::start_write_watch, Windows only) counts every write,
// by the game function that made it, which is what names the code that fires the weapon.
//
// The keys come from an SDL event watch (called on the thread that pumps events) and are only queued there; a thread
// of the probe's own does the work and redraws the overlay ten times a second, so neither the game nor the UI waits
// on it. The game's memory is read while the game runs, as a cheat searcher does: a value can be caught mid-update,
// which is harmless for a search and why it takes a few steps. The overlay's context neither captures input nor the
// mouse, so the game plays on underneath. Every step and every watch's result also goes to stderr, for the log.
// F8 (RmlUi's debugger) and F11 (fullscreen) are the frontend's, so the probe leaves them alone.

#include "crash_handler.h"
#include "debug_probe.h"
#include "mem_search.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
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

constexpr size_t rdram_size = 0x800000;  // 8 MB: the Expansion Pak's, which the game uses
constexpr size_t shown_candidates = 12;

enum class Cmd { Toggle, New, Changed, Unchanged, Up, Down, Width, Next, Watch };

std::mutex cmd_mutex;
std::deque<Cmd> cmds;

int SDLCALL key_watch(void*, SDL_Event* event) {
    if (event->type != SDL_KEYDOWN || event->key.repeat) {
        return 0;
    }
    Cmd cmd;
    switch (event->key.keysym.scancode) {
        case SDL_SCANCODE_F1: cmd = Cmd::Toggle; break;
        case SDL_SCANCODE_F2: cmd = Cmd::New; break;
        case SDL_SCANCODE_F3: cmd = Cmd::Changed; break;
        case SDL_SCANCODE_F4: cmd = Cmd::Unchanged; break;
        case SDL_SCANCODE_F5: cmd = Cmd::Up; break;
        case SDL_SCANCODE_F6: cmd = Cmd::Down; break;
        case SDL_SCANCODE_F7: cmd = Cmd::Width; break;
        case SDL_SCANCODE_F9: cmd = Cmd::Next; break;
        case SDL_SCANCODE_F10: cmd = Cmd::Watch; break;
        default: return 0;
    }
    std::lock_guard lock{ cmd_mutex };
    cmds.push_back(cmd);
    return 0;
}

struct Probe {
    rush2049::MemSearch search;
    int width = 4;  // for the next F2
    size_t selected = 0;
    uint32_t watched = 0;  // vram, 0: none
    int watched_width = 0;
    bool watch_supported = true;
    std::string status = "F2 starts a search.";

    bool visible = false;
    recompui::ContextId context = recompui::ContextId::null();
    recompui::Label* label = nullptr;
};

std::string hex(uint32_t v, int digits) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%0*X", digits, v);
    return buf;
}

void log_candidates(const Probe& p, const char* step) {
    fprintf(stderr, "[memprobe] %s: %zu candidates (%d-byte)\n", step, p.search.count(), p.search.width());
    if (p.search.count() <= 32) {
        for (uint32_t offset : p.search.first(32)) {
            fprintf(stderr, "[memprobe]   %08X = %s\n", 0x80000000u + offset,
                    hex(p.search.previous(offset), p.search.width() * 2).c_str());
        }
    }
    fflush(stderr);
}

void log_watch(const Probe& p) {
    if (p.watched == 0) {
        return;
    }
    fprintf(stderr, "[memprobe] writes to %08X (%d-byte):\n", p.watched, p.watched_width);
    for (const std::string& line : rush2049::write_watch_report(64)) {
        fprintf(stderr, "[memprobe]   %s\n", line.c_str());
    }
    fflush(stderr);
}

void run(Probe& p, Cmd cmd, uint8_t* rdram) {
    using rush2049::MemSearch;
    auto step = [&](MemSearch::Cmp cmp, const char* name) {
        if (!p.search.started()) {
            p.status = "F2 starts a search first.";
            return;
        }
        p.search.filter(rdram, cmp);
        p.selected = 0;
        p.status = std::string("Kept the ") + name + " ones.";
        log_candidates(p, name);
    };
    switch (cmd) {
        case Cmd::Toggle:
            break;
        case Cmd::New:
            p.search.start(rdram, rdram_size, p.width);
            p.selected = 0;
            p.status = "New search. Change the value in the game, then F3-F6.";
            log_candidates(p, "new search");
            break;
        case Cmd::Changed: step(MemSearch::Cmp::Changed, "changed"); break;
        case Cmd::Unchanged: step(MemSearch::Cmp::Unchanged, "unchanged"); break;
        case Cmd::Up: step(MemSearch::Cmp::Increased, "increased"); break;
        case Cmd::Down: step(MemSearch::Cmp::Decreased, "decreased"); break;
        case Cmd::Width:
            p.width = p.width == 4 ? 1 : p.width * 2;
            p.status = "The next search (F2) looks at " + std::to_string(p.width) + "-byte values.";
            break;
        case Cmd::Next: {
            size_t shown = std::min(p.search.count(), shown_candidates);
            p.selected = shown == 0 ? 0 : (p.selected + 1) % shown;
            break;
        }
        case Cmd::Watch: {
            auto list = p.search.first(shown_candidates);
            if (list.empty()) {
                p.status = "No candidate to watch.";
                break;
            }
            log_watch(p);  // the last watch's result, before it is cleared
            uint32_t vram = 0x80000000u + list[std::min(p.selected, list.size() - 1)];
            if (vram == p.watched) {
                rush2049::start_write_watch(0, 0);
                p.watched = 0;
                p.status = "Stopped watching.";
                break;
            }
            p.watch_supported = rush2049::start_write_watch(vram, p.search.width());
            if (p.watch_supported) {
                p.watched = vram;
                p.watched_width = p.search.width();
                p.status = "Watching writes to " + hex(vram, 8) + ". F10 on it again stops.";
            }
            else {
                p.status = "Watching writes needs Windows (a hardware breakpoint).";
            }
            break;
        }
    }
}

std::string render(const Probe& p, const uint8_t* rdram) {
    std::string s = "MEMORY PROBE                                        F1 hides\n";
    s += "F2 new search (" + std::to_string(p.width) + "-byte, F7 changes)   F3 changed   F4 unchanged   F5 up   F6 down\n";
    s += p.status + "\n\n";
    if (p.search.started()) {
        s += std::to_string(p.search.count()) + " candidates after " + std::to_string(p.search.steps()) +
             (p.search.steps() == 1 ? " step\n" : " steps\n");
        int digits = p.search.width() * 2;
        auto list = p.search.first(shown_candidates);
        for (size_t i = 0; i < list.size(); i++) {
            uint32_t offset = list[i];
            uint32_t vram = 0x80000000u + offset;
            s += i == p.selected ? "> " : "   ";
            s += hex(vram, 8) + "   " + hex(rush2049::MemSearch::read(rdram, offset, p.search.width()), digits) +
                 "   was " + hex(p.search.previous(offset), digits);
            s += vram == p.watched ? "   (watched)\n" : "\n";
        }
        if (p.search.count() > list.size()) {
            s += "   ...\n";
        }
        s += "F9 next   F10 watch writes to the one marked\n";
    }
    if (p.watched != 0) {
        s += "\nWrites to " + hex(p.watched, 8) + ", by the game functions that made them (innermost first):\n";
        auto report = rush2049::write_watch_report(6);
        if (report.empty()) {
            s += "   none yet\n";
        }
        for (std::string line : report) {
            if (line.size() > 110) {
                line = line.substr(0, 107) + "...";
            }
            s += "   " + line + "\n";
        }
    }
    return s;
}

void build_overlay(Probe& p) {
    using namespace recompui;
    p.context = create_context();
    p.context.open();
    Element* root = p.context.get_root_element();
    Container* panel = p.context.create_element<Container>(root, FlexDirection::Column, JustifyContent::FlexStart);
    panel->set_position(Position::Absolute);
    panel->set_top(16.0f);
    panel->set_left(16.0f);
    panel->set_padding(12.0f);
    panel->set_border_radius(8.0f);
    panel->set_background_color(Color{ 0, 0, 0, 200 });
    p.label = p.context.create_element<Label>(panel, "", LabelStyle::Small);
    p.label->set_white_space(WhiteSpace::Pre);
    p.label->set_color(Color{ 235, 240, 255, 255 });
    p.label->set_font_size(15.0f);
    p.label->set_line_height(19.0f);
    p.context.close();
    p.context.set_captures_input(false);
    p.context.set_captures_mouse(false);
}

void probe_thread() {
    uint8_t* rdram;
    while ((rdram = rush2049::probe_get_rdram()) == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // Added only now, with the game running, so SDL's events are set up.
    SDL_AddEventWatch(key_watch, nullptr);
    Probe p;
    fprintf(stderr, "[memprobe] ready: F1 shows the probe\n");
    while (true) {
        std::deque<Cmd> pending;
        {
            std::lock_guard lock{ cmd_mutex };
            pending.swap(cmds);
        }
        for (Cmd cmd : pending) {
            if (cmd == Cmd::Toggle) {
                if (p.context == recompui::ContextId::null()) {
                    build_overlay(p);
                }
                p.visible = !p.visible;
                if (p.visible) {
                    recompui::show_context(p.context, "");
                }
                else {
                    recompui::hide_context(p.context);
                    log_watch(p);
                }
            }
            else if (p.visible) {
                run(p, cmd, rdram);
            }
        }
        if (p.visible) {
            p.context.open();
            p.label->set_text(render(p, rdram));
            p.context.close();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace

void rush2049::start_mem_probe() {
    std::thread(probe_thread).detach();
}
