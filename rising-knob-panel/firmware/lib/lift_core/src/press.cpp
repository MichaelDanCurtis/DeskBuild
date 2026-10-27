#include "press.h"

PressDetector::Event PressDetector::update(uint32_t nowMs, bool rawPressed) {
    if (rawPressed != raw_) {
        raw_ = rawPressed;
        rawSinceMs_ = nowMs;
    }
    Event ev = Event::None;
    const bool stable = nowMs - rawSinceMs_ >= debounceMs_;
    if (stable && raw_ != pressed_) {
        pressed_ = raw_;
        if (pressed_) {
            pressedSinceMs_ = nowMs;
            longFired_ = false;
        } else if (!longFired_) {
            ev = Event::Short;
        }
    }
    if (pressed_ && !longFired_ && nowMs - pressedSinceMs_ >= longMs_) {
        longFired_ = true;
        ev = Event::Long;
    }
    return ev;
}
