#include "DeviceService.h"

#include <QDebug>
#include <QtGlobal>
#include <algorithm>

#include "DeviceState.h"

static float regsToFloat_BE(const QVector<uint16_t>& v) {
    if (v.size() < 2)
        return 0.0f;

    union {
        uint32_t u;
        float f;
    } u{};

    // v[0] 高16位, v[1] 低16位
    u.u = (uint32_t(v[0]) << 16) | uint32_t(v[1]);
    return u.f;
}

DeviceService::DeviceService(ModbusRtuClient* worker)
    : QObject(nullptr), m_worker(worker) {
    // 注意：这里不要 startTimer / 不要 new QTimer
    // 因为构造函数可能在主线程执行，后面会 moveToThread
}

void DeviceService::onThreadStarted() {
    qDebug() << "[DeviceService] onThreadStarted, thread =" << QThread::currentThread();

    // ★ 在工作线程创建 QTimer，并把它 parent 到 this（同线程安全）
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setTimerType(Qt::CoarseTimer);
        connect(m_pollTimer, &QTimer::timeout, this, &DeviceService::pollOnce, Qt::QueuedConnection);
    }
}

void DeviceService::startPolling(int intervalMs) {
    // ★ 关键：如果调用线程不是对象线程，转到对象线程执行
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "startPolling",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, intervalMs));
        return;
    }

    if (!m_pollTimer) {
        qWarning() << "[DeviceService] startPolling but timer not inited yet";
        return;
    }

    qDebug() << "[DeviceService] startPolling interval =" << intervalMs;
    m_pollTimer->start(intervalMs);
}

void DeviceService::stopPolling() {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "stopPolling", Qt::QueuedConnection);
        return;
    }

    if (m_pollTimer && m_pollTimer->isActive()) {
        m_pollTimer->stop();
        qDebug() << "[DeviceService] polling stopped";
    }
}

