#include "sensors.h"
#include "pins.h"
#include <SPI.h>
#include <math.h>

static const SPISettings kSensorSpi(8000000, MSBFIRST, SPI_MODE3); // 8 - 10 MHz

// LSM6DSR
static constexpr uint8_t LSM_WHO_AM_I = 0x0F;
static constexpr uint8_t LSM_CTRL1_XL = 0x10;
static constexpr uint8_t LSM_CTRL2_G = 0x11;
static constexpr uint8_t LSM_CTRL3_C = 0x12;
static constexpr uint8_t LSM_CTRL6_C = 0x15;
static constexpr uint8_t LSM_CTRL9_XL = 0x18;
static constexpr uint8_t LSM_STATUS = 0x1E;
static constexpr uint8_t LSM_OUTX_L_G  = 0x22; 

// BMP581 
static constexpr uint8_t BMP_CHIP_ID = 0x01;
static constexpr uint8_t BMP_INT_SOURCE = 0x15;
static constexpr uint8_t BMP_TEMP_DATA = 0x1D;
static constexpr uint8_t BMP_STATUS = 0x28;
static constexpr uint8_t BMP_DSP_IIR = 0x31;
static constexpr uint8_t BMP_OSR_CFG = 0x36;
static constexpr uint8_t BMP_ODR_CFG = 0x37;
static constexpr uint8_t BMP_CMD = 0x7E;

static constexpr float ACC_LSB = 0.244f / 1000.0f;
static constexpr float GYRO_LSB = 70.0f  / 1000.0f;

static bool s_imuOk = false;
static bool s_baroOk = false;

static void writeReg(uint8_t cs, uint8_t reg, uint8_t val) {
    SPI1.beginTransaction(kSensorSpi);
    digitalWrite(cs, LOW);
    SPI1.transfer(reg & 0x7F); 
    SPI1.transfer(val);
    digitalWrite(cs, HIGH);
    SPI1.endTransaction();
}

static void readRegs(uint8_t cs, uint8_t reg, uint8_t* buf, size_t len) {
    SPI1.beginTransaction(kSensorSpi);
    digitalWrite(cs, LOW);
    SPI1.transfer(reg | 0x80);
    for (size_t i = 0; i < len; i++) buf[i] = SPI1.transfer(0x00);
    digitalWrite(cs, HIGH);
    SPI1.endTransaction();
}

static uint8_t readReg(uint8_t cs, uint8_t reg) {
    uint8_t v = 0;
    readRegs(cs, reg, &v, 1);
    return v;
}

static inline int16_t le16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[1] << 8 | p[0]);
}

bool Sensors::imuOk()  { return s_imuOk; }
bool Sensors::baroOk() { return s_baroOk; }

bool Sensors::begin() {
    pinMode(PIN_IMU_CS, OUTPUT);
    pinMode(PIN_BARO_CS, OUTPUT);
    digitalWrite(PIN_IMU_CS, HIGH);
    digitalWrite(PIN_BARO_CS, HIGH);

    pinMode(PIN_IMU_INT1, INPUT);
    pinMode(PIN_IMU_INT2, INPUT);
    pinMode(PIN_BARO_INT, INPUT);

    SPI1.setSCK(PIN_SENS_SCK);
    SPI1.setTX(PIN_SENS_MOSI);
    SPI1.setRX(PIN_SENS_MISO);
    SPI1.begin();

    delay(20);

    writeReg(PIN_IMU_CS, LSM_CTRL3_C, 0x01); 
    delay(20);

    if (readReg(PIN_IMU_CS, LSM_WHO_AM_I) == 0x6B) {
        writeReg(PIN_IMU_CS, LSM_CTRL3_C, 0x44);
        writeReg(PIN_IMU_CS, LSM_CTRL9_XL, 0xE2);
        writeReg(PIN_IMU_CS, LSM_CTRL1_XL, 0x8C);
        writeReg(PIN_IMU_CS, LSM_CTRL2_G, 0x8C);
        writeReg(PIN_IMU_CS, LSM_CTRL6_C, 0x00);
        s_imuOk = true;
    }

    writeReg(PIN_BARO_CS, BMP_CMD, 0xB6); 
    delay(10);

    if (readReg(PIN_BARO_CS, BMP_CHIP_ID) == 0x50) {
        writeReg(PIN_BARO_CS, BMP_OSR_CFG, 0x4B);
        writeReg(PIN_BARO_CS, BMP_DSP_IIR, 0x09); 
        writeReg(PIN_BARO_CS, BMP_ODR_CFG, 0x59);
        writeReg(PIN_BARO_CS, BMP_INT_SOURCE, 0x01);
        delay(20);
        s_baroOk = true;
    }

    return s_imuOk && s_baroOk;
}

bool Sensors::readImu(ImuSample& s) {
    if (!s_imuOk) return false;

    uint8_t b[12];
    readRegs(PIN_IMU_CS, LSM_OUTX_L_G, b, sizeof(b));

    s.gx = le16(&b[0]) * GYRO_LSB;
    s.gy = le16(&b[2]) * GYRO_LSB;
    s.gz = le16(&b[4]) * GYRO_LSB;
    s.ax = le16(&b[6]) * ACC_LSB;
    s.ay = le16(&b[8]) * ACC_LSB;
    s.az = le16(&b[10]) * ACC_LSB;
    return true;
}

bool Sensors::readBaro(BaroSample& s) {
    if (!s_baroOk) return false;

    uint8_t b[6];
    readRegs(PIN_BARO_CS, BMP_TEMP_DATA, b, sizeof(b));

    int32_t traw = (int32_t)((uint32_t)b[2] << 16 | (uint32_t)b[1] << 8  | b[0]);
    if (traw & 0x800000) traw |= (int32_t)0xFF000000; 

    uint32_t praw = (uint32_t)b[5] << 16 | (uint32_t)b[4] << 8  | b[3];

    s.temperature_c = traw / 65536.0f;
    s.pressure_pa   = praw / 64.0f;

    s.altitude_m = 44330.0f * (1.0f - powf(s.pressure_pa / 101325.0f, 0.1902949f));
    return true;
}