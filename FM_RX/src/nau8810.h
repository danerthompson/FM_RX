#ifndef NAU8810_h
#define NAU8810_h

#include <Wire.h>

#define NAU_RESET_ADDR 0x00  // Software reset address
#define NAU_RESET_CMD  0x000 // Can write any data to reset address to reset all registers

#define NAU_POWER2_ADDR 0x01    // Power management 2 address
#define NAU_POWER2_CMD  0x0E0   // Set Mono out, Speaker+/- bits high


class NAU8810 {
    public:
        NAU8810( int ADDR, TwoWire *i2c_wire );
        uint8_t begin();

    private:
        int _addr;
        TwoWire *_wire;
        uint8_t writeToRegister( uint8_t register, uint16_t value );
};



#endif