#ifndef NAU8810_h
#define NAU8810_h

#include <Wire.h>

#define NAU_RESET_ADDR 0x00  // Software reset address
#define NAU_RESET_CMD  0x0000 // Can write any data to reset address to reset all registers

#define NAU_POWER1_ADDR 0x01
#define NAU_POWER1_CMD  0x013F

#define NAU_POWER2_ADDR 0x02
#define NAU_POWER2_CMD 0x0015

#define NAU_POWER3_ADDR 0x03    // Power management 3 address
#define NAU_POWER3_CMD  0x00ED   // Set Mono out, Speaker+/- bits, mono mixer, speaker mixer, DAC enable high

#define NAU_SPEAKER_MIXER_ADDR 0x32
#define NAU_SPEAKER_MIXER_CMD  0x0003    // Bypass path to speaker

#define NAU_PGA_GAIN_ADDR   0x2D
#define NAU_PGA_GAIN_CMD    0x0032   // 35.25 dB of PGA gain at the MIC input

#define NAU_ADC_CTRL_ADDR   0x0E
#define NAU_ADC_CTRL_CMD    0x0000  // Turn off HPF?

#define NAU_CLK_CTRL_ADDR   0x06
#define NAU_CLK_CTRL_CMD    0x0020   // Divide MCLK by 1.5, bypass PLL, in slave mode

#define NAU_EQ_CTRL1_ADDR   0x12
#define NAU_EQ_CTRL1_CMD    0x002C

// try preemphasis



#define NAU_INPUT_CTRL_ADDR 0x2C
#define NAU_INPUT_CTRL_CMD  0x0000   // Disconnect negative mic PGA?

#define NAU_ADC_LOOPBACK_ADDR 0x05
#define NAU_ADC_LOOPBACK_CMD  0x0001    // Enables ADC to DAC loopback (This is probably talking about I2S interface)

#define NAU_SPEAKER_GAIN_ADDR 0x36
#define NAU_SPEAKER_GAIN_CMD  0x003F    // +6 dB of speaker gain

#define NAU_ALC1_CTRL_ADDR  0x20
#define NAU_ALC1_CTRL_CMD   0x138   // Enable ALC, max max gain and min min gain

#define NAU_ALC2_CTRL_ADDR  0x21

#define NAU_OUT_CTRL_ADDR   0x45
#define NAU_OUT_CTRL_CMD    0x20    // Connect speaker output to mono output

#define NAU_DAC_CTRL_ADDR   0x0A
#define NAU_DAC_CTRL_CMD    0x0030  // Turn on de-emphasis


#define NAU_PLL1_ADDR 0x24
#define NAU_PLL2_ADDR 0x25
#define NAU_PLL3_ADDR 0x26
#define NAU_PLL4_ADDR 0x27


class NAU8810 {
    public:
        NAU8810( int ADDR, TwoWire *i2c_wire );
        uint8_t begin();
        uint16_t readRegister( uint8_t reg );
        uint8_t setSpeakerVolume( uint8_t volume );
        uint8_t setPLL(uint32_t inputFreq);
        uint8_t setALCGain(uint8_t volume);
        uint8_t writeToRegister( uint8_t reg, uint16_t value );
        uint8_t setEQGain(uint8_t band, uint8_t volume);
        uint8_t setOutput(uint8_t output);

    private:
        int _addr;
        TwoWire *_wire;
        
        
};



#endif