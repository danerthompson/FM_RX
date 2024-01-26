#include <nau8810.h>

NAU8810::NAU8810(int ADDR, TwoWire *i2c_wire)
{
    _addr = ADDR;
    _wire = i2c_wire;
}

uint8_t NAU8810::writeToRegister(uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    data[0] = (reg << 1) | ((value >> 8) & 0x0001); // First seven bits are register address, last bit is MSB of 9-bit value data
    data[1] = value & 0x00FF;                       // Last 8 bits of 9-bit value data
    _wire->beginTransmission(_addr);
    _wire->write(data[0]);
    _wire->write(data[1]);
    return _wire->endTransmission(); // Zero means success
}

uint16_t NAU8810::readRegister(uint8_t reg)
{
    uint16_t data;

    _wire->beginTransmission(_addr);
    _wire->write(reg << 1);
    _wire->endTransmission();

    _wire->requestFrom(_addr, 2);
    data = _wire->read();  // Read first byte
    data = data << 8;      // Shift over 8 bits
    data |= _wire->read(); // Read second byte
    return data;
}

uint8_t NAU8810::setEQGain(uint8_t band, uint8_t volume) {
    if (volume > 0x1F) {
        volume = 0x1F;
    }

    if (band == 1) {
        return writeToRegister(0x12+band-1, 0x0120 | volume);
    }
    else {
        return writeToRegister(0x12+band-1, 0x0020 | volume);
    }
}

uint8_t NAU8810::setSpeakerVolume(uint8_t volume)
{
    // Speaker volume can range from 0 to 63 (-53 to +6 dB of speaker gain in 1 dB increments)
    if (volume > 63)
    { // Check max value
        volume = 63;
    }

    uint8_t settings = readRegister(NAU_SPEAKER_GAIN_ADDR) & 0x0C0;
    return writeToRegister(NAU_SPEAKER_GAIN_ADDR, settings | volume);
}

uint8_t NAU8810::setALCGain(uint8_t volume)
{
    return writeToRegister(NAU_ALC2_CTRL_ADDR, volume);
}

uint8_t NAU8810::setOutput(uint8_t output) {
    // If output = 0, output on speaker, if output = 1, output on mono
    uint8_t volume = readRegister(NAU_SPEAKER_GAIN_ADDR) & 0x03F;
    if (output == 0) {
        writeToRegister(NAU_SPEAKER_GAIN_ADDR, volume);     // Write volume and unmute speaker
        return writeToRegister(NAU_OUT_CTRL_ADDR, 0x010);   // Mute mono
    }
    else {
        writeToRegister(NAU_SPEAKER_GAIN_ADDR, 0x040 | volume); // Write volume and mute speaker
        return writeToRegister(NAU_OUT_CTRL_ADDR, 0x000);       // Unmute mono
    }
}

uint8_t NAU8810::setPLL(uint32_t inputFreq)
{
    uint8_t err;
    // inputFreq is the frequency sent to the MCLK pin on the codec
    // Assume desired 12.288 MHz at this point
    float R = 12288000.0 * 4.0 / (float)inputFreq; // *4 to compensate for divide by 4? *2 for M/2?
    if (R < 5.0 || R > 13.0)
    {
        return 1; // Error, integer divider can't be less than 5
    }
    uint8_t integer = (uint8_t)R; // Integer part of divider

    float K = (R - (float)integer) * 16777216; // Take the decimal portion * 2^24
    uint32_t fractional = (uint32_t)K;

    err = writeToRegister(NAU_PLL1_ADDR, integer);
    if (!err)
    {
        err = writeToRegister(NAU_PLL2_ADDR, fractional >> 18);
        if (!err)
        {
            err = writeToRegister(NAU_PLL3_ADDR, (fractional & 0x0003FE00) >> 9);
            if (!err)
            {
                err = writeToRegister(NAU_PLL4_ADDR, fractional & 0x000001FF);
            }
        }
    }
    return R;
}

uint8_t NAU8810::begin()
{

    _wire->begin();
    uint8_t err;
    // Could split these settings into different commands if wanted, I used these defaults for my case
    err = writeToRegister(NAU_RESET_ADDR, NAU_RESET_CMD);
    if (!err)
    {
        err = writeToRegister(NAU_POWER1_ADDR, NAU_POWER1_CMD);
        if (!err)
        {
            err = writeToRegister(NAU_POWER2_ADDR, NAU_POWER2_CMD);
            if (!err)
            {
                err = writeToRegister(NAU_POWER3_ADDR, NAU_POWER3_CMD);
                if (!err)
                {
                    err = writeToRegister(NAU_ADC_LOOPBACK_ADDR, NAU_ADC_LOOPBACK_CMD);
                    if (!err)
                    {
                        err = writeToRegister(NAU_CLK_CTRL_ADDR, NAU_CLK_CTRL_CMD);
                        if (!err)
                        {
                            err = writeToRegister(NAU_ALC1_CTRL_ADDR, NAU_ALC1_CTRL_CMD);
                            if (!err)
                            {
                                err = writeToRegister(NAU_OUT_CTRL_ADDR, NAU_OUT_CTRL_CMD);
                                if (!err)
                                {
                                    err = writeToRegister(NAU_DAC_CTRL_ADDR, NAU_DAC_CTRL_CMD);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return err; // Zero means success

    // UNUSED COMMANDS:
    // err = writeToRegister(NAU_INPUT_CTRL_ADDR, NAU_INPUT_CTRL_CMD);
    // err = writeToRegister(NAU_SPEAKER_GAIN_ADDR, NAU_SPEAKER_GAIN_CMD);
    // err = writeToRegister(NAU_ADC_CTRL_ADDR, NAU_ADC_CTRL_CMD);
    // err = writeToRegister(NAU_PGA_GAIN_ADDR, NAU_PGA_GAIN_CMD);
    // err = writeToRegister(NAU_EQ_CTRL1_ADDR, NAU_EQ_CTRL1_CMD);
    // err = writeToRegister(NAU_SPEAKER_MIXER_ADDR, NAU_SPEAKER_MIXER_CMD);
}