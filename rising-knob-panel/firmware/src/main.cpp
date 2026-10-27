// Rising knob panel: the knob display glides out of the wood when a hand comes
// near and sinks flush again when it leaves. An Actuonix PQ12-R linear actuator
// moves it; it positions itself from an RC-servo pulse, so there is no homing.
//
// Two tasks:
//   control (100 Hz, core 0): distance sensor, presence, alignment, the motion
//                             ramp to the actuator, knob ring, button, USB console.
//   loop()  (core 1):         LVGL UI and display brightness.
// They share only `shared` (under a spinlock) and the detent counter.

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "align.h"
#include "config.h"
#include "knob_encoder.h"
#include "lift_settings.h"
#include "motion.h"
#include "presence.h"
#include "press.h"
#include "servo_ledc.h"
#include "tof_sensor.h"
#if RK_ENABLE_DISPLAY
#include "display.h"
#include "ui.h"
#endif

namespace {

enum class Mode : uint8_t { Auto, ForceUp, ForceDown };

constexpr MotionPlanner::Limits kRaiseLimits{config::kRaiseSpeed, config::kRaiseAccel};
constexpr MotionPlanner::Limits kLowerLimits{config::kLowerSpeed, config::kLowerAccel};

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

LiftLimits liftLimits() {
    LiftLimits l;
    l.maxTravelMm = config::kMaxTravelMm;
    return l;
}

AlignSession::Config alignConfig() {
    AlignSession::Config c;
    c.limits = liftLimits();
    return c;
}

ServoLedc actuator;
TofSensor tof;
KnobEncoder encoder;
PresenceDetector presence(presenceConfig());
AlignSession align(alignConfig());
MotionPlanner motion;
PressDetector button;
Preferences prefs;

Mode mode = Mode::Auto;
LiftSettings settings;
uint32_t lastMoveMs = 0;
// At power-up the knob may still be where it was left, a full stroke away. The
// PQ12 crosses that in under 2 s, so keep the first signal on longer than usual.
constexpr uint32_t kBootHoldMs = 2500;
uint32_t releaseAfterMs = kBootHoldMs;
int distanceMm = TofSensor::kNoTarget;
constexpr uint32_t kTofRetryMs = 5000;
uint32_t lastTofRetryMs = 0;
bool logDistance = false;
uint32_t lastLogMs = 0;

// Shared with the UI task.
struct Shared {
    float level;  // 0 = flush (screen off) .. 1 = raised (full brightness)
#if RK_ENABLE_DISPLAY
    ui::Screen screen;
#endif
    float valueMm;
};
portMUX_TYPE sharedLock = portMUX_INITIALIZER_UNLOCKED;
Shared shared{};
std::atomic<int> sharedDetents{0};

float extensionToUs(float mm) {
    return config::kPulseRetractedUs + (config::kPulseExtendedUs - config::kPulseRetractedUs) * (mm / config::kStrokeMm);
}

void saveSettings() {
    prefs.putFloat("flushMm", settings.flushMm);
    prefs.putFloat("heightMm", settings.heightMm);
}

// ---------------------------------------------------------------------------
// USB serial console
// ---------------------------------------------------------------------------

const char* modeName() {
    switch (mode) {
        case Mode::Auto: return "auto";
        case Mode::ForceUp: return "up";
        case Mode::ForceDown: return "down";
    }
    return "?";
}

void printHelp() {
    Serial.println(
        "Commands:\n"
        "  status            show sensor, presence, position and settings\n"
        "  up | down | auto  force the knob up/down, or back to hand detection\n"
        "  log               toggle printing the distance 5x per second\n"
        "  flush <mm>        set the actuator extension at flush and save\n"
        "  height <mm>       set how far it rises above flush and save\n"
        "  align             interactive alignment (same as holding the button 2 s):\n"
        "    + | - | +N | -N   nudge the current value (flush: 0.05 mm, height: 0.1 mm steps)\n"
        "    next            go to the height step / save and finish\n"
        "    cancel          leave without saving");
}

void printStatus() {
    Serial.printf("mode=%s sensor=%s distance=%d mm want=%s blocked=%s\n", modeName(), tof.ok() ? "ok" : "MISSING",
                  distanceMm, presence.wantRaised() ? "up" : "down", presence.blocked() ? "yes" : "no");
    Serial.printf("commanded=%.2f mm (%.0f us) flush=%.2f raised=%.2f signal=%s\n", motion.position(),
                  extensionToUs(motion.position()), settings.flushMm, settings.raisedMm(),
                  actuator.active() ? "on" : "released");
    if (align.active()) {
        Serial.printf("aligning %s: flush=%.2f height=%.2f\n",
                      align.phase() == AlignSession::Phase::Flush ? "flush" : "height", align.settings().flushMm,
                      align.settings().heightMm);
    }
}

void startAlign(uint32_t now) {
    align.begin(now, settings);
    Serial.println("Aligning FLUSH: turn the ring (or +/-), press the button (or 'next') for HEIGHT.");
}

void handleCommand(char* line, uint32_t now) {
    char* arg = strchr(line, ' ');
    if (arg) *arg++ = '\0';

    if (!strcmp(line, "help") || !strcmp(line, "?")) return printHelp();
    if (!strcmp(line, "status")) return printStatus();
    if (!strcmp(line, "log")) {
        logDistance = !logDistance;
        Serial.printf("distance log %s\n", logDistance ? "on" : "off");
        return;
    }
    if (!strcmp(line, "align")) return startAlign(now);
    if (!strcmp(line, "flush") || !strcmp(line, "height")) {
        if (!arg) return printHelp();
        LiftSettings s = settings;
        (line[0] == 'f' ? s.flushMm : s.heightMm) = static_cast<float>(atof(arg));
        settings = clampSettings(s, liftLimits());
        saveSettings();
        Serial.printf("saved: flush=%.2f mm height=%.2f mm\n", settings.flushMm, settings.heightMm);
        return;
    }
    if (align.active()) {
        if (line[0] == '+' || line[0] == '-') {
            const int steps = line[1] ? atoi(line) : (line[0] == '+' ? 1 : -1);
            align.adjust(now, steps, AlignSession::Source::Console);
            Serial.printf("flush=%.2f height=%.2f\n", align.settings().flushMm, align.settings().heightMm);
            return;
        }
        if (!strcmp(line, "next")) return align.next(now);
        if (!strcmp(line, "cancel")) {
            align.cancel();
            Serial.println("Alignment cancelled.");
            return;
        }
    }
    if (!strcmp(line, "up")) mode = Mode::ForceUp;
    else if (!strcmp(line, "down")) mode = Mode::ForceDown;
    else if (!strcmp(line, "auto")) mode = Mode::Auto;
    else return printHelp();
    Serial.printf("mode %s\n", modeName());
}

void pollSerial(uint32_t now) {
    static char line[48];
    static size_t len = 0;
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (len) {
                line[len] = '\0';
                handleCommand(line, now);
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

void publish() {
    Shared s{};
    s.level = (motion.position() - settings.flushMm) / settings.heightMm;
#if RK_ENABLE_DISPLAY
    s.screen = ui::Screen::Dial;
    if (align.phase() == AlignSession::Phase::Flush) s.screen = ui::Screen::AlignFlush;
    else if (align.phase() == AlignSession::Phase::Height) s.screen = ui::Screen::AlignHeight;
#endif
    if (align.active()) {
        s.level = 1.0f;
        s.valueMm = align.phase() == AlignSession::Phase::Flush ? align.settings().flushMm : align.settings().heightMm;
    }
    portENTER_CRITICAL(&sharedLock);
    shared = s;
    portEXIT_CRITICAL(&sharedLock);
}

void controlStep(uint32_t now, float dt) {
    pollSerial(now);

    const int detents = encoder.takeDetents();
    if (detents) {
        if (align.active()) align.adjust(now, detents, AlignSession::Source::Ring);
        else {
            presence.noteActivity(now);
            sharedDetents.fetch_add(detents);
        }
    }

    const auto press = button.update(now, digitalRead(config::kButtonPin) == LOW);
    if (press == PressDetector::Event::Long) {
        if (align.active()) {
            align.cancel();
            Serial.println("Alignment cancelled.");
        } else {
            startAlign(now);
        }
    } else if (press == PressDetector::Event::Short && align.active()) {
        align.next(now);
    }

    int mm;
    if (tof.poll(mm)) distanceMm = mm;
    else if (!tof.ok()) distanceMm = TofSensor::kNoTarget;
    presence.update(now, distanceMm);
    if (!tof.ok() && motion.atTarget() && now - lastTofRetryMs >= kTofRetryMs) {
        lastTofRetryMs = now;  // init blocks a few ms, so only retry while parked
        if (tof.begin()) Serial.println("VL53L1X back online");
    }
    if (logDistance && now - lastLogMs >= 200) {
        lastLogMs = now;
        Serial.printf("distance %d mm  want=%s  commanded %.2f mm\n", distanceMm,
                      presence.wantRaised() ? "up" : "down", motion.position());
    }

    align.update(now);
    LiftSettings finished;
    if (align.takeFinished(finished)) {
        settings = clampSettings(finished, liftLimits());
        saveSettings();
        Serial.printf("Alignment saved: flush=%.2f mm height=%.2f mm\n", settings.flushMm, settings.heightMm);
    }

    float target;
    if (align.active()) target = align.targetMm();
    else if (mode == Mode::ForceUp) target = settings.raisedMm();
    else if (mode == Mode::ForceDown) target = settings.flushMm;
    else target = presence.wantRaised() ? settings.raisedMm() : settings.flushMm;

    if (target != motion.target()) motion.setTarget(target, target > motion.position() ? kRaiseLimits : kLowerLimits);
    if (!motion.atTarget()) {
        motion.update(dt);
        actuator.writeMicroseconds(extensionToUs(motion.position()));
        lastMoveMs = now;
        releaseAfterMs = config::kReleaseAfterMs;
    } else if (config::kReleaseWhenParked && actuator.active() && now - lastMoveMs >= releaseAfterMs) {
        actuator.release();  // parked: the actuator holds on its own and goes quiet
    }
    publish();
}

void controlTask(void*) {
    TickType_t wake = xTaskGetTickCount();
    uint32_t last = millis();
    lastMoveMs = last;
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
    settings.flushMm = prefs.getFloat("flushMm", config::kDefaultFlushMm);
    settings.heightMm = prefs.getFloat("heightMm", config::kDefaultHeightMm);
    settings = clampSettings(settings, liftLimits());

    pinMode(config::kButtonPin, INPUT_PULLUP);
    Wire.begin(config::kI2cSdaPin, config::kI2cSclPin, config::kI2cHz);

    // Command flush straight away: the PQ12 knows its own position, so the knob
    // only moves if it was left somewhere else when the power went off.
    if (!actuator.begin(config::kActuatorPin)) Serial.println("Actuator PWM setup FAILED");
    motion.reset(settings.flushMm);
    actuator.writeMicroseconds(extensionToUs(settings.flushMm));

    if (!encoder.begin(config::kEncoderPinA, config::kEncoderPinB)) Serial.println("Encoder setup FAILED");
    if (!tof.begin()) {
        Serial.println("VL53L1X not found: check SDA=GPIO39, SCL=GPIO44, 3.3 V and GND. Turning the ring still raises the knob.");
    }

#if RK_ENABLE_DISPLAY
    if (display::begin()) ui::begin();
    else Serial.println("Display setup FAILED");
#endif

    xTaskCreatePinnedToCore(controlTask, "control", 6144, nullptr, 5, nullptr, 0);
    Serial.printf("Ready (flush=%.2f mm, raised=%.2f mm). Type 'help' for commands.\n", settings.flushMm,
                  settings.raisedMm());
}

void loop() {
#if RK_ENABLE_DISPLAY
    Shared s;
    portENTER_CRITICAL(&sharedLock);
    s = shared;
    portEXIT_CRITICAL(&sharedLock);
    const int detents = sharedDetents.exchange(0);
    if (detents) ui::addDetents(detents);
    ui::show(s.screen, s.valueMm);
    display::setLevel(s.level);
    display::service();
    delay(5);
#else
    sharedDetents.store(0);
    delay(100);
#endif
}
