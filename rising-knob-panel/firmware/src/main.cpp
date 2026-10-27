// Rising knob panel: the knob display glides out of the wood when a hand comes
// near and sinks flush again when it leaves.
//
// Two tasks:
//   control (100 Hz, core 0): distance sensor -> presence -> motion planner -> servo,
//                             plus the knob encoder and the USB serial console.
//   loop()  (core 1):         LVGL UI and display brightness.
// They share only the atomics below.

#include <Arduino.h>
#include <Preferences.h>

#include <atomic>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "crank.h"
#include "knob_encoder.h"
#include "motion.h"
#include "presence.h"
#include "servo_ledc.h"
#include "tof_sensor.h"
#if RK_ENABLE_DISPLAY
#include "display.h"
#include "ui.h"
#endif

namespace {

constexpr float kFlushDeg = 0.0f;
constexpr float kRaisedDeg = 180.0f;
constexpr MotionPlanner::Limits kRaiseLimits{config::kRaiseMaxVel, config::kRaiseAccel};
constexpr MotionPlanner::Limits kLowerLimits{config::kLowerMaxVel, config::kLowerAccel};

enum class Mode : uint8_t { Auto, ForceUp, ForceDown, Calibrate };

PresenceDetector::Config presenceConfig() {
    PresenceDetector::Config c;
    c.nearMm = config::kNearMm;
    c.farMm = config::kFarMm;
    c.confirmMs = config::kConfirmMs;
    c.lowerDelayMs = config::kLowerDelayMs;
    c.activityHoldMs = config::kActivityHoldMs;
    c.blockedMs = config::kBlockedMs;
    return c;
}

ServoLedc servo;
TofSensor tof;
KnobEncoder encoder;
MotionPlanner motion(kFlushDeg);
PresenceDetector presence(presenceConfig());
Preferences prefs;

Mode mode = Mode::Auto;
uint16_t flushUs = config::kServoFlushUsDefault;
uint16_t raisedUs = config::kServoRaisedUsDefault;
float calUs = 0.0f;  // servo position while calibrating
uint32_t lastMotionMs = 0;
int distanceMm = TofSensor::kNoTarget;
constexpr uint32_t kTofRetryMs = 5000;
uint32_t lastTofRetryMs = 0;
bool logDistance = false;
uint32_t lastLogMs = 0;

// Shared with the UI task.
std::atomic<float> sharedLevel{0.0f};
std::atomic<int> sharedDetents{0};

float angleToUs(float deg) { return flushUs + (raisedUs - flushUs) * (deg / kRaisedDeg); }

const char* modeName() {
    switch (mode) {
        case Mode::Auto: return "auto";
        case Mode::ForceUp: return "up";
        case Mode::ForceDown: return "down";
        case Mode::Calibrate: return "calibrate";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// USB serial console
// ---------------------------------------------------------------------------

void printHelp() {
    Serial.println(
        "Commands:\n"
        "  status            show sensor, presence and servo state\n"
        "  up | down | auto  force the knob up/down, or back to hand detection\n"
        "  log               toggle printing the distance 5x per second\n"
        "  cal               calibrate the servo end points:\n"
        "    f | r           move to the saved flush / raised pulse\n"
        "    +N | -N         nudge the pulse by N microseconds (e.g. +10, -2)\n"
        "    set flush       save the current pulse as flush (crank straight down)\n"
        "    set raised      save the current pulse as raised (crank straight up)\n"
        "    done            leave calibration (knob returns flush)");
}

void printStatus() {
    Serial.printf("mode=%s sensor=%s distance=%s%d mm want=%s blocked=%s\n", modeName(), tof.ok() ? "ok" : "MISSING",
                  distanceMm == TofSensor::kNoTarget ? "none/" : "", distanceMm, presence.wantRaised() ? "up" : "down",
                  presence.blocked() ? "yes" : "no");
    Serial.printf("angle=%.1f deg height=%.2f mm servo=%s pulse=%.0f us (flush=%u raised=%u)\n", motion.position(),
                  crank::heightMm(motion.position()), servo.active() ? "driving" : "released",
                  mode == Mode::Calibrate ? calUs : angleToUs(motion.position()), flushUs, raisedUs);
}

void enterCalibration() {
    mode = Mode::Calibrate;
    calUs = angleToUs(motion.position());
    Serial.printf("Calibration: pulse %.0f us. Use f, r, +N, -N, set flush, set raised, done.\n", calUs);
}

void leaveCalibration() {
    // Assume the knob is wherever the calibration left it and glide home from there.
    const float span = static_cast<float>(raisedUs) - flushUs;
    float deg = span != 0.0f ? (calUs - flushUs) / span * kRaisedDeg : kFlushDeg;
    deg = constrain(deg, kFlushDeg, kRaisedDeg);
    motion.reset(deg);
    mode = Mode::Auto;
    Serial.println("Calibration done, back to auto.");
}

void handleCommand(char* line) {
    if (!strcmp(line, "help") || !strcmp(line, "?")) return printHelp();
    if (!strcmp(line, "status")) return printStatus();
    if (!strcmp(line, "log")) {
        logDistance = !logDistance;
        Serial.printf("distance log %s\n", logDistance ? "on" : "off");
        return;
    }
    if (mode == Mode::Calibrate) {
        if (!strcmp(line, "f")) calUs = flushUs;
        else if (!strcmp(line, "r")) calUs = raisedUs;
        else if (line[0] == '+' || line[0] == '-') calUs += (line[1] ? atoi(line) : (line[0] == '+' ? 5 : -5));
        else if (!strcmp(line, "set flush")) {
            flushUs = static_cast<uint16_t>(calUs);
            prefs.putUShort("flushUs", flushUs);
        } else if (!strcmp(line, "set raised")) {
            raisedUs = static_cast<uint16_t>(calUs);
            prefs.putUShort("raisedUs", raisedUs);
        } else if (!strcmp(line, "done")) return leaveCalibration();
        else return printHelp();
        calUs = constrain(calUs, static_cast<float>(config::kServoMinUs), static_cast<float>(config::kServoMaxUs));
        Serial.printf("pulse %.0f us (flush=%u raised=%u)\n", calUs, flushUs, raisedUs);
        return;
    }
    if (!strcmp(line, "up")) mode = Mode::ForceUp;
    else if (!strcmp(line, "down")) mode = Mode::ForceDown;
    else if (!strcmp(line, "auto")) mode = Mode::Auto;
    else if (!strcmp(line, "cal")) return enterCalibration();
    else return printHelp();
    Serial.printf("mode %s\n", modeName());
}

void pollSerial() {
    static char line[32];
    static size_t len = 0;
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (len) {
                line[len] = '\0';
                handleCommand(line);
                len = 0;
            }
        } else if (len < sizeof(line) - 1) {
            line[len++] = c;
        }
    }
}

// ---------------------------------------------------------------------------
// Control loop
// ---------------------------------------------------------------------------

void controlStep(uint32_t now, float dt) {
    pollSerial();

    const int detents = encoder.takeDetents();
    if (detents) {
        presence.noteActivity(now);
        sharedDetents.fetch_add(detents);
    }

    int mm;
    if (tof.poll(mm)) distanceMm = mm;
    else if (!tof.ok()) distanceMm = TofSensor::kNoTarget;
    presence.update(now, distanceMm);

    // A sensor that dropped out (loose FPC, brown-out) is re-initialised while parked;
    // init blocks for a few milliseconds, which would stutter a move.
    if (!tof.ok() && motion.atTarget() && now - lastTofRetryMs >= kTofRetryMs) {
        lastTofRetryMs = now;
        if (tof.begin()) Serial.println("VL53L1X back online");
    }

    if (logDistance && now - lastLogMs >= 200) {
        lastLogMs = now;
        Serial.printf("distance %d mm  want=%s\n", distanceMm, presence.wantRaised() ? "up" : "down");
    }

    if (mode == Mode::Calibrate) {
        servo.writeMicroseconds(calUs);
        lastMotionMs = now;
        sharedLevel.store(1.0f);
        return;
    }

    bool up = presence.wantRaised();
    if (mode == Mode::ForceUp) up = true;
    if (mode == Mode::ForceDown) up = false;
    const float target = up ? kRaisedDeg : kFlushDeg;
    if (target != motion.target()) motion.setTarget(target, up ? kRaiseLimits : kLowerLimits);

    if (!motion.atTarget()) {
        motion.update(dt);
        servo.writeMicroseconds(angleToUs(motion.position()));
        lastMotionMs = now;
    } else if (servo.active() && now - lastMotionMs >= config::kServoReleaseAfterMs) {
        servo.release();  // parked on a dead centre: the crank holds the knob
    }
    sharedLevel.store(crank::heightFraction(motion.position()));
}

void controlTask(void*) {
    TickType_t wake = xTaskGetTickCount();
    uint32_t last = millis();
    lastMotionMs = last;
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(config::kControlPeriodMs));
        const uint32_t now = millis();
        controlStep(now, (now - last) / 1000.0f);
        last = now;
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\nRising knob panel starting");

