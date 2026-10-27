#pragma once

// Acceleration-limited motion planner for the servo angle.
//
// Each new target is planned from the current position *and velocity* as a
// time-optimal trapezoid: speed up at `maxAccel`, cruise at `maxVel`, brake at
// `maxAccel` and stop exactly on the target. If the knob is moving the wrong way
// (hand returns while it is sinking) or too fast to stop in time, the plan first
// brakes to a standstill and then heads for the target, so the velocity never
// jumps. That is what keeps the knob gliding instead of popping.
class MotionPlanner {
public:
    struct Limits {
        float maxVel;    // deg/s
        float maxAccel;  // deg/s^2
    };

    explicit MotionPlanner(float startDeg = 0.0f) { reset(startDeg); }

    void setTarget(float deg, Limits limits);
    // Advance by dt seconds. Returns the new position in degrees.
    float update(float dt);

    // Jump to a position with zero velocity (used at boot and in calibration).
    void reset(float deg);

    float position() const { return pos_; }
    float velocity() const { return vel_; }
    float target() const { return target_; }
    bool atTarget() const { return segCount_ == 0; }
    // Seconds left in the current move.
    float remaining() const;

private:
    struct Segment {
        float duration;
        float pos0;
        float vel0;
        float accel;
    };

    void addSegment(float duration, float accel);

    static constexpr int kMaxSegments = 4;
    Segment segs_[kMaxSegments] = {};
    int segCount_ = 0;
    int seg_ = 0;
    float segTime_ = 0.0f;

    float pos_ = 0.0f;
    float vel_ = 0.0f;
    float target_ = 0.0f;
};
