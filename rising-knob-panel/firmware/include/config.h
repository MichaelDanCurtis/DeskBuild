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
//   FPC 1  5V     <- 5 V 2 A supply (also powers the board); actuator red wire from the same supply
//   FPC 3  GND    -> actuator black wire, sensor GND, supply GND
//   FPC 4  GPIO40 -> actuator white wire (RC signal); 10k pull-down to GND keeps it quiet at boot
//   FPC 5  GPIO39 -> VL53L1X SDA
//   FPC 6  GPIO44 -> VL53L1X SCL
//   FPC 7  GPIO43 -> unused (it carries the ROM boot log)
// ---------------------------------------------------------------------------
constexpr int kActuatorPin = 40;
constexpr int kI2cSdaPin = 39;
constexpr int kI2cSclPin = 44;
constexpr uint32_t kI2cHz = 100000;  // long FPC + breakout pull-ups: stay at 100 kHz

// On-board peripherals (fixed by the board).
constexpr int kEncoderPinA = 6;
constexpr int kEncoderPinB = 5;
constexpr int kButtonPin = 0;  // the board's BOOT button

// ---------------------------------------------------------------------------
// Actuator: Actuonix PQ12-R (RC linear servo, 20 mm stroke). Per the datasheet a
// 2.0 ms pulse fully retracts it and 1.0 ms fully extends it.
// ---------------------------------------------------------------------------
constexpr float kStrokeMm = 20.0f;
constexpr float kPulseRetractedUs = 2000.0f;
constexpr float kPulseExtendedUs = 1000.0f;
// Stop sending pulses this long after a move ends. The actuator holds its position
// unpowered (it resists ~25 N of back-drive on the 63:1 version) and goes quiet.
constexpr bool kReleaseWhenParked = true;
constexpr uint32_t kReleaseAfterMs = 1000;

// Motion of the commanded position. The PQ12-63 moves ~12-15 mm/s on 5 V, so
// these stay under its top speed and it follows the ramp. Rising takes ~1.8 s,
// sinking ~2.5 s. For the slower 100:1 version, use about 6 / 4 mm/s.
constexpr float kRaiseSpeed = 9.0f;   // mm/s
constexpr float kRaiseAccel = 18.0f;  // mm/s^2
constexpr float kLowerSpeed = 6.0f;
constexpr float kLowerAccel = 12.0f;
constexpr uint32_t kControlPeriodMs = 10;  // 100 Hz control loop

// Positions, as actuator extension in mm. Defaults until you run `align`; the
// saved values live in flash.
constexpr float kDefaultFlushMm = 3.0f;
constexpr float kDefaultHeightMm = 12.0f;
constexpr float kMaxTravelMm = 18.0f;  // the carriage hits the front stop at 19 mm

// ---------------------------------------------------------------------------
// Hand detection (VL53L1X looking out through a hole in the wood).
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
