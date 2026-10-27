#pragma once

#include <cstdint>

// The knob's ring encoder, decoded in hardware by the PCNT peripheral (x4 quadrature).
class KnobEncoder {
public:
    bool begin(int pinA, int pinB);
    // Whole detents turned since the last call (+ clockwise).
    int takeDetents();

private:
    void* unit_ = nullptr;  // pcnt_unit_handle_t
    int last_ = 0;
};
