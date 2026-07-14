#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "pins.h"

namespace Hmi {

inline Adafruit_NeoPixel statusLed(1, PIN_STATUS_LED, NEO_GRB + NEO_KHZ800);
inline Adafruit_NeoPixel ledArray(LED_ARRAY_COUNT, PIN_LED_ARRAY, NEO_GRB + NEO_KHZ800);

inline void begin() {
    statusLed.begin();
    statusLed.setBrightness(40);
    statusLed.clear();
    statusLed.show();

    ledArray.begin();
    ledArray.setBrightness(80);
    ledArray.clear();
    ledArray.show();

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

inline void status(uint8_t r, uint8_t g, uint8_t b) {
    statusLed.setPixelColor(0, statusLed.Color(r, g, b));
    statusLed.show();
}

inline void array(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < LED_ARRAY_COUNT; i++)
        ledArray.setPixelColor(i, ledArray.Color(r, g, b));
    ledArray.show();
}

inline void beep(uint16_t ms = 80, uint16_t freq = 2700) {
    tone(PIN_BUZZER, freq, ms);
}

} 

namespace Power {

inline void begin() {
    pinMode(PIN_VBUS_SENSE, INPUT); 
    analogReadResolution(12);
}

inline bool usbPresent() { return digitalRead(PIN_VBUS_SENSE) == HIGH; }

inline float batteryVolts() {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < 16; i++) acc += analogRead(PIN_VBAT_ADC);
    float counts = acc / 16.0f;
    return (counts / 4095.0f) * ADC_VREF * VBAT_DIVIDER;
}

static constexpr float VBAT_WARN     = 3.60f;
static constexpr float VBAT_CRITICAL = 3.40f;

}