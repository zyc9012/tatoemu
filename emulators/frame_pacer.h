#pragma once

#include "types.h"

#include <SDL3/SDL.h>
#include <algorithm>

// Frame pacing keeps emulation running at the core's native refresh rate.
//
// The two hosts we target need opposite strategies, so each is written as a
// self-contained class and the platform is selected once, at the bottom of this
// file. Everything else in the emulator talks to `FramePacer` and stays free of
// preprocessor branches.
//
// The interface is:
//
//   reset(now)                               restart pacing (load, resume, stall)
//   frameIsDue(now, adjustedFrameTime)       gate before emulating a frame
//   waitForNextFrame(adjusted, elapsed)      called after the frame was presented
//   idle(now, targetFrameTime)               called instead, while paused
//   kVSyncInterval                           VSync mode the strategy requires
//
// `adjustedFrameTime` is the target frame length after the audio-buffer feedback
// loop has nudged it, so both strategies stay in sync with the audio device.

// Strategy for hosts where we own the main loop (native builds).
//
// The loop is free-running, so pacing is just sleeping off whatever is left of the
// frame budget. VSync is disabled because the audio buffer level, not the display,
// is the timing reference.
class BlockingFramePacer {
public:
    static constexpr int kVSyncInterval = 0;

    void reset(u64 /*now*/) {}

    // We are only called once per frame, so every iteration emulates one.
    bool frameIsDue(u64 /*now*/, double /*adjustedFrameTime*/) { return true; }

    void waitForNextFrame(double adjustedFrameTime, double elapsed) {
        const double remaining = adjustedFrameTime - elapsed;
        if (remaining > 1.0) {
            SDL_Delay(static_cast<u32>(remaining));
        }
    }

    void idle(u64 /*now*/, double targetFrameTime) {
        SDL_Delay(static_cast<u32>(targetFrameTime));
    }
};

// Strategy for hosts that call us from their own event loop (Emscripten, where
// emscripten_set_main_loop() drives us from requestAnimationFrame).
//
// Blocking is not an option. Without ASYNCIFY, SDL_Delay() has no way to yield to
// the browser and degrades into a busy-wait that saturates the main thread, which
// starves input dispatch and the audio callback. So instead of sleeping we bank
// elapsed real time and skip the iterations whose frame is not due yet. That also
// keeps emulation at the correct speed on high refresh-rate displays, where the
// host calls us far more than 60 times a second.
//
// kVSyncInterval must stay at 1: SDL's Emscripten backend reroutes the whole main
// loop to setTimeout(0) when VSync is disabled, and Blink suspends timer tasks for
// the duration of a touch gesture (~100ms), so a setTimeout-driven loop freezes on
// every tap. requestAnimationFrame is exempt from that deferral.
class AccumulatingFramePacer {
public:
    static constexpr int kVSyncInterval = 1;

    void reset(u64 now) {
        m_lastPoll = now;
        m_accumulator = 0.0;
    }

    bool frameIsDue(u64 now, double adjustedFrameTime) {
        m_accumulator += static_cast<double>(now - m_lastPoll);
        m_lastPoll = now;

        // The host's cadence rarely lines up with the emulated rate, so run the
        // frame once we are nearer the deadline than not.
        if (m_accumulator < adjustedFrameTime * 0.75) {
            return false;
        }

        // Carry the remainder so the long-run frame rate stays exact, but never let
        // more than one frame of debt or credit build up.
        m_accumulator = std::clamp(m_accumulator - adjustedFrameTime,
                                   -adjustedFrameTime, adjustedFrameTime);
        return true;
    }

    void waitForNextFrame(double /*adjustedFrameTime*/, double /*elapsed*/) {
        // The host schedules our next call; returning promptly is the whole point.
    }

    void idle(u64 now, double /*targetFrameTime*/) {
        // Nothing was emulated, so drop the elapsed time instead of banking it.
        reset(now);
    }

private:
    u64 m_lastPoll = 0;
    double m_accumulator = 0.0;
};

#ifdef __EMSCRIPTEN__
using FramePacer = AccumulatingFramePacer;
#else
using FramePacer = BlockingFramePacer;
#endif
