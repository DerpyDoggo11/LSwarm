#pragma once
#include <Arduino.h>
#include "pins.h"

namespace Motors {

static constexpr uint16_t PWM_MAX  = 1000; 
static constexpr uint32_t PWM_FREQ = 24000;

inline void begin() {
    analogWriteFreq(PWM_FREQ);
    analogWriteRange(PWM_MAX);
    for (uint8_t i = 0; i < 4; i++) {
        pinMode(PIN_MOTOR[i], OUTPUT);
        digitalWrite(PIN_MOTOR[i], LOW);
    }
}

inline void set(uint8_t idx, uint16_t throttle) {
    if (idx > 3) return;
    if (throttle > PWM_MAX) throttle = PWM_MAX;
    analogWrite(PIN_MOTOR[idx], throttle);
}

inline void setAll(uint16_t t) { for (uint8_t i = 0; i < 4; i++) set(i, t); }

inline void disarm() {
    for (uint8_t i = 0; i < 4; i++) {
        analogWrite(PIN_MOTOR[i], 0);
        pinMode(PIN_MOTOR[i], OUTPUT);
        digitalWrite(PIN_MOTOR[i], LOW);
    }
}

} 