void DeviceService::exec(const QVector<ExecItem>& items) {
    /*================ ① 确保在 DeviceService 线程 =================*/
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, items]() { exec(items); },
            Qt::QueuedConnection);
        return;
    }

    qDebug() << "[DeviceService] exec items =" << items.size();

    /*================ ② 逐条处理 =================*/
    for (const ExecItem& it : items) {
        switch (it.func) {
        /*================ 写目标温度 =================*/
        case DevFunc::SetTargetTemp: {
            float temp = it.value.toFloat();

            uint16_t cd, ab;
            floatToRegs_CDAB(temp, cd, ab);

            QVector<uint16_t> regs;
            regs << cd << ab;

            constexpr uint16_t TARGET_TEMP_ADDR = 0x0003;  // ⚠️按你设备实际改

            m_worker->postWriteRegisters(TARGET_TEMP_ADDR, regs);
            qDebug() << "[DeviceService] post write target temp =" << temp;
            break;
        }

        /*================ 其他功能 =================*/
        default:
            qWarning() << "[DeviceService] exec unsupported func:"
                       << int(it.func);
            break;
        }
    }
}
static void dumpDeviceStatus(const DeviceStatus& s) {
    qDebug().noquote()
        << "\n========== DeviceStatus ==========\n"
        << "Temperature:\n"
        << "  currentTemp = " << s.currentTemp << " °C\n"
        << "  targetTemp  = " << s.targetTemp << " °C\n";

    /* ========== 地址 5：微动开关 ========== */
    qDebug().noquote()
        << "Limit Switch (addr 5):\n"
        << QString("  raw = 0x%1")
               .arg(s.limitSwitch.raw, 4, 16, QLatin1Char('0'));

    qDebug().noquote()
        << "  slide_home  =" << ((s.limitSwitch.raw >> 0) & 0x1)
        << "\n  slide_limit =" << ((s.limitSwitch.raw >> 1) & 0x1)
        << "\n  incub_pos1  =" << ((s.limitSwitch.raw >> 2) & 0x1)
        << "\n  incub_pos2  =" << ((s.limitSwitch.raw >> 3) & 0x1)
        << "\n  incub_pos3  =" << ((s.limitSwitch.raw >> 4) & 0x1)
        << "\n  incub_pos4  =" << ((s.limitSwitch.raw >> 5) & 0x1)
        << "\n  incub_pos5  =" << ((s.limitSwitch.raw >> 6) & 0x1)
        << "\n  incub_pos6  =" << ((s.limitSwitch.raw >> 7) & 0x1);

    /* ========== 地址 6：孵育完成标志 ========== */
    qDebug().noquote()
        << "Incub Finish Mask (addr 6):\n"
        << QString("  raw = 0x%1")
               .arg(s.incubFinish.raw, 4, 16, QLatin1Char('0'));

    qDebug().noquote()
        << "  pos1 =" << ((s.incubFinish.raw >> 0) & 0x1)
        << "\n  pos2 =" << ((s.incubFinish.raw >> 1) & 0x1)
        << "\n  pos3 =" << ((s.incubFinish.raw >> 2) & 0x1)
        << "\n  pos4 =" << ((s.incubFinish.raw >> 3) & 0x1)
        << "\n  pos5 =" << ((s.incubFinish.raw >> 4) & 0x1)
        << "\n  pos6 =" << ((s.incubFinish.raw >> 5) & 0x1);

    /* ========== 地址 7：孵育状态 ========== */
    qDebug().noquote()
        << "Incub State (addr 7):\n"
        << "  incubState raw =" << static_cast<int>(s.incubState);

    /* ========== 电机 ========== */
    qDebug().noquote()
        << "Motor:\n"
        << "  motorState =" << s.motorState
        << "\n  motorSteps =" << s.motorSteps
        << "\n  motorSpeed =" << s.motorSpeed;

    /* ========== 光学模块 ========== */
    qDebug().noquote()
        << "Optical:\n"
        << "  fluorescence =" << s.fluorescence
        << "\n==================================";
}

