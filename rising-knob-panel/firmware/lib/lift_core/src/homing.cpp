#include "homing.h"

void Homing::fail(MotorPort& m, const char* why) {
    m.stop();
    error_ = why;
    state_ = State::Failed;
}

void Homing::start(MotorPort& m, bool tripped) {
    error_ = "";
    if (tripped) {
        m.moveToMm(m.positionMm() + cfg_.releaseMm, cfg_.fastMmS, cfg_.accelMmS2);
        state_ = State::Release;
    } else {
        m.moveToMm(m.positionMm() - cfg_.maxSeekMm, cfg_.fastMmS, cfg_.accelMmS2);
        state_ = State::FastSeek;
    }
}

void Homing::update(MotorPort& m, bool tripped) {
    switch (state_) {
        case State::Release:
            if (m.moving()) return;
            if (tripped) return fail(m, "home sensor stays tripped (blocked, unplugged or wrong polarity?)");
            m.moveToMm(m.positionMm() - cfg_.maxSeekMm, cfg_.fastMmS, cfg_.accelMmS2);
            state_ = State::FastSeek;
            return;

        case State::FastSeek:
            if (tripped) {
                tripMm_ = m.positionMm();
                m.stop();
                state_ = State::FastStop;
            } else if (!m.moving()) {
                fail(m, "home sensor never tripped");
            }
            return;

        case State::FastStop:
            if (m.moving()) return;
            m.moveToMm(tripMm_ + cfg_.backoffMm, cfg_.fastMmS, cfg_.accelMmS2);
            state_ = State::Backoff;
            return;

        case State::Backoff:
            if (m.moving()) return;
            if (tripped) return fail(m, "home sensor did not clear on back-off");
            m.moveToMm(m.positionMm() - 2.0f * cfg_.backoffMm, cfg_.slowMmS, cfg_.accelMmS2);
            state_ = State::SlowSeek;
            return;

        case State::SlowSeek:
            if (tripped) {
                tripMm_ = m.positionMm();
                m.stop();
                state_ = State::SlowStop;
            } else if (!m.moving()) {
                fail(m, "home sensor did not trip on the slow approach");
            }
            return;

        case State::SlowStop:
            if (m.moving()) return;
            m.setPositionMm(m.positionMm() - tripMm_);  // the trip point is 0
            state_ = State::Done;
            return;

        default:
            return;
    }
}
