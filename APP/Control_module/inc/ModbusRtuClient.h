#pragma once
#include <modbus/modbus.h>

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>
#include <cstdint>

#include "ModbusTypes.h"

/**
 * @brief Modbus RTU 客户端
 *
 * 职责：
 *  - 管理 libmodbus RTU 连接
 *  - 执行寄存器读写
 *  - 自动重连
 *  - 线程安全（外部保证在同一工作线程调用）
 *
 * 注意：
 *  - 这是【通信执行器】，不是线程
 *  - 必须由 DeviceManager moveToThread()
 */
class ModbusRtuClient : public QObject {
    Q_OBJECT
public:
    explicit ModbusRtuClient(QObject* parent = nullptr);
    ~ModbusRtuClient();

    // ===== 写：使用 RegWriteBlock =====
    bool writeMulti(const QVector<RegWriteBlock>& blocks);

    // ===== 读：使用 RegReadBlock（需要写回 values）=====
    bool readMulti(QVector<RegReadBlock>& blocks);
    bool readBlock(uint16_t addr, uint16_t count, QVector<uint16_t>& out);
    Q_INVOKABLE void postWriteRegisters(uint16_t addr, const QVector<uint16_t>& regs);
signals:
    void ioError(const QString& msg);             ///< 通信错误
    void connectionStateChanged(bool connected);  ///< 连接状态变化

private:
    /* ===== 连接管理 ===== */
    bool ensureConnected();
    bool open();
    void close();
    bool reconnect();

    /* ===== 单次 IO ===== */
    bool writeRegisters(uint16_t addr,
                        const QVector<uint16_t>& values);

    bool readRegisters(uint16_t addr,
                       uint16_t count,
                       QVector<uint16_t>& out);

private:
    QMutex m_ioMutex;
    modbus_t* m_ctx = nullptr;
    bool m_connected = false;

    /* ===== 串口参数 ===== */
    const char* m_dev = "/dev/ttyS4";
    int m_baud = 115200;
    char m_parity = 'N';
    int m_dataBits = 8;
    int m_stopBits = 1;
    int m_slaveId = 2;
};
