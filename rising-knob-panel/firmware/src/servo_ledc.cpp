#include "servo_ledc.h"

#include <Arduino.h>

namespace {
constexpr uint32_t kFreqHz = 50;
constexpr uint8_t kResolutionBits = 14;
constexpr float kPeriodUs = 1e6f / kFreqHz;
constexpr uint32_t kMaxDuty = (1u << kResolutionBits) - 1;
}  // namespace

bool ServoLedc::begin(int pin) {
    pin_ = pin;
    if (!ledcAttach(pin_, kFreqHz, kResolutionBits)) return false;
    ledcWrite(pin_, 0);  // no pulses until the first position is written
    return true;
}

void ServoLedc::writeMicroseconds(float us) {
    if (pin_ < 0) return;
    const uint32_t duty = static_cast<uint32_t>(us / kPeriodUs * (kMaxDuty + 1) + 0.5f);
    ledcWrite(pin_, duty > kMaxDuty ? kMaxDuty : duty);
    active_ = true;
}

void ServoLedc::release() {
    if (pin_ < 0) return;
    ledcWrite(pin_, 0);
    active_ = false;
}
