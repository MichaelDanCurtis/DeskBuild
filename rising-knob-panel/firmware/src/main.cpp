// Rising knob panel: the knob display glides out of the wood when a hand comes
// near and sinks flush again when it leaves.
//
// Two tasks:
//   control (100 Hz, core 0): home sensor, distance sensor, presence, alignment,
//                             stepper, knob ring, button and the USB console.
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
#include "homing.h"
#include "io_expander.h"
#include "knob_encoder.h"
#include "lift_settings.h"
#include "presence.h"
#include "press.h"
#include "stepper_drive.h"
#include "tof_sensor.h"
#if RK_ENABLE_DISPLAY
#include "display.h"
#include "ui.h"
#endif

namespace {

enum class Sys : uint8_t { Homing, Run, Fault };
enum class Mode : uint8_t { Auto, ForceUp, ForceDown };

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

Homing::Config homingConfig() {
    Homing::Config c;
    c.fastMmS = config::kHomeFastSpeed;
    c.slowMmS = config::kHomeSlowSpeed;
    c.accelMmS2 = config::kHomeAccel;
    c.maxSeekMm = config::kMaxTravelMm + 3.0f;
    return c;
}

AlignSession::Config alignConfig() {
    AlignSession::Config c;
    c.limits = liftLimits();
    return c;
}

StepperDrive drive;
Pcf8574 expander;
TofSensor tof;
KnobEncoder encoder;
PresenceDetector presence(presenceConfig());
Homing homing(homingConfig());
AlignSession align(alignConfig());
PressDetector button;
Preferences prefs;

Sys sys = Sys::Homing;
Mode mode = Mode::Auto;
LiftSettings settings;
const char* faultReason = "";
bool driverOn = false;
uint32_t driverOnMs = 0;
uint32_t lastMoveMs = 0;
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

// ---------------------------------------------------------------------------
// Hardware helpers
// ---------------------------------------------------------------------------

void setDriver(bool on, uint32_t now) {
    if (on == driverOn) return;
    expander.setBit(config::kExpEnableBit, !on);  // EN is active low
    driverOn = on;
    driverOnMs = now;
}

// One missed I2C read keeps the last value; three in a row count as tripped (fail safe).
constexpr int kMaxSensorReadFailures = 3;
bool lastHome = true;
int sensorReadFailures = 0;

bool homeTripped() {
    bool high = true;
    if (expander.readBit(config::kExpHomeBit, high)) {
        sensorReadFailures = 0;
        lastHome = high == config::kHomeTrippedHigh;
    } else if (++sensorReadFailures >= kMaxSensorReadFailures) {
        lastHome = true;
    }
    return lastHome;
}

void saveSettings() {
    prefs.putFloat("flushMm", settings.flushMm);
    prefs.putFloat("heightMm", settings.heightMm);
}

void startHoming(uint32_t now, bool homeHit) {
    align.cancel();
    if (!driverOn) {
        setDriver(true, now);
        delay(20);  // let the driver energise before the first step
    }
    homing.start(drive, homeHit);
    sys = Sys::Homing;
    Serial.println("Homing...");
}

void fault(uint32_t now, const char* why) {
    drive.stop();
    setDriver(false, now);
    sys = Sys::Fault;
    faultReason = why;
    Serial.printf("FAULT: %s. Fix it, then type 'home'.\n", why);
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

const char* sysName() {
    switch (sys) {
        case Sys::Homing: return "homing";
        case Sys::Run: return "running";
        case Sys::Fault: return "FAULT";
    }
    return "?";
}

void printHelp() {
    Serial.println(
        "Commands:\n"
        "  status            show sensor, presence, position and settings\n"
        "  up | down | auto  force the knob up/down, or back to hand detection\n"
        "  home              find the home sensor again (also clears a fault)\n"
        "  log               toggle printing the distance 5x per second\n"
        "  flush <mm>        set the flush position (mm above the home trip point) and save\n"
        "  height <mm>       set how far it rises above flush and save\n"
        "  align             interactive alignment (same as holding the button 2 s):\n"
        "    + | - | +N | -N   nudge the current value (flush: 0.05 mm, height: 0.1 mm steps)\n"
        "    next            go to the height step / save and finish\n"
        "    cancel          leave without saving");
}

void printStatus() {
    Serial.printf("state=%s mode=%s sensor=%s distance=%d mm want=%s blocked=%s\n", sysName(), modeName(),
                  tof.ok() ? "ok" : "MISSING", distanceMm, presence.wantRaised() ? "up" : "down",
                  presence.blocked() ? "yes" : "no");
    Serial.printf("position=%.2f mm (flush=%.2f raised=%.2f) motor=%s home sensor=%s expander=%s\n",
                  drive.positionMm(), settings.flushMm, settings.raisedMm(), driverOn ? "on" : "off",
                  homeTripped() ? "tripped" : "clear", expander.ok() ? "ok" : "MISSING");
    if (sys == Sys::Fault) Serial.printf("fault: %s\n", faultReason);
    if (align.active()) {
        Serial.printf("aligning %s: flush=%.2f height=%.2f\n",
                      align.phase() == AlignSession::Phase::Flush ? "flush" : "height", align.settings().flushMm,
                      align.settings().heightMm);
    }
}

void startAlign(uint32_t now) {
    if (sys != Sys::Run) {
        Serial.println("Can't align until homing has finished.");
        return;
    }
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
    if (!strcmp(line, "home")) return startHoming(now, homeTripped());
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

void runStep(uint32_t now, bool homeHit, int detents, PressDetector::Event press) {
    if (detents) {
        if (align.active()) align.adjust(now, detents, AlignSession::Source::Ring);
        else {
            presence.noteActivity(now);
            sharedDetents.fetch_add(detents);
        }
    }
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

    const float pos = drive.positionMm();
    // The sensor only trips below flush. Seeing it up here means steps were lost
    // (something blocked the knob), so find home again rather than trust the count.
    if (homeHit && pos > 0.5f) {
        drive.stop();
        Serial.println("Home sensor tripped unexpectedly: re-homing.");
        startHoming(now, homeHit);
        return;
    }

    const bool needsMove = drive.moving() || std::fabs(target - pos) > 0.5f / config::kStepsPerMm;
    if (needsMove) {
        if (!driverOn) {
            setDriver(true, now);  // let the driver energise before the first step
            return;
        }
        if (now - driverOnMs < 20) return;
        const bool rising = target > pos;
        drive.track(target, rising ? config::kRaiseSpeed : config::kLowerSpeed,
                    rising ? config::kRaiseAccel : config::kLowerAccel);
        lastMoveMs = now;
    } else if (driverOn && now - lastMoveMs >= config::kDisableAfterMs) {
        setDriver(false, now);  // parked: the lead screw holds the knob, the motor goes silent
    }
}

void publish() {
    Shared s{};
    const float pos = drive.positionMm();
    s.level = sys == Sys::Run ? (pos - settings.flushMm) / settings.heightMm : 0.0f;
#if RK_ENABLE_DISPLAY
    s.screen = ui::Screen::Dial;
    if (sys == Sys::Homing) s.screen = ui::Screen::Homing;
    else if (sys == Sys::Fault) s.screen = ui::Screen::Fault;
    else if (align.phase() == AlignSession::Phase::Flush) s.screen = ui::Screen::AlignFlush;
    else if (align.phase() == AlignSession::Phase::Height) s.screen = ui::Screen::AlignHeight;
#endif
    if (align.active()) {
        s.level = 1.0f;
        s.valueMm = align.phase() == AlignSession::Phase::Flush ? align.settings().flushMm : align.settings().heightMm;
    }
    if (sys != Sys::Run) s.level = 1.0f;  // show the homing / fault message
    portENTER_CRITICAL(&sharedLock);
    shared = s;
    portEXIT_CRITICAL(&sharedLock);
}

void controlStep(uint32_t now) {
    pollSerial(now);

    const bool homeHit = homeTripped();
    const int detents = encoder.takeDetents();
    const auto press = button.update(now, digitalRead(config::kButtonPin) == LOW);

    int mm;
    if (tof.poll(mm)) distanceMm = mm;
    else if (!tof.ok()) distanceMm = TofSensor::kNoTarget;
    presence.update(now, distanceMm);
    if (!tof.ok() && !drive.moving() && now - lastTofRetryMs >= kTofRetryMs) {
        lastTofRetryMs = now;  // init blocks a few ms, so only retry while parked
        if (tof.begin()) Serial.println("VL53L1X back online");
    }
    if (logDistance && now - lastLogMs >= 200) {
        lastLogMs = now;
        Serial.printf("distance %d mm  want=%s  position %.2f mm\n", distanceMm,
                      presence.wantRaised() ? "up" : "down", drive.positionMm());
    }

    switch (sys) {
        case Sys::Homing:
            homing.update(drive, homeHit);
            if (homing.state() == Homing::State::Done) {
                sys = Sys::Run;
                lastMoveMs = now;
                Serial.printf("Homed. Flush at %.2f mm, raised at %.2f mm.\n", settings.flushMm, settings.raisedMm());
            } else if (homing.state() == Homing::State::Failed) {
                fault(now, homing.error());
            }
            break;
        case Sys::Run:
            if (sensorReadFailures >= kMaxSensorReadFailures) fault(now, "PCF8574 expander not answering");
            else runStep(now, homeHit, detents, press);
            break;
        case Sys::Fault:
            if (press == PressDetector::Event::Long) startHoming(now, homeHit);
            break;
    }
    publish();
}

void controlTask(void*) {
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(config::kControlPeriodMs));
        controlStep(millis());
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
    const bool expanderOk = expander.begin(config::kExpanderAddr);  // also keeps EN off
    if (!expanderOk) Serial.println("PCF8574 not found at 0x20: check SDA=GPIO39, SCL=GPIO44, 3.3 V and GND.");
    if (!drive.begin(config::kStepPin, config::kDirPin, config::kStepsPerMm, config::kDirHighRaises,
                     config::kJerkRampMm)) {
        Serial.println("Stepper setup FAILED");
    }
    if (!encoder.begin(config::kEncoderPinA, config::kEncoderPinB)) Serial.println("Encoder setup FAILED");
    if (!tof.begin()) Serial.println("VL53L1X not found: turning the ring still raises the knob.");

#if RK_ENABLE_DISPLAY
    if (display::begin()) ui::begin();
    else Serial.println("Display setup FAILED");
#endif

    const uint32_t now = millis();
    if (expanderOk) startHoming(now, homeTripped());
    else fault(now, "PCF8574 expander not answering");

    xTaskCreatePinnedToCore(controlTask, "control", 6144, nullptr, 5, nullptr, 0);
    Serial.println("Type 'help' for commands.");
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
