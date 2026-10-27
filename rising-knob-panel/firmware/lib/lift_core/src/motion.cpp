#include "motion.h"

#include <cmath>

void MotionPlanner::reset(float position) {
    pos_ = target_ = position;
    vel_ = 0.0f;
    segCount_ = seg_ = 0;
    segTime_ = 0.0f;
}

void MotionPlanner::addSegment(float duration, float accel) {
    if (duration <= 0.0f) return;
    // Each segment starts where the previous one ends.
    float p = pos_, v = vel_;
    if (segCount_ > 0) {
        const Segment& s = segs_[segCount_ - 1];
        p = s.pos0 + s.vel0 * s.duration + 0.5f * s.accel * s.duration * s.duration;
        v = s.vel0 + s.accel * s.duration;
    }
    segs_[segCount_++] = {duration, p, v, accel};
}

void MotionPlanner::setTarget(float target, Limits lim) {
    target_ = target;
    segCount_ = seg_ = 0;
    segTime_ = 0.0f;
    const float a = lim.maxAccel;

    float startPos = pos_;
    float u = vel_;  // velocity at the start of the rest of the plan
    float d = target - startPos;

    // Moving away from the target, or too fast to stop before it: brake to zero first.
    const bool away = u * d < 0.0f || (d == 0.0f && u != 0.0f);
    const bool overshoot = !away && u * u / (2.0f * a) > std::fabs(d);
    if (away || overshoot) {
        const float tStop = std::fabs(u) / a;
        addSegment(tStop, u > 0.0f ? -a : a);
        startPos += u * tStop * 0.5f;
        u = 0.0f;
        d = target - startPos;
    }
    if (d == 0.0f) {
        if (segCount_ == 0) reset(target);
        return;
    }

    // Rest of the move in the direction of travel `s`, starting at speed `u` >= 0.
    const float s = d > 0.0f ? 1.0f : -1.0f;
    const float dist = std::fabs(d);
    u = std::fabs(u);
    float peak = std::sqrt(a * dist + 0.5f * u * u);  // triangle profile peak
    float cruise = 0.0f;
    if (peak > lim.maxVel) {
        peak = lim.maxVel;
        const float rampUp = std::fabs(peak * peak - u * u) / (2.0f * a);
        const float rampDown = peak * peak / (2.0f * a);
        cruise = (dist - rampUp - rampDown) / peak;
        if (cruise < 0.0f) cruise = 0.0f;
    }
    addSegment(std::fabs(peak - u) / a, peak >= u ? s * a : -s * a);
    addSegment(cruise, 0.0f);
    addSegment(peak / a, -s * a);
    if (segCount_ == 0) reset(target);
}

float MotionPlanner::update(float dt) {
    if (dt <= 0.0f || segCount_ == 0) return pos_;
    segTime_ += dt;
    while (seg_ < segCount_ && segTime_ >= segs_[seg_].duration) {
        segTime_ -= segs_[seg_].duration;
        seg_++;
    }
    if (seg_ >= segCount_) {
        reset(target_);  // land exactly on the target
        return pos_;
    }
    const Segment& sg = segs_[seg_];
    pos_ = sg.pos0 + sg.vel0 * segTime_ + 0.5f * sg.accel * segTime_ * segTime_;
    vel_ = sg.vel0 + sg.accel * segTime_;
    return pos_;
}

float MotionPlanner::remaining() const {
    float t = -segTime_;
    for (int i = seg_; i < segCount_; i++) t += segs_[i].duration;
    return t > 0.0f ? t : 0.0f;
}
