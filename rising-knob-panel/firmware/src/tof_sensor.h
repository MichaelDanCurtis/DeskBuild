#pragma once

#include <cstdint>

// VL53L1X time-of-flight sensor on its own I2C bus, ranging continuously.
class TofSensor {
public:
    static constexpr int kNoTarget = -1;

    // Safe to call again to recover a sensor that stopped answering.
    bool begin();
    bool ok() const { return ok_; }

    // Returns true when a new measurement arrived; `distanceMm` is then either a
    // valid range or kNoTarget (nothing in view, or a reading the sensor flagged).
    bool poll(int& distanceMm);

    int lastMm() const { return lastMm_; }

private:
    bool busStarted_ = false;
    bool ok_ = false;
    int lastMm_ = kNoTarget;
};
