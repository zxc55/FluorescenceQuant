#include "ModbusRtuClient.h"

#include <errno.h>

#include <QDebug>
#include <QThread>

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
    QMutexLocker locker(&m_ioMutex);

    if (!m_connected) {
        if (!reconnect())
            return false;
    }

    bool allOk = true;

    for (auto& b : blocks) {
        b.values.resize(b.count);

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
    int rc = modbus_write_registers(
        m_ctx,
        addr,
        values.size(),
        values.data());

    if (rc != values.size()) {
        emit ioError(modbus_strerror(errno));
        m_connected = false;
        return false;
    }
    return true;
}

bool ModbusRtuClient::readRegisters(uint16_t addr, uint16_t count, QVector<uint16_t>& out) {
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

    qDebug() << "[MODBUS] read ok"
             << "slave=" << m_slaveId
             << "addr=" << addr
             << "count=" << count
             << "regs=" << out;  // 成功时也把数据打印出来

    return true;  // 成功返回 true
}
bool ModbusRtuClient::readBlock(uint16_t addr, uint16_t count, QVector<uint16_t>& out) {
    QMutexLocker locker(&m_ioMutex);

    if (!m_connected && !reconnect())
        return false;

    out.resize(count);
    int rc = modbus_read_registers(m_ctx, addr, count, out.data());
    if (rc != count) {
        qWarning() << "[MODBUS] read failed slave=" << m_slaveId
                   << "addr=" << addr << "count=" << count
                   << "rc=" << rc << "errno=" << errno
                   << "err=" << modbus_strerror(errno);
        emit ioError(modbus_strerror(errno));
        m_connected = false;
        return reconnect();  // 这里按你策略：失败则触发重连
    }

    qDebug() << "[MODBUS] read ok slave=" << m_slaveId
             << "addr=" << addr << "count=" << count
             << "regs=" << out;
    return true;
}
void ModbusRtuClient::postWriteRegisters(uint16_t addr,
                                         const QVector<uint16_t>& regs) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, addr, regs]() { postWriteRegisters(addr, regs); },
            Qt::QueuedConnection);
        return;
    }

    // ★ 真正的同步写（私有）
    bool ok = writeRegisters(addr, regs);
    qDebug() << "[MODBUS] writeRegisters addr=" << addr
             << "count=" << regs.size()
             << "ok=" << ok;
}