void DeviceService::pollOnce() {
    // pollOnce 一定在工作线程触发
    qDebug() << "\n[DEVICE] ===== pollOnce ===== thread=" << QThread::currentThread();
    DeviceStatus m_status{};
    /* ================= ① 所有需要读取的功能 ================= */
    QVector<DevFunc> pollList = {
        DevFunc::ReadCurrentTemp,       // 1–2
        DevFunc::SetTargetTemp,         // 3–4（读回）
        DevFunc::ReadLimitSwitch,       // 5
        DevFunc::WriteIncubFinishMask,  // 6（读回）
        DevFunc::ReadIncubState,        // 7
        DevFunc::ReadMotorState         // 23
        // DevFunc::ReadMotorSteps,        // 24
        // DevFunc::EnableFluorescence     // 25（读回）
    };

    struct ReqItem {
        uint16_t addr;
        uint16_t count;
        DevFunc func;
        QVector<uint16_t> out;
    };

    QVector<ReqItem> reqs;
    reqs.reserve(pollList.size());

    /* ================= ② 生成读请求 ================= */
    for (auto f : pollList) {
        const FuncDesc* d = findFuncDesc(f);
        if (!d) {
            qWarning() << "[DEVICE] FuncDesc not found:" << int(f);
            continue;
        }

        ReqItem it;
        it.addr = uint16_t(d->startAddr);
        it.count = uint16_t(d->regCount);
        it.func = f;
        it.out.resize(it.count);

        reqs.push_back(it);

        qDebug().nospace()
            << "[DEVICE] need read func=" << int(f)
            << " addr=" << it.addr
            << " count=" << it.count;
    }

    if (reqs.isEmpty()) {
        qWarning() << "[DEVICE] pollOnce reqs empty";
        return;
    }

    /* ================= ③ 按地址排序 ================= */
    std::sort(reqs.begin(), reqs.end(),
              [](const ReqItem& a, const ReqItem& b) {
                  return a.addr < b.addr;
              });

    struct MergeBlock {
        uint16_t start;
        uint16_t count;
        QVector<uint16_t> values;
    };

    QVector<MergeBlock> merged;
    merged.reserve(reqs.size());

    /* ================= ④ 合并连续地址 ================= */
    for (const auto& r : reqs) {
        if (!merged.isEmpty()) {
            auto& last = merged.last();
            uint16_t lastEnd = uint16_t(last.start + last.count);
            if (lastEnd == r.addr) {
                last.count = uint16_t(last.count + r.count);
                continue;
            }
        }
        merged.push_back({r.addr, r.count, {}});
    }

    for (const auto& m : merged) {
        qDebug().nospace()
            << "[DEVICE] merged read addr=" << m.start
            << " count=" << m.count;
    }

    /* ================= ⑤ 执行合并读 ================= */
    bool okAll = true;

    for (auto& m : merged) {
        QVector<uint16_t> out;
        bool ok = m_worker && m_worker->readBlock(m.start, m.count, out);
        if (!ok) {
            okAll = false;
            qWarning() << "[DEVICE] merged block read failed addr="
                       << m.start << "count=" << m.count;
            m.values.clear();
            continue;
        }

        m.values = out;
        qDebug() << "[DEVICE] merged raw regs addr=" << m.start
                 << "count=" << m.count << "regs=" << m.values;
    }

    /* ================= ⑥ 分发到各 ReqItem ================= */
    for (auto& r : reqs) {
        bool filled = false;

        for (const auto& m : merged) {
            if (m.values.isEmpty())
                continue;

            uint16_t mEnd = uint16_t(m.start + m.count);
            uint16_t rEnd = uint16_t(r.addr + r.count);

            if (r.addr >= m.start && rEnd <= mEnd) {
                int offset = int(r.addr - m.start);
                for (int i = 0; i < r.count; ++i)
                    r.out[i] = m.values[offset + i];
                filled = true;
                break;
            }
        }

        if (!filled)
            std::fill(r.out.begin(), r.out.end(), 0);

        qDebug() << "[DEVICE] addr=" << r.addr << "raw regs=" << r.out;
    }

    if (!okAll)
        qWarning() << "[DEVICE] readMulti partial failed";

    /* ================= ⑦ 解析并写入 DeviceStatus ================= */
    for (const auto& r : reqs) {
        switch (r.func) {
        case DevFunc::ReadCurrentTemp:
            m_status.currentTemp = regsToFloat_CDAB(r.out);
            emit currentTempUpdated(m_status.currentTemp);
            break;

        case DevFunc::SetTargetTemp:
            m_status.targetTemp = regsToFloat_CDAB(r.out);
            break;

        case DevFunc::ReadLimitSwitch:
            m_status.limitSwitch.raw = r.out[0];
            emit limitSwitchUpdated(m_status.limitSwitch.raw);
            break;

        case DevFunc::WriteIncubFinishMask:
            m_status.incubFinish.raw = r.out[0];
            break;

        case DevFunc::ReadIncubState:
            m_status.incubState =
                static_cast<IncubState>(r.out[0]);
            emit incubStateUpdated(r.out[0]);
            break;

        case DevFunc::ReadMotorState:
            m_status.motorState = r.out[0];
            emit motorStateUpdated(r.out[0]);
            break;

        case DevFunc::ReadMotorSteps:
            m_status.motorSteps = r.out[0];
            emit motorStepsUpdated(r.out[0]);
            break;

        case DevFunc::SetMotorSpeed:
            m_status.motorSpeed = r.out[0];
            break;

        case DevFunc::EnableFluorescence:
            m_status.fluorescence = r.out[0];
            break;

        default:
            break;
        }
    }
    dumpDeviceStatus(m_status);
}
