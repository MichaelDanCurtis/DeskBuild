#pragma once

// Knob positions, as the PQ12 actuator's extension in mm (0 = fully retracted).

struct LiftSettings {
    float flushMm = 3.0f;    // extension that puts the knob face level with the wood
    float heightMm = 12.0f;  // how far it rises above flush
    float raisedMm() const { return flushMm + heightMm; }
};

struct LiftLimits {
    float minFlushMm = 1.5f;    // keep the rod-end bracket off the actuator body
    float maxFlushMm = 8.0f;
    float minHeightMm = 2.0f;
    float maxTravelMm = 18.0f;  // short of the front hard stop (extension 19 mm)
};

inline LiftSettings clampSettings(LiftSettings s, const LiftLimits& l) {
    if (s.flushMm < l.minFlushMm) s.flushMm = l.minFlushMm;
    if (s.flushMm > l.maxFlushMm) s.flushMm = l.maxFlushMm;
    const float maxHeight = l.maxTravelMm - s.flushMm;
    if (s.heightMm > maxHeight) s.heightMm = maxHeight;
    if (s.heightMm < l.minHeightMm) s.heightMm = l.minHeightMm;
    return s;
}
