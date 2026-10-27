#pragma once

// Knob positions, in mm of carriage travel measured from where the home sensor trips.

struct LiftSettings {
    float flushMm = 1.0f;    // knob face level with the wood
    float heightMm = 12.0f;  // how far it rises above flush
    float raisedMm() const { return flushMm + heightMm; }
};

struct LiftLimits {
    float minFlushMm = 0.3f;   // keep the sensor clear while parked flush
    float maxFlushMm = 4.0f;
    float minHeightMm = 2.0f;
    float maxTravelMm = 16.0f;  // short of the front hard stop
};

inline LiftSettings clampSettings(LiftSettings s, const LiftLimits& l) {
    if (s.flushMm < l.minFlushMm) s.flushMm = l.minFlushMm;
    if (s.flushMm > l.maxFlushMm) s.flushMm = l.maxFlushMm;
    const float maxHeight = l.maxTravelMm - s.flushMm;
    if (s.heightMm > maxHeight) s.heightMm = maxHeight;
    if (s.heightMm < l.minHeightMm) s.heightMm = l.minHeightMm;
    return s;
}
