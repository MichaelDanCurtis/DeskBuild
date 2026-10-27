#pragma once

#include <cstdint>

// Turns a raw button level into debounced short and long presses.
// A long press fires once while the button is still held.
class PressDetector {
public:
    enum class Event { None, Short, Long };

    explicit PressDetector(uint32_t debounceMs = 30, uint32_t longMs = 2000)
        : debounceMs_(debounceMs), longMs_(longMs) {}

    Event update(uint32_t nowMs, bool rawPressed);

private:
    uint32_t debounceMs_, longMs_;
    bool raw_ = false;
    uint32_t rawSinceMs_ = 0;
    bool pressed_ = false;
    uint32_t pressedSinceMs_ = 0;
    bool longFired_ = false;
};
