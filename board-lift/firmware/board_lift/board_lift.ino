// Board lift firmware for the VIEWE UEDX46460015-MD50ET knob display.
//
// Raises the display out of the wood panel when a hand comes near (VL53L0X
// time-of-flight sensor) and lowers it flush again once the hand has gone.
// A micro servo drives the Scotch-yoke crank; hardware notes in ../../README.md.
//
// Arduino IDE: board "ESP32S3 Dev Module", Tools > USB CDC On Boot: Enabled,
// library "VL53L0X" by Pololu.  Serial console at 115200 baud: send '?' for help.

#include <Arduino.h>
#include <Preferences.h>
#include <VL53L0X.h>
#include <Wire.h>

#include "LiftMotion.h"
#include "ProximityTrigger.h"
#include "config.h"

#if (PIN_TOF_SCL == 43 || PIN_TOF_SDA == 43) && !ARDUINO_USB_CDC_ON_BOOT
#error "GPIO43 is UART0 TX: set Tools > USB CDC On Boot > Enabled so Serial uses native USB"
#endif

enum class Mode : uint8_t { Auto, Manual, Calibrate };
enum class LiftState : uint8_t { Lowered, Raising, Raised, Lowering };

constexpr uint32_t kServoHz = 50;
constexpr uint8_t kServoBits = 14;
constexpr uint32_t kTickUs = 5000;          // 200 Hz control loop
constexpr float kCalSlewUsPerS = 400.0f;    // calibration moves glide, never jump
constexpr uint32_t kRangeStaleMs = 500;

VL53L0X tof;
Preferences prefs;
LiftMotion motion;
ProximityTrigger trigger;

Mode mode = Mode::Auto;
LiftState liftState = LiftState::Lowered;
int bdcUs = cfg::kServoUsBdc;
int tdcUs = cfg::kServoUsTdc;
bool servoDriving = false;
int lastPulseUs = 0;
bool settling = false;
uint32_t settledSinceMs = 0;
float calUs = 0;
float calTargetUs = 0;
bool knobSeen = false;
uint32_t lastKnobMs = 0;
bool liveLog = false;

// Written by the ToF task and the knob ISR, read by loop().
volatile int g_rangeMm = -1;
volatile uint32_t g_rangeAtMs = 0;
volatile bool g_tofOk = false;
volatile bool g_knobEvent = false;

const char *stateName(LiftState s) {
  switch (s) {
    case LiftState::Lowered: return "lowered";
    case LiftState::Raising: return "raising";
    case LiftState::Raised: return "raised";
    default: return "lowering";
  }
}

const char *modeName(Mode m) {
  switch (m) {
    case Mode::Auto: return "auto";
    case Mode::Manual: return "manual";
    default: return "calibrate";
  }
}

// Hook: wake, dim or switch your UI here.
void onLiftState(LiftState s) { Serial.printf("lift: %s\n", stateName(s)); }

// Call this from your own UI code (knob/touch callbacks) when kWatchKnobPins is false.
void noteKnobActivity() {
  lastKnobMs = millis();
  knobSeen = true;
}

// ---------------------------------------------------------------- servo

void servoAttach() {
  pinMode(PIN_SERVO, OUTPUT);
  digitalWrite(PIN_SERVO, LOW);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_SERVO, kServoHz, kServoBits);
#else
  ledcSetup(0, kServoHz, kServoBits);
  ledcAttachPin(PIN_SERVO, 0);
#endif
}

void servoDuty(uint32_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_SERVO, duty);
#else
  ledcWrite(0, duty);
#endif
}

void servoPulse(int us) {
  servoDuty((uint32_t)us * ((1u << kServoBits) - 1) / (1000000u / kServoHz));
  servoDriving = true;
  lastPulseUs = us;
}

// No pulses: the servo stops driving and the dead-centre crank holds the board.
void servoRelax() {
  servoDuty(0);
  servoDriving = false;
}

int pulseForDeg(float deg) { return (int)lroundf(bdcUs + (tdcUs - bdcUs) * deg / 180.0f); }

float degForPulse(float us) { return (us - bdcUs) * 180.0f / (tdcUs - bdcUs); }

// ---------------------------------------------------------------- inputs

void IRAM_ATTR onKnobPin() { g_knobEvent = true; }

