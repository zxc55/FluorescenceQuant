#pragma once
#include <QObject>
#include <QThread>

class DeviceService;
class ModbusRtuClient;

class DeviceManager : public QObject {
    Q_OBJECT
public:
    QObject* service() const;

    explicit DeviceManager(QObject* parent = nullptr);
    ~DeviceManager();

public slots:
    void start();
    void stop();

private:
    QThread m_thread;
    ModbusRtuClient* m_worker = nullptr;
    DeviceService* m_service = nullptr;
};
