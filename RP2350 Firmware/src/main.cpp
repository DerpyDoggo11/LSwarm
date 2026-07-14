#include <Arduino.h>
#include "pins.h"
#include "sensors.h"
#include "motors.h"
#include "hmi.h"
#include "esp_link.h"

#if defined(ESP_PASSTHROUGH_ONLY)

void setup() {
  Serial.begin(115200);
  Motors::disarm();
  Hmi::begin();
  EspLink::begin();
  EspLink::passthrough();
}

void loop() {}

#else

static volatile bool g_armed = false;
static volatile float g_vbat = 0.0f;
static volatile bool g_sensorsOk = false;

static volatile uint16_t g_throttle[4] = {0, 0, 0, 0};

void setup() {
  Serial.begin(115200);

  Power::begin();
  Hmi::begin();
  Motors::disarm();
  EspLink::begin();

  Hmi::status(60, 30, 0);

  g_sensorsOk = Sensors::begin();

  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) { 
    // wait for USB 
  }

  Serial.println("Init RP2350");
  Serial.printf("IMU  (LSM6DSR): %s\n", Sensors::imuOk()  ? "ok" : "FAIL");
  Serial.printf("BARO (BMP581) : %s\n", Sensors::baroOk() ? "ok" : "FAIL");
  Serial.printf("VBUS: %s   VBAT: %.2f V\n", Power::usbPresent() ? "present" : "absent", Power::batteryVolts());
  Serial.println("Type 'esp_bridge' to flash the ESP32-C3, 'help' for more.");

  Hmi::status(0, 40, 0);
  Hmi::beep(60);
}

static void handleCommand(const char* cmd) {
  if (!strcmp(cmd, "help")) {
        Serial.println("arm | disarm | t <0-3> <0-1000> | stat | " "esp_reset | esp_boot | esp_bridge");
  } else if (!strcmp(cmd, "arm")) {
      if (!g_sensorsOk) { Serial.println("refused: sensors failed"); return; }
      if (g_vbat < Power::VBAT_CRITICAL && !Power::usbPresent()) {
          Serial.println("refused: battery low"); return;
      }
      g_armed = true;
      Hmi::beep(120);
      Serial.println("ARMED");
  } else if (!strcmp(cmd, "disarm")) {
      g_armed = false;
      for (uint8_t i = 0; i < 4; i++) g_throttle[i] = 0;
      Serial.println("disarmed");
  } else if (!strncmp(cmd, "t ", 2)) {
      int idx, val;
      if (sscanf(cmd + 2, "%d %d", &idx, &val) == 2 && idx >= 0 && idx < 4) {
          g_throttle[idx] = (uint16_t)constrain(val, 0, Motors::PWM_MAX);
          Serial.printf("motor %d = %d\n", idx, g_throttle[idx]);
      }
  } else if (!strcmp(cmd, "stat")) {
      ImuSample imu; BaroSample baro;
      Sensors::readImu(imu); Sensors::readBaro(baro);
      Serial.printf("vbat %.2fV  usb %d  armed %d\n", g_vbat, Power::usbPresent(), g_armed);
      Serial.printf("acc %.2f %.2f %.2f g | gyro %.1f %.1f %.1f dps\n", imu.ax, imu.ay, imu.az, imu.gx, imu.gy, imu.gz);
      Serial.printf("baro %.1f Pa  %.1f C  alt %.1f m\n", baro.pressure_pa, baro.temperature_c, baro.altitude_m);
  } else if (!strcmp(cmd, "esp_reset")) {
      EspLink::reset();  Serial.println("ESP32 reset");
  } else if (!strcmp(cmd, "esp_boot")) {
      EspLink::enterBootloader(); Serial.println("ESP32 in bootloader");
  } else if (!strcmp(cmd, "esp_bridge")) {
      g_armed = false;
      Motors::disarm();
      Serial.println("entering programmer mode - reset RP2350 to exit");
      Serial.flush();
      EspLink::passthrough();
  }
}

void loop() {
  static char line[48];
  static uint8_t n = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
        if (n) { 
          line[n] = 0; handleCommand(line); n = 0; 
        }
      } else if (n < sizeof(line) - 1) {
        line[n++] = c;
      }
  }

  while (Serial1.available()) {
      char c = (char)Serial1.read();
      Serial.write(c);
  }

  static uint32_t tLast = 0;
  if (millis() - tLast >= 200) {
    tLast = millis();
    g_vbat = Power::batteryVolts();

    if (!g_sensorsOk) Hmi::status(60, 0, 0);
    else if (g_vbat < Power::VBAT_CRITICAL) Hmi::status(60, 0, 0);
    else if (g_vbat < Power::VBAT_WARN) Hmi::status(60, 30, 0);
    else if (g_armed) Hmi::status(0, 60, 0);
    else Hmi::status(0, 0, 30);

    if (g_armed && g_vbat < Power::VBAT_CRITICAL && !Power::usbPresent()) {
        g_armed = false;
        for (uint8_t i = 0; i < 4; i++) g_throttle[i] = 0;
        Hmi::beep(300);
    }
  }
}

void setup1() {
  Motors::begin();
}

void loop1() {
  static uint32_t next = 0;
  uint32_t now = micros();
  if ((int32_t)(now - next) < 0) return;

  next = now + 1000; // 1 kHz

  ImuSample imu;
  if (!Sensors::readImu(imu)) { Motors::disarm(); return; }

  if (g_armed) {
      for (uint8_t i = 0; i < 4; i++) Motors::set(i, g_throttle[i]);
  } else {
      Motors::setAll(0);
  }
}

#endif 