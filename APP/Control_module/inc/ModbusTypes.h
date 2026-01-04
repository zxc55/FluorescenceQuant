#pragma once
#include <QVector>
#include <cstdint>

// 读块：起始地址 + 数量 + 读回值
struct RegReadBlock {
    uint16_t startAddr;        // 起始寄存器地址
    uint16_t count;            // 读取寄存器数量
    QVector<uint16_t> values;  // 读回数据（长度=count）
};

// 写块：起始地址 + 写入值
struct RegWriteBlock {
    uint16_t startAddr;        // 起始寄存器地址
    QVector<uint16_t> values;  // 写入数据（长度=寄存器数量）
};
// 地址 5：微动开关状态
union LimitSwitchReg {
    uint16_t raw;
    struct {
        uint16_t slide_home : 1;   // bit0 滑台原点
        uint16_t slide_limit : 1;  // bit1 滑台限位
        uint16_t incub_pos1 : 1;   // bit2 孵育位1
        uint16_t incub_pos2 : 1;   // bit3 孵育位2
        uint16_t incub_pos3 : 1;   // bit4 孵育位3
        uint16_t incub_pos4 : 1;   // bit5 孵育位4
        uint16_t incub_pos5 : 1;   // bit6 孵育位5
        uint16_t incub_pos6 : 1;   // bit7 孵育位6
        uint16_t reserved : 8;
    } bit;
};

// 地址 6：孵育时间到（位触发）
union IncubFinishReg {
    uint16_t raw;
    struct {
        uint16_t incub1_done : 1;  // bit0
        uint16_t incub2_done : 1;  // bit1
        uint16_t incub3_done : 1;  // bit2
        uint16_t incub4_done : 1;  // bit3
        uint16_t incub5_done : 1;  // bit4
        uint16_t incub6_done : 1;  // bit5
        uint16_t reserved : 10;
    } bit;
};

// 地址 7：孵育状态
enum class IncubState : uint16_t {
    Heating = 0,
    Stable = 1,
};
