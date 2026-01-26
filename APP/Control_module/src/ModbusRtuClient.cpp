#include "ModbusRtuClient.h"

#include <errno.h>
#include <unistd.h>

#include <QDebug>
#include <QThread>
static inline bool isModbusProtocolExceptionErrno(int e) {
#ifdef EMBXILFUN
    // 从机异常响应：Illegal function/address/value、Slave failure、Gateway target fail 等
    if (e == EMBXILFUN || e == EMBXILADD || e == EMBXILVAL ||
        e == EMBXSFAIL || e == EMBXGTAR)
        return true;
#endif
    return false;
}
ModbusRtuClient::ModbusRtuClient(QObject* parent)
    : QObject(parent) {
    // open();
}

ModbusRtuClient::~ModbusRtuClient() {
    close();
}
bool ModbusRtuClient::ensureConnected() {
    if (m_connected)
        return true;

    qWarning() << "[MODBUS] ensureConnected -> open";
    return open();
}

bool ModbusRtuClient::open() {
    if (m_connected)
        return true;

    // ⚠️ 只在这里 close，确保 ctx 干净
    close();

    m_ctx = modbus_new_rtu(
        m_dev, m_baud, m_parity, m_dataBits, m_stopBits);
    // modbus_set_debug(m_ctx, TRUE);
    if (!m_ctx) {
        emit ioError("modbus_new_rtu failed");
        return false;
    }

    modbus_set_slave(m_ctx, m_slaveId);

    modbus_set_response_timeout(m_ctx, 0, 300000);
    modbus_set_byte_timeout(m_ctx, 0, 300000);

    if (modbus_connect(m_ctx) != 0) {
        emit ioError(modbus_strerror(errno));
        close();
        return false;
    }
    // 建议：清掉串口残留脏数据，避免第一帧就 CRC/帧错
    modbus_flush(m_ctx);

    // 建议：超时放大一点，先跑通再优化
    modbus_set_response_timeout(m_ctx, 1, 0);  // 1s
    modbus_set_byte_timeout(m_ctx, 1, 0);      // 1s
    qDebug() << "[MODBUS] connected";
    m_connected = true;
    emit connectionStateChanged(true);
    return true;
}

void ModbusRtuClient::close() {
    if (m_ctx) {
        modbus_close(m_ctx);
        modbus_free(m_ctx);
        m_ctx = nullptr;
    }

    if (m_connected) {
        m_connected = false;
        emit connectionStateChanged(false);
    }
}

bool ModbusRtuClient::reconnect() {
    if (m_connected)
        return true;

    qWarning() << "[MODBUS] reconnect...";
    return open();  // 只尝试一次
}

bool ModbusRtuClient::writeMulti(const QVector<RegWriteBlock>& blocks) {
    usleep(1000 * 100);
    QMutexLocker locker(&m_ioMutex);

    if (!ensureConnected())
        return false;

    for (const auto& b : blocks) {
        if (!writeRegisters(b.startAddr, b.values)) {
            m_connected = false;
            return false;
        }
    }
    return true;
}
bool ModbusRtuClient::readMulti(QVector<RegReadBlock>& blocks) {
    usleep(1000 * 100);
    QMutexLocker locker(&m_ioMutex);

    if (!m_connected) {
        if (!reconnect())
            return false;
    }

    bool allOk = true;

    for (auto& b : blocks) {
        b.values.resize(b.count);
        usleep(1000 * 50);
        int rc = modbus_read_registers(
            m_ctx,
            b.startAddr,
            b.count,
            b.values.data());

        if (rc != b.count) {
            qWarning() << "[MODBUS] read failed addr="
                       << b.startAddr
                       << " count=" << b.count
                       << " rc=" << rc;

            // ❗不要立刻 reconnect
            // ❗不要 return false
            allOk = false;

            // 给无效数据填 0，防止上层崩
            b.values.fill(0);
            continue;
        }
    }

    return allOk;
}

