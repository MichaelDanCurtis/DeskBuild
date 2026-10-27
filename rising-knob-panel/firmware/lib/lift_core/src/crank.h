#pragma once

// Slider-crank geometry of the printed mechanism (see ../../../source/rising_knob.py).
// Servo angle 0 deg = crank at bottom dead centre = knob flush with the wood.
// Servo angle 180 deg = crank at top dead centre = knob fully raised.
namespace crank {

constexpr float kCrankRadiusMm = 6.0f;   // R_CRANK = TRAVEL / 2
constexpr float kLinkLengthMm = 18.0f;   // L_LINK
constexpr float kTravelMm = 2.0f * kCrankRadiusMm;

// How far the knob face sits above the wood for a given servo angle (0..180 deg).
float heightMm(float angleDeg);

// 0.0 at flush, 1.0 fully raised.
inline float heightFraction(float angleDeg) { return heightMm(angleDeg) / kTravelMm; }

}  // namespace crank
