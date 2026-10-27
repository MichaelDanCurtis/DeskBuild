// Host-side tests for the hardware-independent logic: `pio test -e native`.
#include <unity.h>

#include <cmath>

#include "crank.h"
#include "motion.h"
#include "presence.h"

void setUp() {}
void tearDown() {}

// ---------- crank ----------

void test_crank_matches_cad_model() {
    // Heights every 30 deg printed by `rising_knob.py check`.
    const float expected[] = {0.0f, 0.55f, 2.23f, 4.97f, 8.23f, 10.94f, 12.0f};
    for (int i = 0; i <= 6; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, expected[i], crank::heightMm(30.0f * i));
    }
}

void test_crank_is_monotonic_and_clamped() {
    float prev = -1.0f;
    for (int d = 0; d <= 180; d++) {
        const float h = crank::heightMm(static_cast<float>(d));
        TEST_ASSERT_TRUE(h > prev);
        prev = h;
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, crank::heightMm(-10.0f));
    TEST_ASSERT_EQUAL_FLOAT(crank::kTravelMm, crank::heightMm(200.0f));
}

// ---------- motion ----------

constexpr float kDt = 0.01f;
constexpr MotionPlanner::Limits kRaise{180.0f, 360.0f};

// Runs the planner, checking the speed and acceleration limits on every step.
// Returns the time taken to settle on the target.
float runChecked(MotionPlanner& m, MotionPlanner::Limits lim, float maxSeconds = 10.0f) {
    float t = 0.0f;
    float prevVel = m.velocity();
    const float lo = std::fmin(m.position(), m.target()) - 0.5f;
    const float hi = std::fmax(m.position(), m.target()) + 0.5f;
    while (!m.atTarget() && t < maxSeconds) {
        m.update(kDt);
        t += kDt;
        TEST_ASSERT_TRUE(std::fabs(m.velocity()) <= lim.maxVel + 1e-3f);
        TEST_ASSERT_TRUE(std::fabs(m.velocity() - prevVel) <= lim.maxAccel * kDt + 1e-3f);
        TEST_ASSERT_TRUE(m.position() >= lo && m.position() <= hi);  // no real overshoot
        prevVel = m.velocity();
    }
    return t;
}

void test_motion_full_stroke_time_and_limits() {
    MotionPlanner m(0.0f);
    m.setTarget(180.0f, kRaise);
    const float t = runChecked(m, kRaise);
    // Trapezoid: 0.5 s ramp up, 0.5 s cruise, 0.5 s ramp down.
    TEST_ASSERT_FLOAT_WITHIN(0.06f, 1.5f, t);
    TEST_ASSERT_EQUAL_FLOAT(180.0f, m.position());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.velocity());
}

void test_motion_short_move_never_reaches_cruise() {
    MotionPlanner m(0.0f);
    m.setTarget(20.0f, kRaise);
    const float t = runChecked(m, kRaise);
    // Triangle profile: 2 * sqrt(d / a).
    TEST_ASSERT_FLOAT_WITHIN(0.06f, 2.0f * std::sqrt(20.0f / 360.0f), t);
}

void test_motion_reversal_is_smooth() {
    MotionPlanner m(180.0f);
    const MotionPlanner::Limits lower{120.0f, 240.0f};
    m.setTarget(0.0f, lower);
    for (int i = 0; i < 80; i++) m.update(kDt);  // sinking at full speed
    TEST_ASSERT_TRUE(m.velocity() < -100.0f);

    // Hand comes back: rise again. Velocity must stay continuous through the turnaround.
    m.setTarget(180.0f, kRaise);
    float prevVel = m.velocity();
    float lowest = m.position();
    for (int i = 0; i < 1000 && !m.atTarget(); i++) {
        m.update(kDt);
        TEST_ASSERT_TRUE(std::fabs(m.velocity() - prevVel) <= kRaise.maxAccel * kDt + 1e-3f);
        prevVel = m.velocity();
        lowest = std::fmin(lowest, m.position());
    }
    TEST_ASSERT_TRUE(m.atTarget());
    TEST_ASSERT_EQUAL_FLOAT(180.0f, m.position());
    TEST_ASSERT_TRUE(lowest > 0.0f);  // braked before reaching flush
}

