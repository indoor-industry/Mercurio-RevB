#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include "pins.h"
#include "config.h"
#include "mcp23017.h"
#include "display.h"
#include "touch.h"
#include "calibration.h"
#include "menu.h"
#include "board.h"
#include "test_sysinfo.h"
#include "test_battery.h"
#include "test_expander.h"
#include "test_touch.h"
#include "test_lora.h"
#include "test_sdcard.h"
#include "test_gps.h"
#include "test_audio_mic.h"
#include "test_audio_speaker.h"
#include "test_wifi_ble.h"
#include "test_backlight.h"
#include "test_calibration.h"
#include "volume.h"

MCP23017 mcp(Wire, MCP_I2C_ADDR);
unsigned long g_gpsBootMs = 0;

void setup() {
    Serial.begin(115200);

    // Bring the MCP23017 out of its hardware reset before talking to it.
    pinMode(PIN_MCP_RST, OUTPUT);
    digitalWrite(PIN_MCP_RST, LOW);
    delay(10);
    digitalWrite(PIN_MCP_RST, HIGH);
    delay(10);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(I2C_FREQ);
    if (!mcp.begin()) {
        Serial.println("WARNING: MCP23017 not responding on I2C bus");
    }

    Display::begin(mcp);
    Volume::begin();

    // SPI-A (touch + LoRa) on the global SPI object; the display/SD bus
    // has its own dedicated SPI host (see USE_HSPI_PORT in platformio.ini).
    SPI.begin(PIN_SPIA_SCLK, PIN_SPIA_MISO, PIN_SPIA_MOSI, -1);
    Touch::begin();

    // Holding SW1 during reset forces a fresh calibration even if valid
    // data is already stored in NVS - this is the escape hatch for when
    // stored calibration is bad and touch can't reach the menu's
    // "Recalibrate Touch" entry.
    bool forceCalib = !MCP23017::gpioBit(mcp.readGPIO(), MCP_BIT_SW1);
    if (!Calibration::load() || forceCalib) {
        Calibration::run();
    }

    // Wake the L76K GPS from standby (GPS_WKUP) and bring it out of
    // reset, then leave its UART open for the rest of the firmware's
    // life. The module is V_BCKP-backed for hot starts and must keep
    // running continuously - re-doing this every time the GPS test
    // screen opened was resetting the module before it could acquire a
    // fix.
    mcp.writePin(MCP_BIT_GPS_WKUP, true);
    mcp.writePin(MCP_BIT_GPS_RST, false);
    delay(100);
    mcp.writePin(MCP_BIT_GPS_RST, true);
    delay(1000); // let the module boot before we start reading its UART
    Serial1.setRxBufferSize(512);
    Serial1.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    g_gpsBootMs = millis();

    Menu::registerTest("System Info", testSysInfo);
    Menu::registerTest("Battery", testBattery);
    Menu::registerTest("I/O Expander", testExpander);
    Menu::registerTest("Touch Test", testTouch);
    Menu::registerTest("LoRa", testLora);
    Menu::registerTest("microSD", testSdCard);
    Menu::registerTest("GPS", testGps);
    Menu::registerTest("Microphone", testAudioMic);
    Menu::registerTest("Speaker", testAudioSpeaker);
    Menu::registerTest("WiFi / BLE", testWifiBle);
    Menu::registerTest("Backlight", testBacklight);
    Menu::registerTest("Recalibrate Touch", testRecalibrate);

    Menu::begin();
}

void loop() {
    Menu::loop();
}
