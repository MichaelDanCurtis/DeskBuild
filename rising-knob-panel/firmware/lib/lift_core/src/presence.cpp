#include "presence.h"

void PresenceDetector::holdUntil(uint32_t deadlineMs) {
    if (!raised_ || reached(deadlineMs, holdUntilMs_)) holdUntilMs_ = deadlineMs;
    raised_ = true;
}

void PresenceDetector::noteActivity(uint32_t nowMs) {
    // Someone is using the knob: whatever is in front of the sensor is a person.
    blocked_ = false;
    presentTiming_ = false;
    holdUntil(nowMs + cfg_.activityHoldMs);
}

void PresenceDetector::update(uint32_t nowMs, int distanceMm) {
    const bool inView = distanceMm != kNoTarget;
    const bool near = inView && distanceMm <= cfg_.nearMm;
    const bool present = inView && distanceMm <= (raised_ ? cfg_.farMm : cfg_.nearMm);

    if (blocked_) {
        if (!inView || distanceMm > cfg_.farMm) blocked_ = false;  // view cleared
        nearTiming_ = false;
    } else {
        // Debounce the rise.
        if (near) {
            if (!nearTiming_) {
                nearTiming_ = true;
                nearSinceMs_ = nowMs;
            }
            if (reached(nowMs, nearSinceMs_ + cfg_.confirmMs)) holdUntil(nowMs + cfg_.lowerDelayMs);
        } else {
            nearTiming_ = false;
        }
        if (raised_ && present) holdUntil(nowMs + cfg_.lowerDelayMs);

        // Something parked in front of the sensor with nobody turning the knob.
        if (raised_ && present) {
            if (!presentTiming_) {
                presentTiming_ = true;
                presentSinceMs_ = nowMs;
            }
            if (reached(nowMs, presentSinceMs_ + cfg_.blockedMs)) {
                blocked_ = true;
                raised_ = false;
                presentTiming_ = false;
                nearTiming_ = false;
                return;
            }
        } else {
            presentTiming_ = false;
        }
    }

    if (raised_ && reached(nowMs, holdUntilMs_)) raised_ = false;
}
