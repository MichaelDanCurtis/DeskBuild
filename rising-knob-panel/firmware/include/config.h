#pragma once

// All pins and tuning for the rising knob panel live here.

#include <cstdint>

#ifndef RK_ENABLE_DISPLAY
#define RK_ENABLE_DISPLAY 1
#endif

namespace config {

// ---------------------------------------------------------------------------
// Pins on the display's FPC connector (VIEWE UEDX46460015-MD50ET pinout).
// The FPC only brings out GPIO39, GPIO40 and UART0 (GPIO43/44), plus 5V and GND.
// There is no 3.3 V pin: feed the sensor from a small 3.3 V regulator, because
// many breakouts pull SDA/SCL up to their supply and the ESP32-S3 is not 5 V tolerant.
//
//   FPC 1  5V     <- 5 V supply (also powers the board); servo V+ from the same supply
//   FPC 3  GND    -> servo GND, sensor GND, supply GND
//   FPC 4  GPIO40 -> servo signal (10k pull-down to GND keeps it quiet during boot)
//   FPC 5  GPIO39 -> VL53L1X SDA
//   FPC 6  GPIO44 -> VL53L1X SCL
//   FPC 7  GPIO43 -> unused (it carries the ROM boot log, so it is kept off the servo)
// ---------------------------------------------------------------------------
constexpr int kServoPin = 40;
constexpr int kTofSdaPin = 39;
constexpr int kTofSclPin = 44;
constexpr uint32_t kTofI2cHz = 100000;  // long FPC + breakout pull-ups: stay at 100 kHz

// On-board peripherals (fixed by the board).
constexpr int kEncoderPinA = 6;
constexpr int kEncoderPinB = 5;

// ---------------------------------------------------------------------------
// Servo (MG90S). Pulse widths are defaults; run the `cal` console command once
// the mechanism is assembled and they are saved to flash.
// ---------------------------------------------------------------------------
constexpr uint16_t kServoFlushUsDefault = 500;    // crank at bottom dead centre
constexpr uint16_t kServoRaisedUsDefault = 2400;  // crank at top dead centre
constexpr uint16_t kServoMinUs = 400;             // hard limits for calibration jogging
constexpr uint16_t kServoMaxUs = 2600;
// Stop sending pulses this long after a move ends. The crank sits on a dead
// centre, so the knob stays put and the servo goes silent.
constexpr uint32_t kServoReleaseAfterMs = 600;

// ---------------------------------------------------------------------------
// Motion (servo degrees; 180 deg = full 12 mm stroke).
// Rising takes ~1.5 s, sinking ~2 s. Lower the accelerations for softer starts.
// ---------------------------------------------------------------------------
constexpr float kRaiseMaxVel = 180.0f;    // deg/s
constexpr float kRaiseAccel = 360.0f;     // deg/s^2
constexpr float kLowerMaxVel = 120.0f;
constexpr float kLowerAccel = 240.0f;
constexpr uint32_t kControlPeriodMs = 10;  // 100 Hz control loop

// ---------------------------------------------------------------------------
// Hand detection (VL53L1X looking out through a small hole in the wood).
// ---------------------------------------------------------------------------
constexpr uint16_t kNearMm = 150;           // a hand this close raises the knob
constexpr uint16_t kFarMm = 250;            // once up, anything this close keeps it up
constexpr uint32_t kConfirmMs = 120;        // hand must stay near this long
constexpr uint32_t kLowerDelayMs = 2500;    // sink this long after the hand leaves
constexpr uint32_t kActivityHoldMs = 8000;  // stay up this long after the ring is turned
constexpr uint32_t kBlockedMs = 120000;     // object parked in view this long is ignored
constexpr uint16_t kMinValidMm = 25;        // closer readings are the wood/hole itself
constexpr uint32_t kTofTimingBudgetUs = 33000;
constexpr uint32_t kTofPeriodMs = 50;

// ---------------------------------------------------------------------------
// Knob ring encoder.
// ---------------------------------------------------------------------------
constexpr int kEncoderCountsPerDetent = 2;  // quadrature edges per click
constexpr bool kEncoderReverse = false;

// ---------------------------------------------------------------------------
// Display.
// ---------------------------------------------------------------------------
constexpr uint8_t kDisplayMaxBrightness = 255;
// The controller frame is 472 x 466 for a 466 x 466 round glass. Nudge the UI if
// the ring looks off-centre on your unit.
constexpr int kUiCenterOffsetX = 0;

}  // namespace config
