// Host tests for the hardware-independent lift logic.
//   c++ -std=c++17 -Wall -Wextra -O1 -I ../board_lift test_logic.cpp -o /tmp/test_logic && /tmp/test_logic
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "LiftMotion.h"
#include "ProximityTrigger.h"

static int failures = 0;

#define CHECK(cond, ...)                                  \
  do {                                                    \
    if (!(cond)) {                                        \
      printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond); \
      printf(__VA_ARGS__);                                \
      printf("\n");                                       \
      failures++;                                         \
    }                                                     \
  } while (0)

struct Trace {
  float settleS = -1;
  float maxV = 0;       // planner speed
  float maxA = 0;       // planner acceleration, excluding the final stopping tick
  float stopDv = 0;     // planner speed dropped to zero on arrival (time-step rounding)
  float maxOutA = 0;    // acceleration of the smoothed output the servo follows
  float maxStep = 0;    // largest smoothed position change per second
  float maxCrankRate = 0;
  float minZ = 1e9f, maxZ = -1e9f;
  bool monotonic = true;
};

static LiftMotion::Config defaultConfig() {
  LiftMotion::Config c;
  c.crankR = 8;
  c.speedUp = 14;
  c.speedDown = 10;
  c.accel = 20;
  c.smoothTau = 0.08f;
  return c;
}

// Runs until settled or maxS; switchAt/switchTo retarget mid-move when switchAt >= 0.
static Trace run(LiftMotion &m, float target, float dt, float maxS, float switchAt = -1,
                 float switchTo = 0, bool jitter = false) {
  Trace t;
  m.setTarget(target);
  float prevZ = m.position(), prevV = m.velocity(), prevDeg = m.crankDeg();
  float prevOutV = 0;
  int dirSign = target >= prevZ ? 1 : -1;
  for (float time = 0; time < maxS;) {
    float step = jitter ? dt * (0.6f + 1.8f * (rand() / (float)RAND_MAX)) : dt;
    time += step;
    if (switchAt >= 0 && time >= switchAt) {
      m.setTarget(switchTo);
      switchAt = -1;
      dirSign = 0;  // direction changes, skip the monotonic check
    }
    m.update(step);
    float z = m.position();
    t.maxV = fmaxf(t.maxV, fabsf(m.velocity()));
    float dv = fabsf(m.velocity() - prevV);
    bool stopping = m.velocity() == 0 && prevV != 0;
    if (stopping)
      t.stopDv = fmaxf(t.stopDv, dv);
    else
      t.maxA = fmaxf(t.maxA, dv / step);
    float outV = (z - prevZ) / step;
    t.maxOutA = fmaxf(t.maxOutA, fabsf(outV - prevOutV) / step);
    prevOutV = outV;
    t.maxStep = fmaxf(t.maxStep, fabsf(z - prevZ) / step);
    t.maxCrankRate = fmaxf(t.maxCrankRate, fabsf(m.crankDeg() - prevDeg) / step);
    t.minZ = fminf(t.minZ, z);
    t.maxZ = fmaxf(t.maxZ, z);
    if (dirSign && (z - prevZ) * dirSign < -1e-6f) t.monotonic = false;
    prevZ = z;
    prevV = m.velocity();
    prevDeg = m.crankDeg();
    if (m.settled()) {
      t.settleS = time;
      break;
    }
  }
  return t;
}

static void testRiseAndFall() {
  LiftMotion m;
  m.begin(defaultConfig(), 0);
  CHECK(m.settled(), "starts settled");

  Trace up = run(m, 16, 0.005f, 10);
  printf("rise:  settle %.2fs  vmax %.2f mm/s  amax %.1f mm/s2  output amax %.1f mm/s2  stop dv %.2f mm/s"
         "  crank rate %.0f deg/s\n",
         up.settleS, up.maxV, up.maxA, up.maxOutA, up.stopDv, up.maxCrankRate);
  CHECK(up.settleS > 1.5f && up.settleS < 3.0f, "settle %.2f", up.settleS);
  CHECK(m.position() == 16.0f, "ends at %.4f", m.position());
  CHECK(up.monotonic, "rise must not reverse");
  CHECK(up.maxV <= 14.0f + 1e-3f, "vmax %.3f", up.maxV);
  CHECK(up.maxA <= 20.0f + 1e-2f, "amax %.3f", up.maxA);
  CHECK(up.stopDv <= 2 * 20.0f * 0.005f + 1e-3f, "stop dv %.3f", up.stopDv);
  CHECK(up.maxOutA <= 20.0f + 0.5f, "output amax %.3f", up.maxOutA);
  CHECK(up.maxStep <= 14.0f + 1e-3f, "smoothed speed %.3f", up.maxStep);
  CHECK(up.maxZ <= 16.0f && up.minZ >= 0.0f, "range %.3f..%.3f", up.minZ, up.maxZ);
  CHECK(fabsf(m.crankDeg() - 180) < 1e-3f, "crank %.3f", m.crankDeg());
  CHECK(up.maxCrankRate < 300, "crank rate %.0f deg/s", up.maxCrankRate);

  Trace down = run(m, 0, 0.005f, 10);
  printf("lower: settle %.2fs  vmax %.2f mm/s\n", down.settleS, down.maxV);
  CHECK(down.settleS > 1.5f && down.settleS < 3.5f, "settle %.2f", down.settleS);
  CHECK(m.position() == 0.0f, "ends at %.4f", m.position());
  CHECK(down.monotonic, "lowering must not reverse");
  CHECK(down.maxV <= 10.0f + 1e-3f, "vmax %.3f", down.maxV);
  CHECK(down.minZ >= 0.0f, "undershoot %.4f", down.minZ);
}

