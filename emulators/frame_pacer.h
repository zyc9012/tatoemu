#pragma once

#include "types.h"

#include <SDL3/SDL.h>
#include <algorithm>

inline double pacerNow() { return static_cast<double>(SDL_GetTicksNS()) / 1e6; }

// Frame pacing keeps emulation running at the core's native refresh rate.
//
// The two hosts we target need opposite strategies, so each is written as a
// self-contained class and the platform is selected once, at the bottom of this
// file. Everything else in the emulator talks to `FramePacer` and stays free of
// preprocessor branches.
class AudioDriftTrim {
public:
    void setAudioTarget(int bytes) { m_target = bytes; }

    double emulationSpeed() const { return m_speed; }

    void syncToAudio(int queued) {
        if (queued < 0 || m_target <= 0) return;

        // Both sides of the queue move in lumps, so the backlog ripples a couple of
        // frames however well we pace; only the drift under it is worth reacting to.
        m_filtered = m_filtered < 0.0 ? queued : m_filtered + (queued - m_filtered) * kSmoothing;

        const double error = (m_filtered - m_target) / m_target;

        // Proportional alone can only hold a correction by sitting off target, which
        // parks cores needing ~1% a whole frame low and underruns them.
        m_bias = std::clamp(m_bias - error * kIntegral, -kMaxTrim, kMaxTrim);
        m_speed = std::clamp(1.0 + m_bias - error * kGain, 1.0 - kMaxTrim, 1.0 + kMaxTrim);
    }

protected:
    void resetTrim() {
        m_speed = 1.0;
        m_bias = 0.0;
        m_filtered = -1.0;
    }

    double m_speed = 1.0;

private:
    static constexpr double kGain = 0.04;
    static constexpr double kMaxTrim = 0.02;
    static constexpr double kIntegral = 0.0003;
    static constexpr double kSmoothing = 0.05;

    int m_target = 0;
    double m_bias = 0.0;
    double m_filtered = -1.0;
};

// Strategy for hosts where we own the main loop (native builds).
//
// The loop is free-running, so pacing is just sleeping off whatever is left of the
// frame budget. VSync is disabled because the audio buffer level, not the display,
// is the timing reference.
class BlockingFramePacer : public AudioDriftTrim {
public:
    static constexpr int kVSyncInterval = 0;

    void reset(double now) {
        resetTrim();
        m_deadline = now;
    }

    // We are only called once per frame, so every iteration emulates one.
    bool frameIsDue(double /*now*/, double /*adjustedFrameTime*/) { return true; }

    void waitForNextFrame(double adjustedFrameTime) {
        m_deadline += adjustedFrameTime;

        const double now = pacerNow();
        const double remaining = m_deadline - now;

        // Too far behind to make up; start again from here rather than bank credit
        // and then sprint through the frames that follow.
        if (remaining < -adjustedFrameTime) {
            m_deadline = now;
            return;
        }
        if (remaining > 0.0) {
            SDL_DelayNS(static_cast<u64>(remaining * 1e6));
        }
    }

    void idle(double /*now*/, double targetFrameTime) {
        SDL_DelayNS(static_cast<u64>(targetFrameTime * 1e6));
        m_deadline = pacerNow();
    }

private:
    double m_deadline = 0.0;
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
class AccumulatingFramePacer : public AudioDriftTrim {
public:
    static constexpr int kVSyncInterval = 1;

    void reset(double now) {
        m_lastPoll = now;
        m_accumulator = 0.0;
        resetTrim();
    }

    bool frameIsDue(double now, double adjustedFrameTime) {
        m_accumulator += now - m_lastPoll;
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

    void waitForNextFrame(double /*adjustedFrameTime*/) {
        // The host schedules our next call; returning promptly is the whole point.
    }

    void idle(double now, double /*targetFrameTime*/) {
        // Nothing was emulated, so drop the elapsed time instead of banking it.
        reset(now);
    }

private:
    double m_lastPoll = 0.0;
    double m_accumulator = 0.0;
};

#ifdef __EMSCRIPTEN__
using FramePacer = AccumulatingFramePacer;
#else
using FramePacer = BlockingFramePacer;
#endif
