#pragma once

#include <cstdint>

// 50 Hz RC-servo pulse output on an LEDC channel, with ~1.2 us resolution.
// `release()` stops the pulses; the PQ12-R then stops driving and holds where it is.
class ServoLedc {
public:
    bool begin(int pin);
    void writeMicroseconds(float us);
    void release();
    bool active() const { return active_; }

private:
    int pin_ = -1;
    bool active_ = false;
};