static void testReversal() {
  LiftMotion m;
  m.begin(defaultConfig(), 0);
  Trace t = run(m, 16, 0.005f, 10, 0.8f, 0);
  printf("reverse at 0.8s: settle %.2fs  amax %.1f  output amax %.1f  peak height %.2f\n", t.settleS,
         t.maxA, t.maxOutA, t.maxZ);
  CHECK(t.maxA <= 20.0f + 1e-2f, "acceleration jump on reversal %.3f", t.maxA);
  CHECK(t.maxOutA <= 20.0f + 0.5f, "output acceleration on reversal %.3f", t.maxOutA);
  CHECK(t.maxStep <= 14.0f + 1e-3f, "position jump %.3f", t.maxStep);
  CHECK(m.position() == 0.0f && t.minZ >= 0.0f, "ends at %.4f, min %.4f", m.position(), t.minZ);
  CHECK(t.maxZ < 16.0f, "should turn back before the top, peak %.2f", t.maxZ);
}

static void testJitteryLoop() {
  srand(1);
  LiftMotion m;
  m.begin(defaultConfig(), 0);
  Trace t = run(m, 16, 0.005f, 10, -1, 0, true);
  CHECK(t.settleS > 0 && m.position() == 16.0f, "jittery dt settle %.2f at %.4f", t.settleS, m.position());
  CHECK(t.monotonic && t.maxZ <= 16.0f, "jittery dt monotonic/overshoot");
}

static void testCrankMapping() {
  LiftMotion m;
  m.begin(defaultConfig(), 0);
  CHECK(fabsf(m.crankDegFor(0)) < 1e-3f, "0mm");
  CHECK(fabsf(m.crankDegFor(8) - 90) < 1e-3f, "8mm -> %.3f", m.crankDegFor(8));
  CHECK(fabsf(m.crankDegFor(16) - 180) < 1e-3f, "16mm -> %.3f", m.crankDegFor(16));
  for (float deg = 0; deg <= 180; deg += 7.5f) {
    float back = m.crankDegFor(m.heightForCrankDeg(deg));
    CHECK(fabsf(back - deg) < 0.05f, "round trip %.1f -> %.3f", deg, back);
  }
  m.setTarget(40);
  CHECK(m.target() == 16.0f, "target clamped to 2r, got %.2f", m.target());
  m.sync(-3);
  CHECK(m.position() == 0.0f && m.settled(), "sync clamps and settles");
}

static void testTrigger() {
  ProximityTrigger t;
  ProximityTrigger::Config c;  // 150/250 mm, 150/2500 ms, min 20 mm
  t.begin(c);
  uint32_t now = 1000;
  auto feed = [&](int mm, uint32_t forMs) {
    for (uint32_t e = 0; e < forMs; e += 33) {
      t.update(now, mm);
      now += 33;
    }
  };

  feed(100, 99);
  CHECK(!t.present(), "100 ms of hand is not enough");
  feed(-1, 500);
  feed(100, 200);
  CHECK(t.present(), "hand for 200 ms raises");
  feed(200, 10000);
  CHECK(t.present(), "inside the hysteresis band keeps it up");
  feed(-1, 2000);
  feed(120, 66);
  feed(-1, 2000);
  CHECK(t.present(), "a brief return restarts the far timer");
  feed(400, 2600);
  CHECK(!t.present(), "2.5 s with nothing near lowers");
  feed(10, 1000);
  CHECK(!t.present(), "readings under minValid count as nothing there");

  ProximityTrigger w;
  w.begin(c);
  now = 0xFFFFFF00u;  // millis() wrap
  for (int i = 0; i < 20; i++, now += 33) w.update(now, 90);
  CHECK(w.present(), "works across millis() wraparound");
}

int main() {
  testRiseAndFall();
  testReversal();
  testJitteryLoop();
  testCrankMapping();
  testTrigger();
  if (failures) {
    printf("%d failure(s)\n", failures);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
