// Board-space motion planner for the Scotch-yoke lift.
//
// The board height follows a trapezoidal speed profile (acceleration limited,
// so a new target mid-move reverses without a jolt), then a first-order
// smoothing stage rounds off the corners.  The height is converted to the
// crank angle the servo needs: z = r * (1 - cos(theta)).
//
// Plain C++ with no Arduino dependencies so it can be unit-tested on a PC.
#pragma once

#include <math.h>

class LiftMotion {
 public:
  struct Config {
    float crankR = 8.0f;      // mm; heights run from 0 to 2 * crankR
    float speedUp = 14.0f;    // mm/s
    float speedDown = 10.0f;  // mm/s
    float accel = 20.0f;      // mm/s^2
    float smoothTau = 0.08f;  // s
  };

  void begin(const Config &c, float startMm) {
    cfg_ = c;
    sync(startMm);
  }

  // Jump to a known height without moving (boot, after raw servo calibration).
  void sync(float mm) {
    z_ = zs_ = target_ = clampHeight(mm);
    v_ = 0;
  }

  void setTarget(float mm) { target_ = clampHeight(mm); }

  void update(float dt) {
    if (dt <= 0) return;
    float err = target_ - z_;
    if (err != 0 || v_ != 0) {
      float dir = err >= 0 ? 1.0f : -1.0f;
      float vmax = dir > 0 ? cfg_.speedUp : cfg_.speedDown;
      float dv = cfg_.accel * dt;
      // Fastest speed that can still stop on target when braking dv per tick.
      float vBrake = sqrtf(dv * dv / 4 + 2 * cfg_.accel * fabsf(err)) - dv / 2;
      float vDes = dir * fminf(vmax, vBrake);
      v_ += clampf(vDes - v_, -dv, dv);
      float step = v_ * dt;
      if (v_ * dir > 0 && fabsf(step) >= fabsf(err)) {
        z_ = target_;
        v_ = 0;
      } else {
        z_ += step;
      }
    }
    zs_ += (z_ - zs_) * (1 - expf(-dt / cfg_.smoothTau));
    if (v_ == 0 && z_ == target_ && fabsf(zs_ - z_) < 0.0002f) zs_ = z_;
  }

  bool settled() const { return v_ == 0 && z_ == target_ && zs_ == z_; }
  float target() const { return target_; }
  float position() const { return zs_; }     // smoothed height actually commanded
  float velocity() const { return v_; }      // planner velocity, before smoothing

  // Crank angle in degrees: 0 = bottom dead centre (lowest), 180 = top dead centre.
  float crankDeg() const { return crankDegFor(zs_); }

  float crankDegFor(float mm) const {
    return acosf(clampf(1 - mm / cfg_.crankR, -1, 1)) * (180.0f / (float)M_PI);
  }

  float heightForCrankDeg(float deg) const {
    return cfg_.crankR * (1 - cosf(clampf(deg, 0, 180) * ((float)M_PI / 180.0f)));
  }

 private:
  static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
  float clampHeight(float mm) const { return clampf(mm, 0, 2 * cfg_.crankR); }

  Config cfg_;
  float z_ = 0;       // planner height
  float zs_ = 0;      // smoothed height
  float v_ = 0;
  float target_ = 0;
};
