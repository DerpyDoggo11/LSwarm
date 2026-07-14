#pragma once
#include <Arduino.h>

struct ImuSample{
    float ax, ay, az;
    float gx, gy, gz;
};

struct BaroSample {
    float pressure_pa;
    float temperature_c;
    float altitude_m;
};

namespace Sensors {
    bool begin();
    bool imuOk();
    bool baroOk();
    bool readImu(ImuSample& s);
    bool readBaro(BaroSample& s);
}