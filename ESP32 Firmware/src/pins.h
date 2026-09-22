#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// External ESP32-S3 Super Mini pin map (drone).
//
// The on-board ESP32-C3-MINI (U8) is bypassed. An external ESP32-S3 Super Mini
// is wired to the 6-pin "ESP32" header (GND 3V3 BOOT EN TX RX) for the RP2350
// link, and (optionally) to a DW3000 UWB add-on module for anchor ranging.
//
// Pick only "safe" S3 GPIOs. Avoid 0/3/45/46 (strapping), 19/20 (USB), 43/44
// (USB-serial-jtag TX/RX) and 48 (on-board RGB LED).
// ---------------------------------------------------------------------------

// UART link to the RP2350 (Serial1). Cross-wired at the header:
//   header TX (RP2350 GPIO0, RP2350 out) -> S3 RX below
//   header RX (RP2350 GPIO1, RP2350 in)  <- S3 TX below
static constexpr int PIN_LINK_RX = 4;
static constexpr int PIN_LINK_TX = 5;
static constexpr uint32_t LINK_BAUD = 115200;

// On-board DW3000 UWB tag (SPI). The tag is on the drone PCB; its DW nets route
// to the dead ESP32-C3 pads. Tap them there and run to these S3 GPIOs. The C3
// pin each DW net lands on (find the pad):
//   WAKEUP=C3 IO1  IRQ=C3 IO3  MISO=C3 IO4  MOSI=C3 IO5
//   CLK=C3 IO6     CS=C3 IO7   RST=C3 IO10
static constexpr int PIN_DW_MOSI   = 11;
static constexpr int PIN_DW_SCK    = 12;
static constexpr int PIN_DW_MISO   = 13;
static constexpr int PIN_DW_CS     = 10;
static constexpr int PIN_DW_IRQ    = 9;
static constexpr int PIN_DW_RST    = 8;
static constexpr int PIN_DW_WAKEUP = 7;
