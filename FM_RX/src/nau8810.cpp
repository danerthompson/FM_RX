#include <nau8810.h>

NAU8810::NAU8810(int ADDR, TwoWire *i2c_wire)
{
    _addr = ADDR;
    _wire = i2c_wire;
}

uint8_t NAU8810::writeToRegister(uint8_t reg, uint16_t value) {
    uint8_t data[2];
    data[0] = (reg << 1) | ((value >> 8) & 0x0001);     // First seven bits are register address, last bit is MSB of 9-bit value data
    data[1] = value & 0x00FF;                           // Last 8 bits of 9-bit value data
    _wire->beginTransmission(_addr);
    _wire->write(data, 2);
    return _wire->endTransmission();    // Zero means success
}

uint8_t NAU8810::begin()
{
    uint8_t err;
    err = writeToRegister(NAU_RESET_ADDR, NAU_RESET_CMD);
    if (!err) {
        err = writeToRegister(NAU_POWER2_ADDR, NAU_POWER2_CMD);
    }
    return err;     // Zero means success
}