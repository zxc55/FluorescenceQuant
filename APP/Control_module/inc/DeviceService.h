#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "DeviceProtocol.h"
#include "DeviceState.h"
#include "DeviceStatusObject.h"
#include "ModbusRtuClient.h"
enum class TaskType {
    PollOnce,
    ExecItems
};

struct Task {
    TaskType type;
    QVector<ExecItem> execItems;  // 仅 ExecItems 使用
};
class DeviceService : public QObject {
    Q_OBJECT
    Q_PROPERTY(DeviceStatusObject* status READ status CONSTANT)

public:
    explicit DeviceService(ModbusRtuClient* worker);
    DeviceService(QObject* parent = nullptr) : QObject(parent), m_worker(nullptr) {}
    ~DeviceService();
    Q_INVOKABLE void ENFLALED(int enable);
    Q_INVOKABLE void exec(const QVector<ExecItem>& items);
    Q_INVOKABLE void motorStart();
    Q_INVOKABLE void motorStart_2();
    Q_INVOKABLE void setTargetTemperature(float temperature);
    Q_INVOKABLE void setIncubationTime(int seconds);
    DeviceStatusObject* status() { return &m_statusObj; }
    // 启停后台线程
    void start(int pollIntervalMs);
    void stop();

signals:
    void currentTempUpdated(float);
    void limitSwitchUpdated(uint16_t);
    void incubStateUpdated(uint16_t);
    void motorStateUpdated(uint16_t);
    void motorStepsUpdated(uint16_t);

private:
    // ===== 后台线程 =====
    void threadLoop();

    // ===== 原 pollOnce 逻辑，改为普通函数 =====
    void pollOnceInternal();

private:
    int m_pollIntervalMs = 500;
    static int INCUB_TOTAL_SEC;  // 6 分钟
    // ===== 孵育超时寄存器配置 =====
    static constexpr uint16_t FUYU_TIMEOUT_ADDR = 8;
    // ===== 防止重复写 =====
    bool m_fuyuTimeoutSent = false;
    bool m_lastIncubPos[6] = {false, false, false, false, false, false};
    DeviceStatus m_status;           // 协议层（struct）
    DeviceStatusObject m_statusObj;  // ★ UI 层（QObject）

    ModbusRtuClient* m_worker = nullptr;

    std::thread m_thread;
    std::atomic<bool> m_running{false};

    std::queue<Task> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};