void tofTask(void *) {
  Wire.begin(PIN_TOF_SDA, PIN_TOF_SCL, 400000);
  tof.setBus(&Wire);
  tof.setTimeout(250);
  for (;;) {
    if (!g_tofOk) {
      if (!tof.init()) {
        g_rangeMm = -1;
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
      tof.setMeasurementTimingBudget(cfg::kTofBudgetUs);
      tof.startContinuous();
      g_tofOk = true;
    }
    uint16_t mm = tof.readRangeContinuousMillimeters();  // blocks for about one timing budget
    if (tof.timeoutOccurred()) {
      g_tofOk = false;
      g_rangeMm = -1;
      continue;
    }
    g_rangeMm = mm >= 8000 ? -1 : mm;  // 8190/8191 mean nothing in range
    g_rangeAtMs = millis();
  }
}

// ---------------------------------------------------------------- motion

bool targetIsUp() { return motion.target() > (cfg::kZDownMm + cfg::kZUpMm) / 2; }

LiftState computeState() {
  if (motion.settled()) return targetIsUp() ? LiftState::Raised : LiftState::Lowered;
  return targetIsUp() ? LiftState::Raising : LiftState::Lowering;
}

void rememberRest(bool up) {
  if (prefs.getBool("restUp", false) != up) prefs.putBool("restUp", up);
}

void driveServo(float dt, uint32_t now) {
  motion.update(dt);
  if (!motion.settled()) {
    servoPulse(pulseForDeg(motion.crankDeg()));
    settling = false;
    return;
  }
  if (!servoDriving) return;
  if (!settling) {
    servoPulse(pulseForDeg(motion.crankDeg()));
    settling = true;
    settledSinceMs = now;
  }
  bool up = targetIsUp();
  if (cfg::kHoldWhenRaised && up) return;
  if (now - settledSinceMs >= cfg::kServoRelaxMs) {
    servoRelax();
    rememberRest(up);
  }
}

void driveCalibration(float dt) {
  float step = kCalSlewUsPerS * dt;
  float err = calTargetUs - calUs;
  calUs += err > step ? step : (err < -step ? -step : err);
  servoPulse((int)lroundf(calUs));
}

// Hand the current raw servo position back to the planner.
void leaveCalibration() {
  if (mode != Mode::Calibrate) return;
  motion.sync(motion.heightForCrankDeg(degForPulse(calUs)));
  settling = false;
}

// ---------------------------------------------------------------- serial console

void printHelp() {
  Serial.println(
      "u        raise (manual mode)\n"
      "d        lower (manual mode)\n"
      "a        automatic mode (proximity + knob)\n"
      "s        status\n"
      "l        toggle live log\n"
      "p <us>   calibrate: glide servo to a raw pulse width\n"
      "b        store current pulse as bottom dead centre (board flush)\n"
      "t        store current pulse as top dead centre (board raised)\n"
      "w        write calibration to flash");
}

void printStatus() {
  Serial.printf("mode=%s state=%s height=%.2fmm crank=%.1fdeg pulse=%dus driving=%d\n", modeName(mode),
                stateName(liftState), motion.position(), motion.crankDeg(), lastPulseUs, servoDriving);
  Serial.printf("tof=%s range=%dmm hand=%d knobBusy=%d bdc=%dus tdc=%dus\n", g_tofOk ? "ok" : "missing",
                (int)g_rangeMm, trigger.present(), knobSeen && millis() - lastKnobMs < cfg::kActivityHoldMs,
                bdcUs, tdcUs);
}

void storeCalibrationEnd(bool bottom) {
  if (mode != Mode::Calibrate) {
    Serial.println("glide to the end first with 'p <us>'");
    return;
  }
  int us = (int)lroundf(calUs);
  if (abs(us - (bottom ? tdcUs : bdcUs)) < 500) {
    Serial.println("too close to the other end; check the crank position");
    return;
  }
  (bottom ? bdcUs : tdcUs) = us;
  Serial.printf("%s = %dus (send 'w' to keep it)\n", bottom ? "bdc" : "tdc", us);
}

void handleCommand(char *line) {
  switch (line[0]) {
    case 'a':
      leaveCalibration();
      mode = Mode::Auto;
      Serial.println("auto");
      break;
    case 'u':
    case 'd':
      leaveCalibration();
      mode = Mode::Manual;
      motion.setTarget(line[0] == 'u' ? cfg::kZUpMm : cfg::kZDownMm);
      break;
    case 'p': {
      int us = atoi(line + 1);
      if (us < 400 || us > 2700) {
        Serial.println("usage: p <400..2700>");
        break;
      }
      if (mode != Mode::Calibrate) {
        calUs = pulseForDeg(motion.crankDeg());
        mode = Mode::Calibrate;
      }
      calTargetUs = us;
      break;
    }
    case 'b':
    case 't':
      storeCalibrationEnd(line[0] == 'b');
      break;
    case 'w':
      prefs.putInt("bdcUs", bdcUs);
      prefs.putInt("tdcUs", tdcUs);
      Serial.println("calibration saved");
      break;
    case 's':
      printStatus();
      break;
    case 'l':
      liveLog = !liveLog;
      break;
    default:
      printHelp();
  }
}

void pollSerial() {
  static char buf[32];
  static uint8_t n = 0;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      buf[n] = 0;
      if (n) handleCommand(buf);
      n = 0;
    } else if (n < sizeof(buf) - 1) {
      buf[n++] = ch;
    }
  }
}

