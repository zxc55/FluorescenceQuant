#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>

#include "DeviceProtocol.h"
#include "DeviceState.h"
#include "ModbusRtuClient.h"
class DeviceService : public QObject {
    Q_OBJECT
public:
    explicit DeviceService(ModbusRtuClient* worker);

    Q_INVOKABLE void exec(const QVector<ExecItem>& items);

public slots:
    void onThreadStarted();  // 在工作线程执行初始化
    void startPolling(int intervalMs = 500);
    void stopPolling();

signals:
    void currentTempUpdated(float);
    void limitSwitchUpdated(uint16_t);
    void incubStateUpdated(uint16_t);
    void motorStateUpdated(uint16_t);
    void motorStepsUpdated(uint16_t);

private slots:
    void pollOnce();

private:
    void processLoop();
    DeviceStatus m_status;
    ModbusRtuClient* m_worker = nullptr;
    QTimer* m_pollTimer = nullptr;  // ★ 改为指针，确保在工作线程 new
};
