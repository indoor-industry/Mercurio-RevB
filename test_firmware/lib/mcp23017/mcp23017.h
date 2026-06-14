#pragma once

#include <Arduino.h>
#include <Wire.h>

// Minimal MCP23017 driver for the LoRa-device RevB I/O expander.
// Configures all 16 pins per the board's fixed bit map (see pins.h) and
// provides a combined 16-bit GPIO read plus simple output helpers.
class MCP23017 {
public:
    MCP23017(TwoWire &wire, uint8_t addr);

    // Configure IODIR / GPPU / IOCON.MIRROR and set initial output levels.
    // Returns false if the device does not respond on the I2C bus.
    bool begin();

    // Read both GPIOA and GPIOB in a single I2C transaction.
    // Bit n = GPA(n) for n<8, GPB(n-8) for n>=8.
    uint16_t readGPIO();

    // Set an output pin (bit index per pins.h MCP_BIT_* map) high/low.
    void writePin(uint8_t bit, bool level);

    // Read a single bit from the most recent readGPIO() result.
    static bool gpioBit(uint16_t gpio, uint8_t bit) {
        return (gpio >> bit) & 0x01;
    }

    // Pulse the display reset line (GPA2): low for 10ms, then high.
    void pulseDisplayReset();

private:
    TwoWire &_wire;
    uint8_t _addr;
    uint16_t _olat; // shadow of OLATA/OLATB

    void writeReg8(uint8_t reg, uint8_t val);
    uint8_t readReg8(uint8_t reg);
    void writeReg16(uint8_t regLow, uint16_t val);
};
