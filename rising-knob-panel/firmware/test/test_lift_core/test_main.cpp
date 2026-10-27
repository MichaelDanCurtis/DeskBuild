// Host-side tests for the hardware-independent logic: `pio test -e native`.
#include <unity.h>

#include <cmath>

#include "align.h"
#include "homing.h"
#include "lift_settings.h"
#include "presence.h"
#include "press.h"

void setUp() {}
void tearDown() {}

// ---------- settings ----------

void test_settings_clamp() {
    LiftLimits l;
    LiftSettings s;
    s.flushMm = -1.0f;
    s.heightMm = 30.0f;
    s = clampSettings(s, l);
    TEST_ASSERT_EQUAL_FLOAT(l.minFlushMm, s.flushMm);
    TEST_ASSERT_EQUAL_FLOAT(l.maxTravelMm - l.minFlushMm, s.heightMm);
    TEST_ASSERT_TRUE(s.raisedMm() <= l.maxTravelMm);
}

// ---------- homing ----------

// A carriage on a lead screw with a normally-closed home switch at true position 0.
// The controller only sees `positionMm()`, which starts out wrong by `offset`.
struct FakeMotor : MotorPort {
    float truePos;
    float offset;  // reported = true + offset
    float target;
    float speed = 0.0f;
    float accel = 1.0f;
    bool switchBroken = false;  // reads "released" forever
    bool switchStuck = false;   // reads "pressed" forever
    float travelled = 0.0f;

    FakeMotor(float startTrue, float startReported) : truePos(startTrue), offset(startReported - startTrue), target(startReported) {}

    float positionMm() override { return truePos + offset; }
    bool moving() override { return std::fabs(target - positionMm()) > 1e-6f; }
    void moveToMm(float mm, float s, float a) override { target = mm; speed = s; accel = a; }
    void stop() override {
        // Decelerating from `speed` overshoots by v^2 / 2a in the direction of travel.
        const float dir = target > positionMm() ? 1.0f : -1.0f;
        if (moving()) target = positionMm() + dir * speed * speed / (2.0f * accel);
    }
    void setPositionMm(float mm) override {
        offset = mm - truePos;
        target = mm;
    }
    bool switchPressed() const {
        if (switchBroken) return false;
        if (switchStuck) return true;
        return truePos <= 0.0f;
    }
    void step(float dt) {
        const float d = target - positionMm();
        const float s = std::fmin(std::fabs(d), speed * dt);
        const float move = d > 0 ? s : -s;
        truePos += move;
        travelled += std::fabs(move);
    }
};

Homing::State runHoming(Homing& h, FakeMotor& m, int maxTicks = 20000) {
    h.start(m, m.switchPressed());
    for (int i = 0; i < maxTicks && h.busy(); i++) {
        m.step(0.01f);
        h.update(m, m.switchPressed());
    }
    return h.state();
}

void test_homing_from_above_finds_trip_point() {
    FakeMotor m(7.3f, 0.0f);  // really 7.3 mm above the switch, but the firmware thinks 0
    Homing h;
    TEST_ASSERT_EQUAL(static_cast<int>(Homing::State::Done), static_cast<int>(runHoming(h, m)));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, m.truePos, m.positionMm());
    // It stops a hair past the trip point (switch still pressed); the firmware then moves to flush.
    TEST_ASSERT_TRUE(m.switchPressed());
    TEST_ASSERT_TRUE(m.positionMm() > -0.05f);
}

void test_homing_starting_on_the_switch() {
    FakeMotor m(-0.6f, 5.0f);
    Homing h;
    TEST_ASSERT_EQUAL(static_cast<int>(Homing::State::Done), static_cast<int>(runHoming(h, m)));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, m.truePos, m.positionMm());
}

void test_homing_fails_safe_when_switch_never_trips() {
    FakeMotor m(5.0f, 0.0f);
    m.switchBroken = true;
    Homing::Config c;
    Homing h(c);
    TEST_ASSERT_EQUAL(static_cast<int>(Homing::State::Failed), static_cast<int>(runHoming(h, m)));
    TEST_ASSERT_TRUE(m.travelled <= c.maxSeekMm + 0.5f);
}

void test_homing_fails_safe_when_switch_stuck() {
    FakeMotor m(5.0f, 0.0f);
    m.switchStuck = true;
    Homing::Config c;
    Homing h(c);
    TEST_ASSERT_EQUAL(static_cast<int>(Homing::State::Failed), static_cast<int>(runHoming(h, m)));
    TEST_ASSERT_TRUE(m.travelled <= c.releaseMm + 0.01f);  // only the release move
}

// ---------- align ----------

AlignSession::Config alignCfg() {
    AlignSession::Config c;
    c.previewDelayMs = 1000;
    c.previewHoldMs = 3000;
    c.idleAdvanceMs = 15000;
    return c;
}

LiftSettings startSettings() {
    LiftSettings s;
    s.flushMm = 1.0f;
    s.heightMm = 12.0f;
    return s;
}

