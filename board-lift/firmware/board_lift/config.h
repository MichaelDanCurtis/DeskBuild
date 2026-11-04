// Board lift configuration.
#pragma once

#include <stdint.h>

// ---- Pins. The external ones are on the board's 10-pin FPC / adapter board.
#define PIN_SERVO        39   // FPC pin 5 "TX2"
#define PIN_TOF_SDA      40   // FPC pin 4 "RX2"
#define PIN_TOF_SCL      43   // FPC pin 7 "TX1" (UART0 TX): needs Tools > USB CDC On Boot: Enabled

// On-board knob inputs, used as "someone is using the knob" (VIEWE examples: encoder 6/5, button 0)
#define PIN_ENCODER_A    6
#define PIN_ENCODER_B    5
#define PIN_KNOB_BUTTON  0

namespace cfg {

// Set false when your own UI code owns the encoder/button pins (e.g. ESP_Knob),
// and call noteKnobActivity() from its callbacks instead.
constexpr bool kWatchKnobPins = true;

// ---- Mechanism
constexpr float kCrankRmm = 8.0f;     // horn hole radius; full travel = 2 * kCrankRmm
constexpr float kZDownMm = 0.0f;      // lowered stop; raise a little if the board sits below flush
constexpr float kZUpMm = 16.0f;       // raised stop (<= 2 * kCrankRmm)

// ---- Servo. Defaults only: calibrate over serial ('p', 'b', 't', 'w') and they are kept in flash.
constexpr int kServoUsBdc = 600;      // pulse at bottom dead centre (crank straight down, board flush)
constexpr int kServoUsTdc = 2400;     // pulse at top dead centre (crank straight up, board raised)
constexpr uint32_t kServoRelaxMs = 400;  // stop pulses this long after arriving; dead centre holds
constexpr bool kHoldWhenRaised = false;  // keep driving the servo while raised

// ---- Motion, in board millimetres
constexpr float kSpeedUpMmS = 14.0f;
constexpr float kSpeedDownMmS = 10.0f;
constexpr float kAccelMmS2 = 20.0f;
constexpr float kSmoothTauS = 0.08f;

// ---- Proximity (VL53L0X)
constexpr uint16_t kNearMm = 150;     // hand closer than this ...
constexpr uint32_t kNearHoldMs = 150; // ... for this long raises the board
constexpr uint16_t kFarMm = 250;      // nothing closer than this ...
constexpr uint32_t kFarHoldMs = 2500; // ... for this long lowers it
constexpr uint16_t kMinValidMm = 20;  // shorter readings are treated as noise (see README)
constexpr uint32_t kTofBudgetUs = 33000;

// Knob turned or pressed within this window keeps the board raised (or raises it).
constexpr uint32_t kActivityHoldMs = 6000;

}  // namespace cfg
