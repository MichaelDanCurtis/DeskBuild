#pragma once

// All pins and tuning for the rising knob panel live here.

#include <cstdint>

#ifndef RK_ENABLE_DISPLAY
#define RK_ENABLE_DISPLAY 1
#endif

namespace config {

// ---------------------------------------------------------------------------
// Pins on the display's FPC connector (VIEWE UEDX46460015-MD50ET pinout).
// The FPC only brings out GPIO39, GPIO40 and UART0 (GPIO43/44), plus 5V and GND,
// so the enable line and the home sensor go through a PCF8574 I2C expander that
// shares the bus with the distance sensor.
//
//   FPC 1  5V     <- 5 V from the 12 V -> 5 V buck converter
//   FPC 3  GND    -> common ground (buck, TMC2209, PCF8574, VL53L1X, home sensor, 3.3 V regulator)
//   FPC 4  GPIO40 -> TMC2209 STEP (10k pull-down to GND)
//   FPC 5  GPIO39 -> I2C SDA: VL53L1X + PCF8574
//   FPC 6  GPIO44 -> I2C SCL: VL53L1X + PCF8574
//   FPC 7  GPIO43 -> TMC2209 DIR (it carries the boot log, harmless while EN is off)
//
//   PCF8574 P0 -> TMC2209 EN. The PCF8574 powers up with every pin high, which
//                 keeps the motor off until the firmware enables it.
//   PCF8574 P1 <- home sensor output: a slotted optical endstop board (high while
//                 the carriage's fin blocks the beam), or a microswitch wired
//                 normally-closed to GND (high while pressed). Either way an
//                 unplugged sensor also reads high, which looks like "tripped",
//                 so homing stops instead of driving into the end stop.
// ---------------------------------------------------------------------------
constexpr int kStepPin = 40;
constexpr int kDirPin = 43;
constexpr int kI2cSdaPin = 39;
constexpr int kI2cSclPin = 44;
constexpr uint32_t kI2cHz = 100000;  // long FPC + breakout pull-ups: stay at 100 kHz

constexpr uint8_t kExpanderAddr = 0x20;
constexpr uint8_t kExpEnableBit = 0;
constexpr uint8_t kExpHomeBit = 1;
constexpr bool kHomeTrippedHigh = true;  // set false for a sensor that pulls low when blocked

// On-board peripherals (fixed by the board).
constexpr int kEncoderPinA = 6;
constexpr int kEncoderPinB = 5;
constexpr int kButtonPin = 0;  // the board's BOOT button

// ---------------------------------------------------------------------------
// Drive: NEMA 17 with a Tr8x2 lead screw on a TMC2209 in standalone mode
// (MS1/MS2 low = 8 microsteps, StealthChop).
// ---------------------------------------------------------------------------
constexpr float kFullStepsPerRev = 200.0f;
constexpr float kMicrosteps = 8.0f;
constexpr float kLeadMm = 2.0f;
constexpr float kStepsPerMm = kFullStepsPerRev * kMicrosteps / kLeadMm;  // 800
constexpr bool kDirHighRaises = true;  // flip if the knob moves the wrong way

// Motion, in mm of knob travel. Rising takes ~1.5 s and sinking ~2 s.
constexpr float kRaiseSpeed = 12.0f;  // mm/s
constexpr float kRaiseAccel = 24.0f;  // mm/s^2
constexpr float kLowerSpeed = 8.0f;
constexpr float kLowerAccel = 16.0f;
constexpr float kJerkRampMm = 0.5f;          // acceleration builds up over this distance
constexpr uint32_t kDisableAfterMs = 1500;   // motor off once parked (the screw self-locks)
constexpr uint32_t kControlPeriodMs = 10;    // 100 Hz control loop

// Positions are measured from where the home sensor trips. Defaults until you
// run `align`; the saved values live in flash.
constexpr float kDefaultFlushMm = 1.0f;
constexpr float kDefaultHeightMm = 12.0f;
constexpr float kMaxTravelMm = 16.0f;  // short of the front hard stop even with the sensor slid 2 mm

// Homing.
constexpr float kHomeFastSpeed = 3.0f;
constexpr float kHomeSlowSpeed = 0.3f;
constexpr float kHomeAccel = 30.0f;

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
