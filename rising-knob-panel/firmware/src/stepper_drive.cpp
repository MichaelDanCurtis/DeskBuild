#include "stepper_drive.h"

#include <FastAccelStepper.h>

#include <cmath>
#include <cstdlib>

namespace {
FastAccelStepperEngine engine;
constexpr uint16_t kDirChangeDelayUs = 50;
}  // namespace

bool StepperDrive::begin(int stepPin, int dirPin, float stepsPerMm, bool dirHighRaises, float jerkRampMm) {
    stepsPerMm_ = stepsPerMm;
    engine.init();
    stepper_ = engine.stepperConnectToPin(stepPin);
    if (!stepper_) return false;
    stepper_->setDirectionPin(dirPin, dirHighRaises, kDirChangeDelayUs);
    stepper_->setLinearAcceleration(static_cast<uint32_t>(jerkRampMm * stepsPerMm));
    return true;
}

int32_t StepperDrive::toSteps(float mm) const { return static_cast<int32_t>(std::lround(mm * stepsPerMm_)); }

void StepperDrive::apply(float speedMmS, float accelMmS2) {
    stepper_->setSpeedInHz(static_cast<uint32_t>(speedMmS * stepsPerMm_));
    stepper_->setAcceleration(static_cast<int32_t>(accelMmS2 * stepsPerMm_));
}

float StepperDrive::positionMm() { return stepper_ ? stepper_->getCurrentPosition() / stepsPerMm_ : 0.0f; }

bool StepperDrive::moving() { return stepper_ && stepper_->isRunning(); }

void StepperDrive::moveToMm(float mm, float speedMmS, float accelMmS2) {
    if (!stepper_) return;
    apply(speedMmS, accelMmS2);
    stepper_->moveTo(toSteps(mm));
}

void StepperDrive::stop() {
    if (stepper_) stepper_->stopMove();
}

void StepperDrive::setPositionMm(float mm) {
    if (stepper_) stepper_->setCurrentPosition(toSteps(mm));
}

void StepperDrive::track(float mm, float speedMmS, float accelMmS2) {
    if (!stepper_) return;
    const int32_t target = toSteps(mm);
    const int32_t pos = stepper_->getCurrentPosition();
    if (!stepper_->isRunning()) {
        if (pos != target) moveToMm(mm, speedMmS, accelMmS2);
        return;
    }
    if (stepper_->targetPos() == target) return;

    const int32_t velocity = stepper_->getCurrentSpeedInMilliHz(false);
    const int32_t delta = target - pos;
    const bool sameDirection = (velocity > 0 && delta > 0) || (velocity < 0 && delta < 0);
    if (sameDirection && static_cast<uint32_t>(std::abs(delta)) >= stepper_->stepsToStop()) {
        moveToMm(mm, speedMmS, accelMmS2);  // reachable without reversing: just retarget
    } else if (!stepper_->isStopping()) {
        stepper_->stopMove();  // brake; once stopped, the next call heads for the new target
    }
}