// ---------------------------------------------------------------- main

void setup() {
  Serial.begin(115200);
  prefs.begin("lift", false);
  bdcUs = prefs.getInt("bdcUs", cfg::kServoUsBdc);
  tdcUs = prefs.getInt("tdcUs", cfg::kServoUsTdc);
  bool restUp = prefs.getBool("restUp", false);

  LiftMotion::Config mc;
  mc.crankR = cfg::kCrankRmm;
  mc.speedUp = cfg::kSpeedUpMmS;
  mc.speedDown = cfg::kSpeedDownMmS;
  mc.accel = cfg::kAccelMmS2;
  mc.smoothTau = cfg::kSmoothTauS;
  motion.begin(mc, restUp ? cfg::kZUpMm : cfg::kZDownMm);
  liftState = restUp ? LiftState::Raised : LiftState::Lowered;

  // Hold the servo where it was left.  If that was raised, auto mode then lowers it smoothly.
  servoAttach();
  servoPulse(pulseForDeg(motion.crankDeg()));

  ProximityTrigger::Config tc;
  tc.nearMm = cfg::kNearMm;
  tc.farMm = cfg::kFarMm;
  tc.minValidMm = cfg::kMinValidMm;
  tc.nearHoldMs = cfg::kNearHoldMs;
  tc.farHoldMs = cfg::kFarHoldMs;
  trigger.begin(tc);

  if (cfg::kWatchKnobPins) {
    for (int pin : {PIN_ENCODER_A, PIN_ENCODER_B, PIN_KNOB_BUTTON}) {
      pinMode(pin, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(pin), onKnobPin, CHANGE);
    }
  }

  xTaskCreatePinnedToCore(tofTask, "tof", 4096, nullptr, 1, nullptr, 0);
  Serial.println("board lift ready; send '?' for help");
}

void loop() {
  pollSerial();

  static uint32_t lastTickUs = micros();
  uint32_t nowUs = micros();
  if (nowUs - lastTickUs < kTickUs) return;
  float dt = fminf((nowUs - lastTickUs) * 1e-6f, 0.05f);
  lastTickUs = nowUs;
  uint32_t now = millis();

  if (g_knobEvent) {
    g_knobEvent = false;
    noteKnobActivity();
  }
  int range = (g_tofOk && now - g_rangeAtMs < kRangeStaleMs) ? g_rangeMm : -1;
  bool hand = trigger.update(now, range);
  bool knobBusy = knobSeen && now - lastKnobMs < cfg::kActivityHoldMs;

  if (mode == Mode::Calibrate) {
    driveCalibration(dt);
    return;
  }
  if (mode == Mode::Auto) motion.setTarget(hand || knobBusy ? cfg::kZUpMm : cfg::kZDownMm);
  driveServo(dt, now);

  LiftState s = computeState();
  if (s != liftState) {
    liftState = s;
    onLiftState(s);
  }

  static uint32_t lastLogMs = 0;
  if (liveLog && now - lastLogMs >= 100) {
    lastLogMs = now;
    Serial.printf("range=%d hand=%d knob=%d z=%.2f crank=%.1f us=%d\n", range, hand, knobBusy,
                  motion.position(), motion.crankDeg(), lastPulseUs);
  }
}
