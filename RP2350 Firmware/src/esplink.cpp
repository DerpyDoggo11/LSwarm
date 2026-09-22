#include "esplink.h"
#include "motors.h"
#include "hmi.h"
#include <cstring>

namespace EspLink {


void begin() {
    pinMode(PIN_ESP_EN, OUTPUT);
    pinMode(PIN_ESP_BOOT, OUTPUT);
#if defined(EXTERNAL_ESP)
    // On-board ESP32-C3 is unused; an external ESP32-S3 drives the header UART.
    // Hold the C3 in reset so it can't contend on the shared TX/RX lines.
    digitalWrite(PIN_ESP_EN, LOW);
#else
    digitalWrite(PIN_ESP_EN, HIGH);
#endif
    digitalWrite(PIN_ESP_BOOT, HIGH);

    Serial1.setTX(PIN_ESP_TX);
    Serial1.setRX(PIN_ESP_RX);
    Serial1.setFIFOSize(512);
    Serial1.begin(LINK_BAUD);
}

void reset() {
    digitalWrite(PIN_ESP_BOOT, HIGH);
    digitalWrite(PIN_ESP_EN, LOW);
    delay(50);
    digitalWrite(PIN_ESP_EN, HIGH);
    delay(200);
}

void enterBootloader() {
    digitalWrite(PIN_ESP_BOOT, LOW);
    digitalWrite(PIN_ESP_EN, LOW);
    delay(50);
    digitalWrite(PIN_ESP_EN, HIGH);
    delay(100);
    digitalWrite(PIN_ESP_BOOT, HIGH);
}

void hold() {
    digitalWrite(PIN_ESP_EN, LOW);
}

[[noreturn]] void passthrough() {
    Motors::disarm();
    Hmi::status(0, 0, 60);

    // SerialUSB (native USB CDC) exposes no host-baud getter, so the UART to the
    // ESP32 runs at a fixed rate. Flash with:  esptool --before default_reset --baud 115200
    Serial1.begin(LINK_BAUD);

    bool lastDtr = false, lastRts = false;

    for (;;) {
        // Direct-drive auto-reset: DTR -> BOOT(IO0), RTS -> EN(reset), both active-low.
        // (No auto-reset transistors on this board, so map straight through.)
        bool dtr = Serial.dtr();
        bool rts = Serial.rts();
        if (dtr != lastDtr || rts != lastRts) {
            digitalWrite(PIN_ESP_BOOT, dtr ? LOW : HIGH);
            digitalWrite(PIN_ESP_EN,   rts ? LOW : HIGH);
            lastDtr = dtr;
            lastRts = rts;
        }

        while (Serial.available() && Serial1.availableForWrite())
            Serial1.write((uint8_t)Serial.read());
        while (Serial1.available() && Serial.availableForWrite())
            Serial.write((uint8_t)Serial1.read());
    }
}

void pollForBridgeRequest() {
    static char buf[16];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            buf[n] = 0;
            if (strcmp(buf, "esp_bridge") == 0) {
                Serial.println("RP2350 entering ESP32 programming mode");
                Serial.flush();
                delay(50);
                passthrough();
            }
            n = 0;
        } else if (n < sizeof(buf) - 1) {
            buf[n++] = c;
        }
    }
}

}
