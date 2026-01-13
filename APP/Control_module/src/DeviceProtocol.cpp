#include "DeviceProtocol.h"

/* ================= 协议配置表 ================= */
const FuncDesc g_funcTable[] = {

    /* ================= 温度相关 ================= */
    // 当前温度：状态寄存器（只读）
    {DevFunc::ReadCurrentTemp, 1, 2, ValueType::FLOAT,
     true, false, false},  // canRead, canWrite, canPoll

    // 目标温度：参数寄存器（可读可写）
    {DevFunc::SetTargetTemp, 3, 2, ValueType::FLOAT,
     true, true, false},  // ⚠️ 不参与 poll

    /* ================= 孵育状态 ================= */
    // 限位开关：状态寄存器
    {DevFunc::ReadLimitSwitch, 5, 1, ValueType::U16,
     true, false, true},

    // 孵育完成掩码：参数寄存器（读 + 写）
    {DevFunc::WriteIncubFinishMask, 6, 1, ValueType::BITMASK,
     true, true, false},

    // 孵育状态：状态寄存器
    {DevFunc::ReadIncubState, 7, 1, ValueType::U16,
     true, false, true},
    // 孵育时间到：状态寄存器
    {DevFunc::incubatetimeout, 8, 1, ValueType::U16,
     true, false, true},
    /* ================= 电机控制（命令） ================= */
    // 回原点：命令寄存器（写即触发）
    {DevFunc::MotorHome, 20, 1, ValueType::NONE,
     false, true, false},  // ❌ 不可读 ❌ 不可 poll

    // 开始检测：命令寄存器
    {DevFunc::MotorStart, 21, 1, ValueType::NONE,
     false, true, false},

    /* ================= 电机参数 ================= */
    // 电机速度：参数寄存器（读写）
    {DevFunc::SetMotorSpeed, 22, 1, ValueType::U16,
     true, true, false},

    /* ================= 电机状态 ================= */
    // 电机状态：状态寄存器
    {DevFunc::ReadMotorState, 23, 1, ValueType::U16,
     false, false, false},

    // 电机步数：状态 + 参数（取决于固件）
    {DevFunc::ReadMotorSteps, 24, 1, ValueType::U16,
     true, false, true},

    {DevFunc::WriteMotorSteps, 24, 1, ValueType::U16,
     false, true, false},

    /* ================= 光学模块 ================= */
    // 荧光开关：控制寄存器（写触发）
    {DevFunc::EnableFluorescence, 25, 1, ValueType::U16,
     false, true, false},
};
const int g_funcTableSize =
    sizeof(g_funcTable) / sizeof(g_funcTable[0]);
const FuncDesc* findFuncDesc(DevFunc f) {
    for (const auto& d : g_funcTable) {
        if (d.func == f)
            return &d;
    }
    return nullptr;
}