void test_motion_target_inside_braking_distance() {
    MotionPlanner m(0.0f);
    m.setTarget(180.0f, kRaise);
    for (int i = 0; i < 75; i++) m.update(kDt);  // ~90 deg, at cruise speed
    // Retarget 10 deg ahead: it can't stop in time, so it brakes past and comes back.
    const float target = m.position() + 10.0f;
    m.setTarget(target, kRaise);
    float prevVel = m.velocity();
    float highest = m.position();
    for (int i = 0; i < 1000 && !m.atTarget(); i++) {
        m.update(kDt);
        TEST_ASSERT_TRUE(std::fabs(m.velocity() - prevVel) <= kRaise.maxAccel * kDt + 1e-3f);
        prevVel = m.velocity();
        highest = std::fmax(highest, m.position());
    }
    TEST_ASSERT_TRUE(m.atTarget());
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, target, m.position());
    TEST_ASSERT_TRUE(highest > target);
}

void test_motion_lower_limits_mid_move() {
    MotionPlanner m(0.0f);
    m.setTarget(180.0f, kRaise);
    for (int i = 0; i < 60; i++) m.update(kDt);  // at 180 deg/s
    const MotionPlanner::Limits slow{120.0f, 240.0f};
    m.setTarget(180.0f, slow);
    // It eases down to the new top speed (never braking harder than the new limit),
    // then never exceeds it again.
    float prevVel = m.velocity();
    bool slowedDown = false;
    for (int i = 0; i < 1000 && !m.atTarget(); i++) {
        m.update(kDt);
        TEST_ASSERT_TRUE(std::fabs(m.velocity() - prevVel) <= slow.maxAccel * kDt + 1e-3f);
        if (m.velocity() <= slow.maxVel + 1e-3f) slowedDown = true;
        if (slowedDown) TEST_ASSERT_TRUE(m.velocity() <= slow.maxVel + 1e-3f);
        prevVel = m.velocity();
    }
    TEST_ASSERT_TRUE(m.atTarget());
    TEST_ASSERT_EQUAL_FLOAT(180.0f, m.position());
}

void test_motion_handles_irregular_ticks() {
    MotionPlanner m(0.0f);
    m.setTarget(180.0f, kRaise);
    const float steps[] = {0.004f, 0.02f, 0.011f, 0.035f, 0.009f};
    for (int i = 0; i < 2000 && !m.atTarget(); i++) m.update(steps[i % 5]);
    TEST_ASSERT_TRUE(m.atTarget());
    TEST_ASSERT_EQUAL_FLOAT(180.0f, m.position());
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
    RUN_TEST(test_crank_matches_cad_model);
    RUN_TEST(test_crank_is_monotonic_and_clamped);
    RUN_TEST(test_motion_full_stroke_time_and_limits);
    RUN_TEST(test_motion_short_move_never_reaches_cruise);
    RUN_TEST(test_motion_reversal_is_smooth);
    RUN_TEST(test_motion_target_inside_braking_distance);
    RUN_TEST(test_motion_lower_limits_mid_move);
    RUN_TEST(test_motion_handles_irregular_ticks);
    RUN_TEST(test_presence_ignores_single_blip);
    RUN_TEST(test_presence_raises_after_confirm_and_lowers_after_delay);
    RUN_TEST(test_presence_hysteresis_band_keeps_it_up);
    RUN_TEST(test_presence_activity_holds_longer);
    RUN_TEST(test_presence_activity_raises_from_flush);
    RUN_TEST(test_presence_blocked_sensor_lowers_and_latches);
    RUN_TEST(test_presence_survives_millis_wraparound);
    return UNITY_END();
}
