#include <Arduino.h>
#include "pins.h"
#include "sensors.h"
#include "motors.h"
#include "hmi.h"
#include "esplink.h"
#include "selftest.h"

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

// Last position solution pushed up from the ESP32-S3 ("P,x,y,z,q").
static volatile float g_px = 0, g_py = 0, g_pz = 0; static volatile int g_pq = 0;

// Bench motor test: -1 = normal flight-control path, 0..3 = drive that one motor
// at g_motorTestPwm and hold the others off. Applied on core1 so all PWM writes
// stay on a single core (no cross-core race with analogWrite).
static volatile int8_t   g_motorTest    = -1;
static volatile uint16_t g_motorTestPwm = 0;

// When true, the periodic status-LED state machine is suspended so a manual
// 'status <r> <g> <b>' (or the self-test result colour) stays on screen.
static volatile bool     g_statusOverride = false;

static constexpr uint16_t MTEST_PWM = 220;   // ~22% - enough to spin a prop-less motor
static constexpr uint32_t MTEST_MS  = 700;   // per-motor spin duration

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
        Serial.println("arm | disarm | t <0-3> <0-1000> | stat");
        Serial.println("selftest | mtest | m <0-3> <0-1000> | beep | status [<r> <g> <b>] | led <r> <g> <b>");
        Serial.println("esp_reset | esp_boot | esp_bridge");
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
      g_motorTest = -1;
      for (uint8_t i = 0; i < 4; i++) g_throttle[i] = 0;
      Serial.println("disarmed");
  } else if (!strcmp(cmd, "selftest")) {
      bool ok = SelfTest::run();
      g_statusOverride = true;                 // hold the result colour
      Hmi::status(ok ? 0 : 60, ok ? 60 : 0, 0);
      Serial.println("(status LED held; type 'status' to resume automatic)");
  } else if (!strcmp(cmd, "mtest")) {
      Serial.println("MOTOR TEST: remove propellers! spinning each motor ~0.7s in order...");
      for (uint8_t i = 0; i < 4; i++) {
          Serial.printf("  motor %u\n", i);
          g_motorTestPwm = MTEST_PWM;
          g_motorTest = (int8_t)i;
          delay(MTEST_MS);
          g_motorTest = -1;                    // core1 zeroes all outputs
          delay(400);
      }
      Serial.println("motor test complete");
  } else if (!strncmp(cmd, "m ", 2)) {
      int idx, val;
      if (sscanf(cmd + 2, "%d %d", &idx, &val) == 2 && idx >= 0 && idx < 4) {
          g_motorTestPwm = (uint16_t)constrain(val, 0, Motors::PWM_MAX);
          g_motorTest = (g_motorTestPwm == 0) ? -1 : (int8_t)idx;
          Serial.printf("bench motor %d = %d (props off!)\n", idx, g_motorTestPwm);
      } else {
          Serial.println("usage: m <0-3> <0-1000>");
      }
  } else if (!strcmp(cmd, "beep")) {
      Hmi::beep(120);
      Serial.println("beep");
  } else if (!strncmp(cmd, "status", 6)) {
      int r, g, b;
      if (sscanf(cmd + 6, "%d %d %d", &r, &g, &b) == 3) {
          g_statusOverride = true;
          Hmi::status(r, g, b);
          Serial.printf("status %d %d %d (override; 'status' alone resumes auto)\n", r, g, b);
      } else {
          g_statusOverride = false;
          Serial.println("status: automatic");
      }
  } else if (!strncmp(cmd, "led", 3)) {
      int r, g, b;
      if (sscanf(cmd + 3, "%d %d %d", &r, &g, &b) == 3) {
          Hmi::array(r, g, b);
          Serial.printf("led %d %d %d\n", r, g, b);
      } else {
          Serial.println("usage: led <r> <g> <b>");
      }
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

// Lines coming up from the ESP32-S3 over Serial1.
//   S,<text>       -> log to USB console
//   P,x,y,z,q      -> position solution (store for flight/telemetry)
//   N,id,x,y,z     -> neighbour position (ESP-NOW relay)  [ignored for now]
//   C,<cmd>...     -> command from the Pi, mapped onto the local console verbs
static void handleEspLine(char* s) {
  if (s[0] == 'P') {
    float x, y, z; int q;
    if (sscanf(s + 2, "%f,%f,%f,%d", &x, &y, &z, &q) == 4) {
      g_px = x; g_py = y; g_pz = z; g_pq = q;
    }
  } else if (s[0] == 'C') {
    // Translate "C,verb,a,b,c" into the existing console command "verb a b c".
    char cmd[48]; size_t j = 0;
    for (char* p = s + 2; *p && j < sizeof(cmd) - 1; p++)
      cmd[j++] = (*p == ',') ? ' ' : *p;
    cmd[j] = 0;
    handleCommand(cmd);
  } else {
    Serial.println(s);            // S,... and anything else -> console
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

  static char eline[96];
  static uint8_t en = 0;
  while (Serial1.available()) {
      char c = (char)Serial1.read();
      if (c == '\n' || c == '\r') {
          if (en) { eline[en] = 0; handleEspLine(eline); en = 0; }
      } else if (en < sizeof(eline) - 1) {
          eline[en++] = c;
      }
  }

  static uint32_t tLast = 0;
  if (millis() - tLast >= 200) {
    tLast = millis();
    g_vbat = Power::batteryVolts();

    // Push telemetry up to the ESP32-S3 -> Pi ("T,vbat,armed,usb").
    Serial1.printf("T,%.2f,%d,%d\n", g_vbat, g_armed ? 1 : 0, Power::usbPresent() ? 1 : 0);

    if (!g_statusOverride) {
        if (!g_sensorsOk) Hmi::status(60, 0, 0);
        else if (g_vbat < Power::VBAT_CRITICAL) Hmi::status(60, 0, 0);
        else if (g_vbat < Power::VBAT_WARN) Hmi::status(60, 30, 0);
        else if (g_armed) Hmi::status(0, 60, 0);
        else Hmi::status(0, 0, 30);
    }

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

  // Bench motor test overrides the flight-control path ('mtest' / 'm' commands).
  if (g_motorTest >= 0) {
      for (uint8_t i = 0; i < 4; i++)
          Motors::set(i, (i == g_motorTest) ? g_motorTestPwm : 0);
      return;
  }

  ImuSample imu;
  if (!Sensors::readImu(imu)) { Motors::disarm(); return; }

  if (g_armed) {
      for (uint8_t i = 0; i < 4; i++) Motors::set(i, g_throttle[i]);
  } else {
      Motors::setAll(0);
  }
}

#endif 