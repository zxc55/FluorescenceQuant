#pragma once
#include <QVariant>
#include <cstdint>

/* ================= 业务功能码 ================= */
enum class DevFunc : uint16_t {
    ReadCurrentTemp,
    SetTargetTemp,

    ReadLimitSwitch,
    WriteIncubFinishMask,
    ReadIncubState,
    incubatetimeout,

    MotorHome,
    MotorStart,
    SetMotorSpeed,
    ReadMotorState,
    ReadMotorSteps,
    WriteMotorSteps,
    SetTactualemp,
    Settint_time,
    EnableFluorescence
};

/* ================= 数据类型 ================= */
enum class ValueType {
    U16,
    FLOAT,
    BITMASK,
    NONE
};

/* ================= 功能描述表 ================= */
struct FuncDesc {
    DevFunc func;
    uint16_t startAddr;
    uint16_t regCount;
    ValueType type;

    bool canRead;   // 能不能被 readBlock
    bool canWrite;  // 能不能写
    bool canPoll;   // 能不能加入 pollOnce
};

/* ================= 执行参数 ================= */
struct ExecItem {
    DevFunc func;    // 功能码
    QVariant value;  // 实际数据
};
extern const FuncDesc g_funcTable[];
extern const int g_funcTableSize;
/* 查表接口 */
const FuncDesc* findFuncDesc(DevFunc f);
