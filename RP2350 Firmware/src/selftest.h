#pragma once
#include <Arduino.h>
#include <math.h>
#include "hmi.h"
#include "sensors.h"

// Non-destructive peripheral self-test (everything except spinning motors).
// Motors are tested separately via the 'mtest' / 'm' commands in main.cpp so
// that spinning is always an explicit, deliberate action.
namespace SelfTest {

inline bool testStatusLed() {
    Serial.print("  status LED  (SK6812)  R/G/B ... ");
    const uint8_t seq[3][3] = {{80, 0, 0}, {0, 80, 0}, {0, 0, 80}};
    for (auto& c : seq) { Hmi::status(c[0], c[1], c[2]); delay(350); }
    Hmi::status(0, 0, 0);
    Serial.println("done - verify it cycled red/green/blue");
    return true;
}

inline bool testLedArray() {
    Serial.print("  bottom LEDs (4x SK6812) R/G/B ... ");
    const uint8_t seq[3][3] = {{80, 0, 0}, {0, 80, 0}, {0, 0, 80}};
    for (auto& c : seq) { Hmi::array(c[0], c[1], c[2]); delay(450); }
    Hmi::array(0, 0, 0);
    Serial.println("done - verify all 4 cycled red/green/blue");
    return true;
}

inline bool testBuzzer() {
    Serial.print("  buzzer      (MLT-5020) ... ");
    Hmi::beep(150, 2000); delay(250);
    Hmi::beep(150, 3000); delay(250);
    Serial.println("done - verify you heard two tones");
    return true;
}

inline bool testImu() {
    Serial.print("  IMU  (LSM6DSR) ... ");
    if (!Sensors::imuOk()) { Serial.println("FAIL - not detected (check SPI wiring / CS on GPIO7)"); return false; }
    ImuSample s;
    if (!Sensors::readImu(s)) { Serial.println("FAIL - read error"); return false; }
    float amag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
    Serial.printf("ok  acc=%.2f,%.2f,%.2f g (|a|=%.2f)  gyro=%.1f,%.1f,%.1f dps\n",
                  s.ax, s.ay, s.az, amag, s.gx, s.gy, s.gz);
    if (amag < 0.6f || amag > 1.4f)
        Serial.println("    WARN: |acc| not ~1g - hold the board still and level to re-check");
    return true;
}

inline bool testBaro() {
    Serial.print("  BARO (BMP581)  ... ");
    if (!Sensors::baroOk()) { Serial.println("FAIL - not detected (check SPI wiring / CS on GPIO9)"); return false; }
    BaroSample s;
    if (!Sensors::readBaro(s)) { Serial.println("FAIL - read error"); return false; }
    Serial.printf("ok  %.1f Pa  %.2f C  alt %.1f m\n", s.pressure_pa, s.temperature_c, s.altitude_m);
    if (s.pressure_pa < 30000.0f || s.pressure_pa > 110000.0f)
        Serial.println("    WARN: pressure outside 300-1100 hPa - suspect bad reading");
    return true;
}

// Runs the full non-motor self-test. Returns true if IMU + baro both pass
// (LED/buzzer are pass/fail-by-eye-and-ear and always "pass" here).
inline bool run() {
    Serial.println("=== SELF TEST (motors excluded) ===");
    testStatusLed();
    testLedArray();
    testBuzzer();
    bool imu  = testImu();
    bool baro = testBaro();
    bool ok = imu && baro;
    Serial.printf("=== SENSORS: %s ===\n", ok ? "PASS" : "CHECK FAILURES ABOVE");
    Serial.println("Remove propellers, then run 'mtest' to spin-test all 4 motors,");
    Serial.println("or 'm <0-3> <0-1000>' to drive one motor directly.");
    return ok;
}

}
