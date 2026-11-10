// The frame lock (--frame-lock): the game's logic in a fixed order, one frame of the frame loop for each step of the
// race thread, each frame one 60th of a second long. Playing over a network needs it (docs/netplay.md, stage 1): a
// race has to play the same from the same input, and without the lock how many frames fall between two steps, and
// how long each frame says it took, depend on the machine.
//
// Two threads run the game. Thread 5 (func_800E7808) waits for a retrace, empties its retrace queue, reads the
// controllers (func_800E73D8) and, in a race, runs the tick (func_800E6AF8): one step a retrace. Thread 7, the frame
// loop (func_800FD464), runs a frame of everything else (weapons, effects, the random generator, the menus) and draws
// it. It waits for each frame in func_800C9AE0 (a message to 0x8002ECF8, then one from 0x8002ECC0 and the rest of that
// queue emptied); in a load it waits instead on the graphics-task counter (0x8002EB70) and goes on at 0x800FD510.
// func_800C9AE0 then works out the frame's time (func_800013F4): the retraces since the last frame (0x8002EB98), at
// most 5, times 1/60 (0x8002AFB8) is the frame's time in seconds (0x8002EB94), which the frame's logic steps by.
//
// Locked, the two take turns. Thread 5, about to read the controllers (us.toml, 0x800E78BC), hands its last step to
// thread 7 and waits until that frame is done; thread 7, at either wait (0x800C9B88 or 0x800FD510), hands back the frame
// it finished and waits for the next step. So each step is followed by exactly one frame, and each frame by one step,
// whatever the machine: a slow frame holds the next step back, and the game slows down rather than skipping. The
// frame's retrace count is set to 1 before its time is worked out (0x800014C4), so every frame is 1/60 s. That makes
// game time one step per retrace only at 60 frames a second: at 30 the game runs at half speed, so the lock wants the
// Game Frame Rate at 60.
//
// Waiting is rush2049_yield_to (src/spin_yield.cpp): the CPU to the other thread when it is ready, as each waits on
// the other (rush2049_spin_yield skips a thread that spins too, and thread 5 kept it). Only threads 5 and 7
// take part: func_800C9AE0 has other callers, and the hooks check which thread they are on.
//
// Not yet locked: the game's 60ths timer (the scheduler's retrace count, 0x8002EB64) and the clock and countdown
// worked out from it (0x8002EB90, func_80001578) still follow the retraces, and some twenty places in the game read
// the count. What still differs between two runs is what --sync-hash (src/sync_probe.cpp) measures.

#include <cstdio>

#include "rush2049_game.h"
#include "rush2049_hooks.h"
#include "ultramodern/ultramodern.hpp"

namespace {

bool locked = false;
// Whose turn it is: thread 5's to step, or thread 7's to run a frame of the last step. Only the game thread that
// runs touches these.
bool frame_turn = false;
bool step_taken = false;   // thread 5 has stepped since it last handed over
bool frame_taken = false;  // thread 7 is running a frame it was handed

uint32_t thread_id(uint8_t* rdram) {
    return (uint32_t)TO_PTR(OSThread, ultramodern::this_thread())->id;
}

}

void rush2049::set_frame_lock(bool on) {
    locked = on;
}

bool rush2049::frame_lock() {
    return locked;
}

extern "C" void rush2049_frame_lock_step(uint8_t* rdram) {
    if (!locked || thread_id(rdram) != 5) {
        return;
    }
    if (step_taken) {
        frame_turn = true;
        while (frame_turn) {
            rush2049_yield_to(rdram, 7);
        }
    }
    step_taken = true;
}

extern "C" void rush2049_frame_lock_frame(uint8_t* rdram) {
    if (!locked || thread_id(rdram) != 7) {
        return;
    }
    if (frame_taken) {
        frame_taken = false;
        frame_turn = false;
    }
    while (!frame_turn) {
        rush2049_yield_to(rdram, 5);
    }
    frame_taken = true;
}

extern "C" uint32_t rush2049_frame_lock_retraces(uint32_t retraces) {
    return locked ? 1 : retraces;
}
