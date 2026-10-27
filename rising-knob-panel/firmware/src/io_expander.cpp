#include "io_expander.h"

#include <Wire.h>

bool Pcf8574::begin(uint8_t address) {
    addr_ = address;
    out_ = 0xFF;  // all inputs / high, which is also the power-on state
    ok_ = write();
    return ok_;
}

bool Pcf8574::write() {
    Wire.beginTransmission(addr_);
    Wire.write(out_);
    ok_ = Wire.endTransmission() == 0;
    return ok_;
}

bool Pcf8574::setBit(uint8_t bit, bool high) {
    const uint8_t next = high ? (out_ | (1u << bit)) : (out_ & ~(1u << bit));
    if (next == out_ && ok_) return true;
    out_ = next;
    return write();
}

bool Pcf8574::readBit(uint8_t bit, bool& high) {
    if (Wire.requestFrom(addr_, static_cast<uint8_t>(1)) != 1) {
        ok_ = false;
        return false;
    }
    ok_ = true;
    high = (Wire.read() >> bit) & 1;
    return true;
}
