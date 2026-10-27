#pragma once

#include <cstdint>

// PCF8574 8-bit I2C expander on the shared Wire bus. Its pins are quasi-bidirectional:
// write 1 to a pin to use it as an input (weak pull-up) or as a high output.
class Pcf8574 {
public:
    bool begin(uint8_t address);
    bool ok() const { return ok_; }

    bool setBit(uint8_t bit, bool high);
    // Returns false if the expander didn't answer.
    bool readBit(uint8_t bit, bool& high);

private:
    bool write();

    uint8_t addr_ = 0x20;
    uint8_t out_ = 0xFF;
    bool ok_ = false;
};
