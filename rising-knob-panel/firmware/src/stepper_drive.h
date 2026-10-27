#pragma once

#include <cstdint>

#include "homing.h"

class FastAccelStepper;

// The lead-screw stepper, in mm. Steps are generated in hardware (RMT) by
// FastAccelStepper with acceleration ramps, so the knob glides.
class StepperDrive : public MotorPort {
public:
    bool begin(int stepPin, int dirPin, float stepsPerMm, bool dirHighRaises, float jerkRampMm);

    // MotorPort (used by homing): plain moves.
    float positionMm() override;
    bool moving() override;
    void moveToMm(float mm, float speedMmS, float accelMmS2) override;
    void stop() override;
    void setPositionMm(float mm) override;

    // Normal operation: call every tick with the wanted position. A target behind
    // the current direction of travel (hand returns while sinking) makes the knob
    // brake to a standstill first and then head the other way, so it never jerks.
    void track(float mm, float speedMmS, float accelMmS2);

private:
    int32_t toSteps(float mm) const;
    void apply(float speedMmS, float accelMmS2);

    FastAccelStepper* stepper_ = nullptr;
    float stepsPerMm_ = 1.0f;
};
