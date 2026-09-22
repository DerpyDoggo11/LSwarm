#pragma once
#include <Arduino.h>

// Thin wrapper around the DW3000 UWB radio for the drone (UWB *tag*).
//
// Ranging uses single-sided two-way ranging (SS-TWR) via the Makerfabs DW3000
// Arduino library (https://github.com/Makerfabs/Makerfabs-ESP32-UWB-DW3000 ->
// library "DW3000"). The tag polls each anchor address in turn and reads back
// a distance in metres.
//
// ponytail: ranging state-machine bodies live in the library's SS_TWR example.
// This wrapper keeps the fleet-specific parts (pins, anchor list, antenna-delay
// calibration). If your installed library renames a call, fix it in uwb.cpp
// only -- nothing else in the firmware touches the radio.

namespace Uwb {

// Antenna delay is the ONE physical calibration knob. Put a tag and an anchor a
// known distance apart (e.g. 5.00 m), tune until the reported range matches.
// Units are DW3000 device-time ticks (~15.65 ps each).
static constexpr uint16_t ANTENNA_DELAY = 16385;

bool begin();                 // init radio; false if DW3000 not detected
bool ok();
uint32_t deviceId();

// Range to one anchor address. Returns true and fills metres on success.
bool rangeTo(uint8_t anchorAddr, float& metres);

}
