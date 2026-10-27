#pragma once

// Finds the carriage's absolute position with the home sensor, without crashing
// into anything: a fast approach, a back-off, then a slow approach whose trip point
// becomes position 0.

class MotorPort {
public:
    virtual ~MotorPort() = default;
    virtual float positionMm() = 0;
    virtual bool moving() = 0;
    virtual void moveToMm(float mm, float speedMmS, float accelMmS2) = 0;
    virtual void stop() = 0;  // decelerate to a standstill
    virtual void setPositionMm(float mm) = 0;
};

class Homing {
public:
    struct Config {
        float fastMmS = 3.0f;
        float slowMmS = 0.3f;
        float accelMmS2 = 30.0f;
        float releaseMm = 3.0f;   // move off a sensor that is already tripped
        float backoffMm = 1.0f;
        float maxSeekMm = 20.0f;  // give up if the sensor hasn't tripped by then
    };

    enum class State { Idle, Release, FastSeek, FastStop, Backoff, SlowSeek, SlowStop, Done, Failed };

    Homing() = default;
    explicit Homing(const Config& c) : cfg_(c) {}

    void start(MotorPort& m, bool tripped);
    // Call every control tick with the current sensor state.
    void update(MotorPort& m, bool tripped);

    State state() const { return state_; }
    bool busy() const { return state_ != State::Idle && state_ != State::Done && state_ != State::Failed; }
    const char* error() const { return error_; }

private:
    void fail(MotorPort& m, const char* why);

    Config cfg_;
    State state_ = State::Idle;
    float tripMm_ = 0.0f;
    const char* error_ = "";
};
