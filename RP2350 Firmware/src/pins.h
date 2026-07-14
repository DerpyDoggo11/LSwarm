#pragma once
#include <cstdint>

// ESP32
static constexpr uint8_t PIN_ESP_TX = 0;
static constexpr uint8_t PIN_ESP_RX = 1;  
static constexpr uint8_t PIN_ESP_EN = 2; 
static constexpr uint8_t PIN_ESP_BOOT = 3;

// Sensors
static constexpr uint8_t PIN_SENS_SCK = 10;
static constexpr uint8_t PIN_SENS_MOSI = 11;
static constexpr uint8_t PIN_SENS_MISO = 8;
static constexpr uint8_t PIN_IMU_CS = 7;
static constexpr uint8_t PIN_BARO_CS = 9;

static constexpr uint8_t PIN_BARO_INT = 4;
static constexpr uint8_t PIN_IMU_INT1 = 5;
static constexpr uint8_t PIN_IMU_INT2 = 6;

// Motors
static constexpr uint8_t PIN_MOTOR[4] = {18, 19, 20, 21};

// MISC
static constexpr uint8_t PIN_BUZZER = 22;
static constexpr uint8_t PIN_STATUS_LED = 23; 
static constexpr uint8_t PIN_LED_ARRAY = 25; 
static constexpr uint8_t LED_ARRAY_COUNT = 4;

// Power
static constexpr uint8_t PIN_VBUS_SENSE = 24; 
static constexpr uint8_t PIN_VBAT_ADC  = 26; 
static constexpr float VBAT_DIVIDER = 2.0f;
static constexpr float ADC_VREF = 3.3f;