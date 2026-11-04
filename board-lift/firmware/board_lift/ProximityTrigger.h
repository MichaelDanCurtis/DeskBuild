// "Hand present" decision from a stream of distance readings, with both
// distance hysteresis (near/far thresholds) and time hysteresis (hold times),
// so the board never twitches on a single noisy sample.
//
// Plain C++ with no Arduino dependencies so it can be unit-tested on a PC.
#pragma once

#include <stdint.h>

class ProximityTrigger {
 public:
  struct Config {
    uint16_t nearMm = 150;
    uint16_t farMm = 250;
    uint16_t minValidMm = 20;     // shorter readings count as "nothing there"
    uint32_t nearHoldMs = 150;
    uint32_t farHoldMs = 2500;
  };

  void begin(const Config &c) { cfg_ = c; }

  // rangeMm < 0 means no reading or nothing in range.
  bool update(uint32_t nowMs, int rangeMm) {
    bool valid = rangeMm >= (int)cfg_.minValidMm;
    bool isNear = valid && rangeMm <= (int)cfg_.nearMm;
    bool isFar = !valid || rangeMm >= (int)cfg_.farMm;

    if (!isNear) {
      nearRun_ = false;
    } else if (!nearRun_) {
      nearRun_ = true;
      nearSince_ = nowMs;
    }
    if (!isFar) {
      farRun_ = false;
    } else if (!farRun_) {
      farRun_ = true;
      farSince_ = nowMs;
    }

    if (!present_ && nearRun_ && nowMs - nearSince_ >= cfg_.nearHoldMs) present_ = true;
    if (present_ && farRun_ && nowMs - farSince_ >= cfg_.farHoldMs) present_ = false;
    return present_;
  }

  bool present() const { return present_; }

 private:
  Config cfg_;
  bool present_ = false;
  bool nearRun_ = false;
  bool farRun_ = false;
  uint32_t nearSince_ = 0;
  uint32_t farSince_ = 0;
};