void test_align_starts_by_showing_flush_then_lifts() {
    AlignSession a(alignCfg());
    a.begin(0, startSettings());
    TEST_ASSERT_TRUE(a.showingFlush());
    TEST_ASSERT_EQUAL_FLOAT(1.0f, a.targetMm());
    a.update(3000);
    TEST_ASSERT_FALSE(a.showingFlush());
    TEST_ASSERT_EQUAL_FLOAT(13.0f, a.targetMm());
}

void test_align_ring_adjusts_flush_with_preview() {
    AlignSession a(alignCfg());
    a.begin(0, startSettings());
    a.update(3000);
    a.adjust(4000, 3, AlignSession::Source::Ring);  // +0.15 mm
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.15f, a.settings().flushMm);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 13.15f, a.targetMm());  // up, so the ring can turn
    a.update(4900);
    TEST_ASSERT_FALSE(a.showingFlush());
    a.update(5000);
    TEST_ASSERT_TRUE(a.showingFlush());  // drops to the new flush to preview it
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.15f, a.targetMm());
    a.update(8000);
    TEST_ASSERT_FALSE(a.showingFlush());  // and comes back up
}

void test_align_console_nudge_stays_flush() {
    AlignSession a(alignCfg());
    a.begin(0, startSettings());
    a.adjust(500, -2, AlignSession::Source::Console);
    a.update(100000);
    TEST_ASSERT_EQUAL(static_cast<int>(AlignSession::Phase::Flush), static_cast<int>(a.phase()));
    TEST_ASSERT_TRUE(a.showingFlush());
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.9f, a.targetMm());
}

void test_align_height_is_live_and_saves() {
    AlignSession a(alignCfg());
    a.begin(0, startSettings());
    a.next(100);
    TEST_ASSERT_EQUAL(static_cast<int>(AlignSession::Phase::Height), static_cast<int>(a.phase()));
    a.adjust(200, -5, AlignSession::Source::Ring);  // -0.5 mm
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 12.5f, a.targetMm());
    a.next(300);
    LiftSettings out;
    TEST_ASSERT_TRUE(a.takeFinished(out));
    TEST_ASSERT_FALSE(a.takeFinished(out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, out.flushMm);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 11.5f, out.heightMm);
}

void test_align_idle_advances_and_saves() {
    AlignSession a(alignCfg());
    a.begin(0, startSettings());
    a.update(3000);
    a.update(15000);  // nothing touched in phase 1
    TEST_ASSERT_EQUAL(static_cast<int>(AlignSession::Phase::Height), static_cast<int>(a.phase()));
    a.update(30001);
    LiftSettings out;
    TEST_ASSERT_TRUE(a.takeFinished(out));
}

void test_align_cancel_discards() {
    AlignSession a(alignCfg());
    a.begin(0, startSettings());
    a.adjust(100, 10, AlignSession::Source::Ring);
    a.cancel();
    LiftSettings out;
    TEST_ASSERT_FALSE(a.active());
    TEST_ASSERT_FALSE(a.takeFinished(out));
}

void test_align_respects_limits() {
    AlignSession a(alignCfg());
    a.begin(0, startSettings());
    a.adjust(100, -100, AlignSession::Source::Console);
    TEST_ASSERT_EQUAL_FLOAT(LiftLimits().minFlushMm, a.settings().flushMm);
    a.next(200);
    a.adjust(300, 500, AlignSession::Source::Ring);
    TEST_ASSERT_TRUE(a.settings().raisedMm() <= LiftLimits().maxTravelMm + 1e-4f);
}

// ---------- button ----------

void test_press_short_and_debounce() {
    PressDetector p(30, 2000);
    TEST_ASSERT_EQUAL(static_cast<int>(PressDetector::Event::None), static_cast<int>(p.update(0, true)));
    p.update(10, false);  // 10 ms glitch is ignored
    TEST_ASSERT_EQUAL(static_cast<int>(PressDetector::Event::None), static_cast<int>(p.update(100, false)));
    p.update(200, true);
    p.update(240, true);
    p.update(500, false);
    TEST_ASSERT_EQUAL(static_cast<int>(PressDetector::Event::Short), static_cast<int>(p.update(540, false)));
}

void test_press_long_fires_once_while_held() {
    PressDetector p(30, 2000);
    int longs = 0, shorts = 0;
    for (uint32_t t = 0; t <= 4000; t += 10) {
        const auto e = p.update(t, t < 3000);
        if (e == PressDetector::Event::Long) longs++;
        if (e == PressDetector::Event::Short) shorts++;
    }
    TEST_ASSERT_EQUAL(1, longs);
    TEST_ASSERT_EQUAL(0, shorts);  // releasing after a long press is not also a short press
}

// ---------- presence ----------

PresenceDetector::Config cfg() {
    PresenceDetector::Config c;
    c.nearMm = 150;
    c.farMm = 250;
    c.confirmMs = 120;
    c.lowerDelayMs = 2500;
    c.activityHoldMs = 8000;
    c.blockedMs = 60000;
    return c;
}

