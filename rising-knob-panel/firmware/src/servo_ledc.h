#pragma once

#include <cstdint>

// 50 Hz hobby-servo output on an LEDC channel, with ~1.2 us resolution.
// `release()` stops the pulses so an analog servo (MG90S) stops holding and goes quiet.
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
