#include "mcp23017.h"
#include "pins.h"

MCP23017::MCP23017(TwoWire &wire, uint8_t addr)
    : _wire(wire), _addr(addr), _olat(0) {}

void MCP23017::writeReg8(uint8_t reg, uint8_t val) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    _wire.write(val);
    _wire.endTransmission();
}

uint8_t MCP23017::readReg8(uint8_t reg) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    _wire.endTransmission(false);
    _wire.requestFrom(_addr, (uint8_t)1);
    if (_wire.available()) {
        return _wire.read();
    }
    return 0;
}

void MCP23017::writeReg16(uint8_t regLow, uint16_t val) {
    _wire.beginTransmission(_addr);
    _wire.write(regLow);
    _wire.write((uint8_t)(val & 0xFF));
    _wire.write((uint8_t)(val >> 8));
    _wire.endTransmission();
}

bool MCP23017::begin() {
    // Probe the device first.
    _wire.beginTransmission(_addr);
    if (_wire.endTransmission() != 0) {
        return false;
    }

    // IOCON: enable INTA/INTB mirroring (bit6 = MIRROR).
    writeReg8(MCP_REG_IOCON, 0x40);

    // IODIR: 1 = input, 0 = output.
    // Port A: GPS_RST(0)=out, BAT_STAT1(1)=in, DISP_RST(2)=out,
    //         GPS_WKUP(3)=out, GPS_1PPS(4)=in, AUDIO_EN(5)=out,
    //         SW1(6)=in, GPA7(7)=out (unused, errata)
    writeReg8(MCP_REG_IODIRA, 0x52);
    // Port B: SW2(0)=in, DIO2(1)=in, DIO3(2)=in, GPB3(3)=in (unused),
    //         TOUCH_IRQ(4)=in, BAT_STAT2(5)=in, SD_DET(6)=in,
    //         GPB7(7)=out (unused, errata)
    writeReg8(MCP_REG_IODIRB, 0x7F);

    // GPPU: pull-ups on inputs that are open-drain / floating-when-idle.
    // Port A: BAT_STAT1(1), SW1(6)
    writeReg8(MCP_REG_GPPUA, 0x42);
    // Port B: SW2(0), GPB3(3, unused), TOUCH_IRQ(4), BAT_STAT2(5), SD_DET(6)
    writeReg8(MCP_REG_GPPUB, 0x79);

    // Initial output levels: GPS_RST and DISP_RST held HIGH (inactive,
    // assuming active-low resets); everything else low.
    _olat = (1u << MCP_BIT_GPS_RST) | (1u << MCP_BIT_DISP_RST);
    writeReg16(MCP_REG_OLATA, _olat);

    return true;
}

uint16_t MCP23017::readGPIO() {
    _wire.beginTransmission(_addr);
    _wire.write(MCP_REG_GPIOA);
    _wire.endTransmission(false);
    _wire.requestFrom(_addr, (uint8_t)2);
    uint16_t val = 0;
    if (_wire.available() >= 2) {
        uint8_t a = _wire.read();
        uint8_t b = _wire.read();
        val = (uint16_t)a | ((uint16_t)b << 8);
    }
    return val;
}

void MCP23017::writePin(uint8_t bit, bool level) {
    if (level) {
        _olat |= (1u << bit);
    } else {
        _olat &= ~(1u << bit);
    }
    writeReg16(MCP_REG_OLATA, _olat);
}

void MCP23017::pulseDisplayReset() {
    writePin(MCP_BIT_DISP_RST, false);
    delay(10);
    writePin(MCP_BIT_DISP_RST, true);
    delay(10);
}