    prefs.begin("rknob", false);
    flushUs = prefs.getUShort("flushUs", config::kServoFlushUsDefault);
    raisedUs = prefs.getUShort("raisedUs", config::kServoRaisedUsDefault);

    if (!servo.begin(config::kServoPin)) Serial.println("Servo PWM setup FAILED");
    // Park flush on power-up so the mechanism starts from a known place. The pulses
    // keep coming until the control task has run for kServoReleaseAfterMs.
    servo.writeMicroseconds(flushUs);

    if (!encoder.begin(config::kEncoderPinA, config::kEncoderPinB)) Serial.println("Encoder setup FAILED");
    if (!tof.begin()) {
        Serial.println("VL53L1X not found: check SDA=GPIO39, SCL=GPIO44, 5V and GND. Turning the ring still raises the knob.");
    }

#if RK_ENABLE_DISPLAY
    if (display::begin()) ui::begin();
    else Serial.println("Display setup FAILED");
#endif

    xTaskCreatePinnedToCore(controlTask, "control", 6144, nullptr, 5, nullptr, 0);
    Serial.printf("Ready (flush=%u us, raised=%u us). Type 'help' for commands.\n", flushUs, raisedUs);
}

void loop() {
#if RK_ENABLE_DISPLAY
    const int detents = sharedDetents.exchange(0);
    if (detents) ui::addDetents(detents);
    display::setLevel(sharedLevel.load());
    display::service();
    delay(5);
#else
    sharedDetents.store(0);
    delay(100);
#endif
}
