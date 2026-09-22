#include "uwb.h"
#include "pins.h"

// SS-TWR tag built on the Makerfabs "DW3000" library. That library owns the
// register map and the frame timestamping; we only drive its high-level calls.
//
// Add to platformio.ini:
//   lib_deps = https://github.com/Makerfabs/Makerfabs-ESP32-UWB-DW3000.git
//   build_flags = -DUSE_UWB=1
//
// The library's SPI pins are set once here via its constructor/config. If your
// library version exposes different setters, this is the only file to touch.

#if defined(USE_UWB)
#include "DW3000.h"

static bool s_ok = false;

bool Uwb::begin() {
    // Library uses the default VSPI bus; point it at our wiring.
    SPI.begin(PIN_DW_SCK, PIN_DW_MISO, PIN_DW_MOSI, PIN_DW_CS);
    DW3000.begin();
    DW3000.hardReset();
    delay(200);
    if (!DW3000.checkForIDLE()) { s_ok = false; return false; }
    DW3000.softReset();
    delay(200);
    DW3000.init();
    DW3000.setupGPIO();
    DW3000.configureAsTX();          // tag drives the poll
    DW3000.setTXAntennaDelay(Uwb::ANTENNA_DELAY);
    DW3000.clearSystemStatus();
    s_ok = true;
    return true;
}

bool Uwb::ok() { return s_ok; }
uint32_t Uwb::deviceId() { return s_ok ? DW3000.readDeviceID() : 0; }

// One SS-TWR exchange with a single anchor. Mirrors the library SS_TWR
// initiator example, condensed to a blocking call with a short timeout.
bool Uwb::rangeTo(uint8_t anchorAddr, float& metres) {
    if (!s_ok) return false;

    DW3000.ds_sendFrame(1);                 // poll frame, payload = round id
    DW3000.setDestinationAddress(anchorAddr);

    uint32_t t0 = millis();
    while (!DW3000.receivedFrameSucc()) {    // wait for the response
        if (millis() - t0 > 12) { DW3000.clearSystemStatus(); return false; }
    }
    DW3000.clearSystemStatus();

    // Library computes range in cm from the four SS-TWR timestamps.
    double cm = DW3000.getRangeResult();
    if (cm <= 0.0 || cm > 30000.0) return false;   // reject junk (>300 m)
    metres = (float)(cm / 100.0);
    return true;
}

#else   // USE_UWB not set -> no radio populated, ranging disabled.

bool Uwb::begin()                         { return false; }
bool Uwb::ok()                            { return false; }
uint32_t Uwb::deviceId()                  { return 0; }
bool Uwb::rangeTo(uint8_t, float& m)      { m = 0; return false; }

#endif