// Feeds `distance` every 50 ms from t to t + duration. Returns the time reached.
uint32_t feed(PresenceDetector& p, uint32_t t, uint32_t duration, int distance) {
    for (uint32_t end = t + duration; t != end; t += 50) p.update(t, distance);
    return t;
}

void test_presence_ignores_single_blip() {
    PresenceDetector p(cfg());
    p.update(0, PresenceDetector::kNoTarget);
    p.update(50, 80);  // one stray reading
    p.update(100, PresenceDetector::kNoTarget);
    feed(p, 150, 1000, PresenceDetector::kNoTarget);
    TEST_ASSERT_FALSE(p.wantRaised());
}

void test_presence_raises_after_confirm_and_lowers_after_delay() {
    PresenceDetector p(cfg());
    uint32_t t = feed(p, 0, 100, 120);
    TEST_ASSERT_FALSE(p.wantRaised());  // not yet confirmed
    t = feed(p, t, 100, 120);
    TEST_ASSERT_TRUE(p.wantRaised());

    t = feed(p, t, 2400, PresenceDetector::kNoTarget);
    TEST_ASSERT_TRUE(p.wantRaised());  // still inside the lower delay
    feed(p, t, 200, PresenceDetector::kNoTarget);
    TEST_ASSERT_FALSE(p.wantRaised());
}

void test_presence_hysteresis_band_keeps_it_up() {
    PresenceDetector p(cfg());
    uint32_t t = feed(p, 0, 300, 120);
    TEST_ASSERT_TRUE(p.wantRaised());
    // Hand drifts back to 200 mm: beyond `near` but inside `far`.
    t = feed(p, t, 10000, 200);
    TEST_ASSERT_TRUE(p.wantRaised());
    // But 200 mm alone never raises it from flush.
    PresenceDetector q(cfg());
    feed(q, 0, 5000, 200);
    TEST_ASSERT_FALSE(q.wantRaised());
}

void test_presence_activity_holds_longer() {
    PresenceDetector p(cfg());
    uint32_t t = feed(p, 0, 300, 120);
    p.noteActivity(t);
    t = feed(p, t, 7900, PresenceDetector::kNoTarget);
    TEST_ASSERT_TRUE(p.wantRaised());
    feed(p, t, 200, PresenceDetector::kNoTarget);
    TEST_ASSERT_FALSE(p.wantRaised());
}

void test_presence_activity_raises_from_flush() {
    PresenceDetector p(cfg());
    p.noteActivity(1000);
    TEST_ASSERT_TRUE(p.wantRaised());
}

void test_presence_blocked_sensor_lowers_and_latches() {
    PresenceDetector p(cfg());
    uint32_t t = feed(p, 0, 61000, 90);  // a cup parked in front of the sensor
    TEST_ASSERT_FALSE(p.wantRaised());
    TEST_ASSERT_TRUE(p.blocked());
    t = feed(p, t, 5000, 90);
    TEST_ASSERT_FALSE(p.wantRaised());  // still ignored
    t = feed(p, t, 200, PresenceDetector::kNoTarget);
    TEST_ASSERT_FALSE(p.blocked());  // cup removed
    feed(p, t, 300, 120);
    TEST_ASSERT_TRUE(p.wantRaised());  // works normally again
}

void test_presence_survives_millis_wraparound() {
    PresenceDetector p(cfg());
    const uint32_t start = 0xFFFFFFFFu - 1000u;
    uint32_t t = feed(p, start, 300, 120);
    TEST_ASSERT_TRUE(p.wantRaised());
    t = feed(p, t, 2400, PresenceDetector::kNoTarget);  // crosses zero
    TEST_ASSERT_TRUE(p.wantRaised());
    feed(p, t, 200, PresenceDetector::kNoTarget);
    TEST_ASSERT_FALSE(p.wantRaised());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_settings_clamp);
    RUN_TEST(test_homing_from_above_finds_trip_point);
    RUN_TEST(test_homing_starting_on_the_switch);
    RUN_TEST(test_homing_fails_safe_when_switch_never_trips);
    RUN_TEST(test_homing_fails_safe_when_switch_stuck);
    RUN_TEST(test_align_starts_by_showing_flush_then_lifts);
    RUN_TEST(test_align_ring_adjusts_flush_with_preview);
    RUN_TEST(test_align_console_nudge_stays_flush);
    RUN_TEST(test_align_height_is_live_and_saves);
    RUN_TEST(test_align_idle_advances_and_saves);
    RUN_TEST(test_align_cancel_discards);
    RUN_TEST(test_align_respects_limits);
    RUN_TEST(test_press_short_and_debounce);
    RUN_TEST(test_press_long_fires_once_while_held);
    RUN_TEST(test_presence_ignores_single_blip);
    RUN_TEST(test_presence_raises_after_confirm_and_lowers_after_delay);
    RUN_TEST(test_presence_hysteresis_band_keeps_it_up);
    RUN_TEST(test_presence_activity_holds_longer);
    RUN_TEST(test_presence_activity_raises_from_flush);
    RUN_TEST(test_presence_blocked_sensor_lowers_and_latches);
    RUN_TEST(test_presence_survives_millis_wraparound);
    return UNITY_END();
}
