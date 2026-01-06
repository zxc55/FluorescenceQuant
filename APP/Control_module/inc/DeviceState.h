#ifndef DEVICESTATE_H_
#define DEVICESTATE_H_

#include <stdint.h>

#include <QVector>

#include "ModbusTypes.h"
inline float regsToFloat_CDAB(const QVector<uint16_t>& r) {
    if (r.size() < 2)
        return 0.0f;

    union {
        uint32_t u;
        float f;
    } v;

    v.u = (uint32_t(r[1]) << 16) | r[0];
    return v.f;
}

inline void floatToRegs_CDAB(float f, uint16_t& cd, uint16_t& ab) {
    union {
        uint32_t u;
        float f;
    } v;

    v.f = f;
    ab = uint16_t(v.u & 0xFFFF);
    cd = uint16_t((v.u >> 16) & 0xFFFF);
}
/* ================= 设备完整状态 ================= */

struct DeviceStatus {
    /* ===== 温度 ===== */
    float currentTemp;  // addr 1–2
    float targetTemp;   // addr 3–4（读回）

    /* ===== 孵育 ===== */
    LimitSwitchReg limitSwitch;  // addr 5
    IncubFinishReg incubFinish;  // addr 6
    IncubState incubState;       // addr 7
    uint16_t fuyutimeout;        // addr 8
    /* ===== 电机 ===== */
    uint16_t motorState;  // addr 23
    uint16_t motorSteps;  // addr 24

    /* ===== 扩展 ===== */
    uint16_t motorSpeed;    // addr 22（读回）
    uint16_t fluorescence;  // addr 25
};
#endif  // DEVICESTATE_H_