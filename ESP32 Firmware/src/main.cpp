#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "pins.h"
#include "uwb.h"

// ===========================================================================
// LSwarm drone radio brain (external ESP32-S3 Super Mini)
//
//   RP2350 (flight controller)  <--UART-->  this ESP32-S3
//   this ESP32-S3               <--WiFi-->  Raspberry Pi Zero 2 W (AP)
//   this ESP32-S3               <-ESP-NOW-> peer drones
//   this ESP32-S3 + DW3000      <--UWB--->  LS Anchors  (ranging -> position)
//
// Position is solved on the Pi: the drone sends anchor ranges up, the Pi
// trilaterates and sends "P,x,y,z,q" back. That keeps this firmware small and
// the math testable in Python. See SETUP_EXTERNAL_ESP32.txt.
// ===========================================================================

// ---- per-drone / site config (EDIT THESE) --------------------------------
static constexpr uint8_t  DRONE_ID   = 1;
static const char*        WIFI_SSID  = "LSwarm";
static const char*        WIFI_PASS  = "swarmnet";
static const char*        PI_IP      = "192.168.4.1";
static constexpr uint16_t PI_PORT    = 9000;   // Pi listens here
static constexpr uint16_t LOCAL_PORT = 9001;   // we listen here for replies

// Anchor UWB addresses to range against. Must match the anchors' `id`.
static const uint8_t ANCHORS[] = {1, 2, 3, 4};
static constexpr size_t N_ANCHORS = sizeof(ANCHORS);

// Broadcast MAC for ESP-NOW neighbour sharing.
static uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
// --------------------------------------------------------------------------

static WiFiUDP udp;
static float g_x=0, g_y=0, g_z=0; static int g_q=0;   // last known position
static float g_vbat=0; static int g_armed=0;          // from RP2350 telemetry

static void toRp(const char* s) { Serial1.println(s); }

// ---- ESP-NOW: share our position, hear neighbours ------------------------
static void onEspNowRecv(const esp_now_recv_info_t*, const uint8_t* data, int len) {
    // "N,<id>,<x>,<y>,<z>" from a peer -> hand to RP2350 as a neighbour hint.
    if (len < 2 || data[0] != 'N') return;
    char buf[64]; int n = len < 63 ? len : 63;
    memcpy(buf, data, n); buf[n] = 0;
    Serial1.println(buf);            // RP2350 may use this for spacing
}

static void espNowShare() {
    char msg[64];
    int n = snprintf(msg, sizeof(msg), "N,%u,%.2f,%.2f,%.2f", DRONE_ID, g_x, g_y, g_z);
    esp_now_send(BCAST, (uint8_t*)msg, n);
}

// ---- WiFi + UDP ----------------------------------------------------------
static void wifiConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(100);
    udp.begin(LOCAL_PORT);
}

static void handlePiPacket(char* p) {
    // "P,x,y,z,q"  -> position solution   |  "C,..." -> command for RP2350
    if (p[0] == 'P') {
        sscanf(p + 2, "%f,%f,%f,%d", &g_x, &g_y, &g_z, &g_q);
        Serial1.println(p);                  // forward position to flight ctrl
        espNowShare();
    } else if (p[0] == 'C') {
        Serial1.println(p);                  // forward command verbatim
    }
}

static void pumpUdp() {
    int sz = udp.parsePacket();
    if (sz <= 0) return;
    char buf[128];
    int n = udp.read(buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = 0;
    handlePiPacket(buf);
}

// ---- RP2350 link: read its telemetry lines -------------------------------
static void pumpRp() {
    static char line[96]; static uint8_t n = 0;
    while (Serial1.available()) {
        char c = (char)Serial1.read();
        if (c == '\n' || c == '\r') {
            if (n) {
                line[n] = 0;
                if (line[0] == 'T')          // "T,vbat,armed,usb"
                    sscanf(line + 2, "%f,%d", &g_vbat, &g_armed);
                n = 0;
            }
        } else if (n < sizeof(line) - 1) line[n++] = c;
    }
}

// ---- UWB ranging round ---------------------------------------------------
static void rangeAndReport() {
    char msg[160];
    int off = snprintf(msg, sizeof(msg), "R,%u", DRONE_ID);
    bool any = false;
    for (size_t i = 0; i < N_ANCHORS; i++) {
        float d;
        if (Uwb::rangeTo(ANCHORS[i], d)) {
            off += snprintf(msg + off, sizeof(msg) - off, ",%u,%.3f", ANCHORS[i], d);
            any = true;
        }
    }
    if (any && WiFi.status() == WL_CONNECTED) {
        udp.beginPacket(PI_IP, PI_PORT);
        udp.write((uint8_t*)msg, off);
        udp.endPacket();
    }
}

static void reportHealth() {
    if (WiFi.status() != WL_CONNECTED) return;
    char msg[64];
    int n = snprintf(msg, sizeof(msg), "D,%u,%.2f,%d,%d",
                     DRONE_ID, g_vbat, g_armed, Uwb::ok() ? 1 : 0);
    udp.beginPacket(PI_IP, PI_PORT);
    udp.write((uint8_t*)msg, n);
    udp.endPacket();
}

void setup() {
    Serial.begin(115200);                                   // USB debug
    Serial1.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
    delay(50);
    toRp("S,esp32-s3 boot");

    wifiConnect();

    // ESP-NOW must share the AP's channel to coexist with WiFi-STA.
    esp_wifi_set_channel(WiFi.channel(), WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(onEspNowRecv);
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, BCAST, 6);
        peer.channel = WiFi.channel();
        esp_now_add_peer(&peer);
    }

    if (Uwb::begin()) toRp("S,UWB ready");
    else              toRp("S,UWB NOT DETECTED (ranging off)");

    toRp(WiFi.status() == WL_CONNECTED ? "S,wifi ok" : "S,wifi DOWN");
}

void loop() {
    pumpRp();
    pumpUdp();

    static uint32_t tRange = 0;
    if (millis() - tRange >= 100) { tRange = millis(); rangeAndReport(); }   // 10 Hz

    static uint32_t tHealth = 0;
    if (millis() - tHealth >= 1000) { tHealth = millis(); reportHealth(); }

    if (WiFi.status() != WL_CONNECTED) {
        static uint32_t tRetry = 0;
        if (millis() - tRetry >= 3000) { tRetry = millis(); WiFi.reconnect(); }
    }
}