bool ModbusRtuClient::writeRegisters(uint16_t addr,
                                     const QVector<uint16_t>& values) {
    usleep(1000 * 100);

    if (!m_ctx) {
        emit ioError("modbus ctx is null");
        m_connected = false;
        return false;
    }

    if (values.isEmpty()) {
        // 你也可以 return false；我这里按“空写无意义”处理为失败
        emit ioError("writeRegisters: values empty");
        return false;
    }

    // 单从站场景也建议每次写前 set_slave，避免你后面扩展多从站踩坑
    modbus_set_slave(m_ctx, m_slaveId);

    const int nb = values.size();
    const int rc = modbus_write_registers(m_ctx, int(addr), nb, values.constData());

    if (rc == nb) {
        return true;
    }

    const int e = errno;
    const char* es = modbus_strerror(e);

    qWarning() << "[MODBUS] write failed"
               << "slave=" << m_slaveId
               << "addr=" << addr
               << "count=" << nb
               << "rc=" << rc
               << "errno=" << e
               << "err=" << es;

    emit ioError(QString::fromLocal8Bit(es));

    // 关键：协议异常不等于断线
    if (!isModbusProtocolExceptionErrno(e)) {
        m_connected = false;
    }
    return false;
}
bool ModbusRtuClient::readRegisters(uint16_t addr, uint16_t count, QVector<uint16_t>& out) {
    usleep(1000 * 100);
    out.resize(count);  // 先把输出数组扩到需要的寄存器数量

    int rc = modbus_read_registers(m_ctx, addr, count, out.data());  // 03 功能码读保持寄存器

    if (rc != count)  // rc != count 说明失败或读不完整
    {
        int e = errno;  // 保存 errno（避免后面函数改变 errno）
        qWarning() << "[MODBUS] read failed"
                   << "slave=" << m_slaveId
                   << "addr=" << addr
                   << "count=" << count
                   << "rc=" << rc
                   << "errno=" << e
                   << "err=" << modbus_strerror(e);  // 打印 libmodbus 的错误描述

        emit ioError(modbus_strerror(e));  // 发信号给上层（可选）

        m_connected = false;  // 标记断开，触发下次重连
        return false;         // 这里先返回 false，不要在底层疯狂 reconnect
    }

    // qDebug() << "[MODBUS] read ok"
    //          << "slave=" << m_slaveId
    //          << "addr=" << addr
    //          << "count=" << count
    //          << "regs=" << out;  // 成功时也把数据打印出来

    return true;  // 成功返回 true
}
bool ModbusRtuClient::readBlock(uint16_t addr, uint16_t count, QVector<uint16_t>& out) {
    usleep(1000 * 100);
    QMutexLocker locker(&m_ioMutex);  // 加锁，保证串口互斥

    if (!m_connected) {    // 如果当前未连接
        if (!reconnect())  // 尝试重连
            return false;  // 重连失败直接返回
    }

    out.resize(count);  // 调整输出数组长度

    int rc = modbus_read_registers(m_ctx, addr, count, out.data());  // 读保持寄存器
    if (rc == int(count)) {                                          // 如果读到的数量等于期望数量
        return true;                                                 // 成功返回
    }

    int err = errno;                            // 保存 errno（避免后面被覆盖）
    const char* errStr = modbus_strerror(err);  // 获取错误描述字符串

    qWarning() << "[MODBUS] read failed slave=" << m_slaveId  // 打印失败日志
               << "addr=" << addr
               << "count=" << count
               << "rc=" << rc
               << "errno=" << err
               << "err=" << errStr;

    emit ioError(errStr);  // 发出 IO 错误信号（UI可提示）

    // ============================================================
    // ★ 关键修复：协议异常（Illegal xxx）不等于断线，绝不重连
    // ============================================================
    const QString msg = QString::fromLatin1(errStr);     // 转 QString 便于判断
    if (msg.contains("Illegal", Qt::CaseInsensitive)) {  // 如果是 Illegal data value/address/function
        // 这里不要动 m_connected，不要 reconnect
        return false;  // 直接返回失败，让上层决定怎么处理
    }

    // ============================================================
    // ★ 真正链路错误：才认为断线并尝试重连（但不重发本次请求）
    // ============================================================
    m_connected = false;  // 标记掉线
    reconnect();          // 尝试恢复连接（仅恢复链路）
    return false;         // 返回失败
}

// bool ModbusRtuClient::readBlock(uint16_t addr, uint16_t count, QVector<uint16_t>& out) {
//     QMutexLocker locker(&m_ioMutex);

//     if (!m_connected && !reconnect())
//         return false;

//     out.resize(count);
//     int rc = modbus_read_registers(m_ctx, addr, count, out.data());
//     if (rc != count) {
//         qWarning() << "[MODBUS] read failed slave=" << m_slaveId
//                    << "addr=" << addr << "count=" << count
//                    << "rc=" << rc << "errno=" << errno
//                    << "err=" << modbus_strerror(errno);
//         emit ioError(modbus_strerror(errno));
//         m_connected = false;
//         return reconnect();  // 这里按你策略：失败则触发重连
//     }

//     // qDebug() << "[MODBUS] read ok slave=" << m_slaveId
//     //          << "addr=" << addr << "count=" << count
//     //          << "regs=" << out;
//     return true;
// }
void ModbusRtuClient::postWriteRegisters(uint16_t addr,
                                         const QVector<uint16_t>& regs) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, addr, regs]() { postWriteRegisters(addr, regs); },
            Qt::QueuedConnection);
        return;
    }
    usleep(1000 * 100);
    // 必须串口互斥：避免与 writeMulti/readMulti/readBlock 并发
    QMutexLocker locker(&m_ioMutex);

    if (!ensureConnected()) {
        qWarning() << "[MODBUS] writeRegisters ensureConnected failed";
        return;
    }

    const bool ok = writeRegisters(addr, regs);
    qDebug() << "[MODBUS] writeRegisters addr=" << addr
             << "count=" << regs.size()
             << "slave=" << m_slaveId
             << "ok=" << ok;
}
