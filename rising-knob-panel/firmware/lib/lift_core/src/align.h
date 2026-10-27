#pragma once

#include <cstdint>

#include "lift_settings.h"

// Interactive alignment of the flush and raised positions.
//
// Phase 1, FLUSH: sets where the knob stops when lowered. The ring can't be
// turned while the knob is flush, so turning it lifts the knob; after you stop
// turning, the knob drops to the new flush position for a few seconds so you
// can see and feel it against the wood, then comes back up. Console nudges
// (`+`/`-`) keep the knob at flush the whole time.
//
// Phase 2, HEIGHT: sets how far it rises. The knob follows every change live.
//
// `next()` (button press / console) moves on. Leaving a phase alone for
// `idleAdvanceMs` also moves on, and finishing phase 2 saves.
class AlignSession {
public:
    struct Config {
        float flushStepMm = 0.05f;
        float heightStepMm = 0.1f;
        uint32_t previewDelayMs = 1200;
        uint32_t previewHoldMs = 3000;
        uint32_t idleAdvanceMs = 15000;
        LiftLimits limits;
    };

    enum class Phase { Off, Flush, Height };
    enum class Source { Ring, Console };

    AlignSession() = default;
    explicit AlignSession(const Config& c) : cfg_(c) {}

    void begin(uint32_t nowMs, const LiftSettings& current);
    void adjust(uint32_t nowMs, int steps, Source source);
    void next(uint32_t nowMs);
    void cancel();
    void update(uint32_t nowMs);

    Phase phase() const { return phase_; }
    bool active() const { return phase_ != Phase::Off; }
    bool showingFlush() const { return phase_ == Phase::Flush && atFlush_; }
    const LiftSettings& settings() const { return work_; }
    // Where the knob should be now, in mm from the home trip point.
    float targetMm() const;
    // True exactly once after the session finishes normally; `out` gets the values to save.
    bool takeFinished(LiftSettings& out);

private:
    void enterFlushPreview(uint32_t nowMs, uint32_t holdMs);

    Config cfg_;
    Phase phase_ = Phase::Off;
    LiftSettings work_;
    bool atFlush_ = false;
    bool holdForever_ = false;
    uint32_t flushUntilMs_ = 0;
    bool previewPending_ = false;
    uint32_t previewAtMs_ = 0;
    uint32_t lastActivityMs_ = 0;
    bool finished_ = false;
};
