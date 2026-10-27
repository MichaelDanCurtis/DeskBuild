#pragma once

#include <cstdint>

// Decides whether the knob should be up, from distance readings and knob activity.
//
// - A hand must stay closer than `nearMm` for `confirmMs` before the knob rises,
//   so a single noisy reading never triggers it.
// - Once up, anything within `farMm` (a wider band than `nearMm`) keeps it up.
//   The gap between the two is the hysteresis that stops it bobbing.
// - It sinks `lowerDelayMs` after the hand was last seen, or `activityHoldMs`
//   after the ring was last turned, whichever is later.
// - If something sits in front of the sensor for `blockedMs` with no knob
//   activity (a cup, a book), the knob sinks and ignores that object until the
//   view clears beyond `farMm`.
class PresenceDetector {
public:
    struct Config {
        uint16_t nearMm = 150;
        uint16_t farMm = 250;
        uint32_t confirmMs = 120;
        uint32_t lowerDelayMs = 2500;
        uint32_t activityHoldMs = 8000;
        uint32_t blockedMs = 120000;
    };

    static constexpr int kNoTarget = -1;

    PresenceDetector() = default;
    explicit PresenceDetector(const Config& config) : cfg_(config) {}

    // distanceMm: latest valid range in mm, or kNoTarget when nothing is in view.
    // Call whenever a new reading arrives (or periodically with the last one).
    void update(uint32_t nowMs, int distanceMm);
    // The ring was turned or the screen touched.
    void noteActivity(uint32_t nowMs);

    bool wantRaised() const { return raised_; }
    bool blocked() const { return blocked_; }

private:
    static bool reached(uint32_t nowMs, uint32_t deadlineMs) {
        return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
    }
    void holdUntil(uint32_t deadlineMs);

    Config cfg_;
    bool raised_ = false;
    bool blocked_ = false;
    bool nearTiming_ = false;
    uint32_t nearSinceMs_ = 0;
    bool presentTiming_ = false;
    uint32_t presentSinceMs_ = 0;
    uint32_t holdUntilMs_ = 0;
};
