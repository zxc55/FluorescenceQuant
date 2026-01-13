#include "DeviceService.h"

#include <QDebug>
#include <QtGlobal>
#include <algorithm>

#include "DeviceState.h"
#include "DeviceStatusObject.h"
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
}

DeviceService::~DeviceService() {
    stop();
}
void DeviceService::start(int pollIntervalMs) {
    if (m_running)
        return;

    m_pollIntervalMs = pollIntervalMs;
    m_running = true;
    m_thread = std::thread(&DeviceService::threadLoop, this);
}
void DeviceService::stop() {
    if (!m_running)
        return;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_running = false;
    }
    m_cv.notify_all();

    if (m_thread.joinable())
        m_thread.join();

    qDebug() << "[DeviceService] std::thread stopped";
}
void DeviceService::exec(const QVector<ExecItem>& items) {
    if (items.isEmpty())
        return;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_queue.push(Task{TaskType::ExecItems, items});
    }

    m_cv.notify_one();

    qDebug() << "[DeviceService] exec queued items =" << items.size();
}
void DeviceService::threadLoop() {
    using clock = std::chrono::steady_clock;

    // ===== poll 定时 =====
    auto nextPollTime = clock::now();  // 启动立即 poll 一次

    // ===== 倒计时 1 秒 tick =====
    auto lastSecondTick = clock::now();

    while (true) {
        /* =========================================================
         * ① 每秒倒计时（不阻塞、不影响 poll / 写任务）
         * ========================================================= */
        {
            auto now = clock::now();
            if (now - lastSecondTick >= std::chrono::seconds(1)) {
                lastSecondTick = now;
                bool anyTimeout = false;
                // 6 个孵育槽
                for (int i = 0; i < 6; ++i) {
                    if (!m_lastIncubPos[i])
                        continue;  // 当前不在孵育

                    int sec = 0;
                    switch (i) {
                    case 0:
                        sec = m_statusObj.incubRemain1();
                        break;
                    case 1:
                        sec = m_statusObj.incubRemain2();
                        break;
                    case 2:
                        sec = m_statusObj.incubRemain3();
                        break;
                    case 3:
                        sec = m_statusObj.incubRemain4();
                        break;
                    case 4:
                        sec = m_statusObj.incubRemain5();
                        break;
                    case 5:
                        sec = m_statusObj.incubRemain6();
                        break;
                    }
                    // 已经结束，不再处理
                    if (sec <= 0)
                        continue;

                    // ===== 倒计时 =====
                    sec--;
                    m_statusObj.setIncubRemain(i, sec);

                    // ★ 关键：刚好结束
                    if (sec == 0) {
                        qDebug() << "[DeviceService] incub slot" << i << "finished";
                        anyTimeout = true;
                    }
                    if (anyTimeout && !m_fuyuTimeoutSent) {
                        ExecItem it;
                        it.func = DevFunc::incubatetimeout;
                        it.value = 1;

                        exec({it});  // 进入线程队列

                        m_fuyuTimeoutSent = true;

                        qDebug() << "[DeviceService] fuyutimeout write addr=8";
                    }
                }
            }
        }

        /* =========================================================
         * ② 等待任务 / poll 时间 / 退出
         * ========================================================= */
        Task task;
        bool hasTask = false;
        bool doPoll = false;

        {
            std::unique_lock<std::mutex> lk(m_mutex);

            m_cv.wait_until(
                lk,
                nextPollTime,
                [this]() {
                    return !m_queue.empty() || !m_running;
                });

            // ===== 退出条件 =====
            if (!m_running && m_queue.empty())
                break;

            // ===== 优先处理写任务 =====
            if (!m_queue.empty()) {
                task = std::move(m_queue.front());
                m_queue.pop();
                hasTask = true;
            } else {
                // ===== poll 时间到 =====
                doPoll = true;
                nextPollTime = clock::now() +
                               std::chrono::milliseconds(m_pollIntervalMs);
            }
        }

        /* =========================================================
         * ③ 执行写任务
         * ========================================================= */
        if (hasTask) {
            switch (task.type) {
            case TaskType::ExecItems:
                for (const auto& it : task.execItems) {
                    switch (it.func) {
                    case DevFunc::SetTargetTemp: {
                        float temp = it.value.toFloat();

                        uint16_t cd, ab;
                        floatToRegs_CDAB(temp, cd, ab);

                        QVector<uint16_t> regs;
                        regs << cd << ab;

                        constexpr uint16_t TARGET_TEMP_ADDR = 0x0003;
                        m_worker->postWriteRegisters(TARGET_TEMP_ADDR, regs);

                        qDebug() << "[DeviceService] write target temp =" << temp;
                        break;
                    }

                    case DevFunc::incubatetimeout: {  // ★ 孵育超时
                        QVector<uint16_t> regs;
                        regs << 1;  // 写 1

                        constexpr uint16_t FUYU_TIMEOUT_ADDR = 8;
                        m_worker->postWriteRegisters(FUYU_TIMEOUT_ADDR, regs);

                        qDebug() << "[DeviceService] write incubate timeout = 1";
                        break;
                    }

                    default:
                        qWarning() << "[DeviceService] unsupported DevFunc:"
                                   << int(it.func);
                        break;
                    }
                }
                break;

            default:
                break;
            }
            continue;
        }

        /* =========================================================
         * ④ 执行 poll
         * ========================================================= */
        if (doPoll) {
            pollOnceInternal();
            continue;
        }
    }

    qDebug() << "[DeviceService] threadLoop exit";
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
void DeviceStatusObject::dumpStatus() const {
    qDebug().noquote()
        << "\n========== DeviceStatusObject ==========\n"
        << "Temperature:\n"
        << "  currentTemp =" << m_currentTemp << "°C\n"
        << "  targetTemp  =" << m_targetTemp << "°C\n"

        << "Home State:\n"
        << "  powerOnHome =" << m_powerOnHome << "\n"
        << "  cardHome    =" << m_cardHome << "\n"

        << "Incub Positions:\n"
        << "  pos1 =" << m_incubPos1 << " remain1 =" << m_incubRemain1 << "s\n"
        << "  pos2 =" << m_incubPos2 << " remain2 =" << m_incubRemain2 << "s\n"
        << "  pos3 =" << m_incubPos3 << " remain3 =" << m_incubRemain3 << "s\n"
        << "  pos4 =" << m_incubPos4 << " remain4 =" << m_incubRemain4 << "s\n"
        << "  pos5 =" << m_incubPos5 << " remain5 =" << m_incubRemain5 << "s\n"
        << "  pos6 =" << m_incubPos6 << " remain6 =" << m_incubRemain6 << "s\n"

        << "=======================================";
}
void DeviceService::pollOnceInternal() {
    // pollOnce 一定在工作线程触发
    //  m_statusObj.setCurrentTemp(42.0f);

    // qDebug() << "\n[DEVICE] ===== pollOnce ===== thread=" << QThread::currentThread();
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

        // qDebug().nospace()
        //     << "[DEVICE] need read func=" << int(f)
        //     << " addr=" << it.addr
        //     << " count=" << it.count;
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

    // for (const auto& m : merged) {
    //     qDebug().nospace()
    //         << "[DEVICE] merged read addr=" << m.start
    //         << " count=" << m.count;
    // }

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
        // qDebug() << "[DEVICE] merged raw regs addr=" << m.start
        //          << "count=" << m.count << "regs=" << m.values;
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
            qDebug() << "[DEVICE] currentTemp=" << m_status.currentTemp;
            m_statusObj.setCurrentTemp(m_status.currentTemp);
            // emit currentTempUpdated(m_status.currentTemp);
            break;

        case DevFunc::SetTargetTemp:
            m_status.targetTemp = regsToFloat_CDAB(r.out);
            break;

        case DevFunc::ReadLimitSwitch: {
            uint16_t raw = r.out[0];
            m_status.limitSwitch.raw = raw;
            bool powerOnHome = (raw & (1 << 0)) != 0;  // bit0
            bool cardHome = (raw & (1 << 1)) != 0;     // bit1
            m_statusObj.setPowerOnHome(powerOnHome);
            m_statusObj.setCardHome(cardHome);
            // ===== 解析 6 个孵育槽 bit =====
            bool curr[6] = {
                raw & (1 << 2),
                raw & (1 << 3),
                raw & (1 << 4),
                raw & (1 << 5),
                raw & (1 << 6),
                raw & (1 << 7),
            };

            for (int i = 0; i < 6; ++i) {
                // 更新孵育槽是否激活（给 QML）
                m_statusObj.setIncubPos(i, curr[i]);

                // 0 → 1：刚放入孵育槽，启动 6 分钟倒计时
                if (!m_lastIncubPos[i] && curr[i]) {
                    m_statusObj.setIncubRemain(i, INCUB_TOTAL_SEC);
                }

                // 1 → 0：移出孵育槽，清空倒计时
                if (m_lastIncubPos[i] && !curr[i]) {
                    m_statusObj.setIncubRemain(i, 0);
                }

                m_lastIncubPos[i] = curr[i];
            }

            break;
        }

        case DevFunc::WriteIncubFinishMask:
            m_status.incubFinish.raw = r.out[0];
            break;

        case DevFunc::ReadIncubState:
            m_status.incubState =
                static_cast<IncubState>(r.out[0]);
            emit incubStateUpdated(r.out[0]);
            break;
        case DevFunc::incubatetimeout:

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