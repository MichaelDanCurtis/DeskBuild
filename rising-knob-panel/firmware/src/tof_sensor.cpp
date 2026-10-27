#include "tof_sensor.h"

#include <Arduino.h>
#include <VL53L1X.h>
#include <Wire.h>

#include "config.h"

namespace {
VL53L1X tof;
}

bool TofSensor::begin() {
    if (!busStarted_) {
        Wire.begin(config::kTofSdaPin, config::kTofSclPin, config::kTofI2cHz);
        tof.setBus(&Wire);
        tof.setTimeout(100);
        busStarted_ = true;
    }
    ok_ = tof.init();
    if (!ok_) return false;
    // Short mode ranges to ~1.3 m and copes best with daylight, which is plenty for a hand.
    tof.setDistanceMode(VL53L1X::Short);
    tof.setMeasurementTimingBudget(config::kTofTimingBudgetUs);
    tof.startContinuous(config::kTofPeriodMs);
    return true;
}

bool TofSensor::poll(int& distanceMm) {
    if (!ok_ || !tof.dataReady()) return false;
    tof.read(false);
    if (tof.timeoutOccurred()) {
        ok_ = false;
        return false;
    }
    const auto& r = tof.ranging_data;
    const bool valid = r.range_status == VL53L1X::RangeValid && r.range_mm >= config::kMinValidMm;
    lastMm_ = distanceMm = valid ? static_cast<int>(r.range_mm) : kNoTarget;
    return true;
}
