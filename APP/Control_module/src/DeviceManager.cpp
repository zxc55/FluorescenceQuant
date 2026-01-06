#include "DeviceManager.h"

#include <QDebug>

#include "DeviceService.h"
#include "ModbusRtuClient.h"

DeviceManager::DeviceManager(QObject* parent)
    : QObject(parent) {
    m_worker = new ModbusRtuClient;
    m_service = new DeviceService(m_worker);
}

DeviceManager::~DeviceManager() {
    stop();
}

QObject* DeviceManager::service() const {
    return m_service;  // 暴露给 QML
}

void DeviceManager::start() {
    qDebug() << "[DEV-MGR] start()";

    if (!m_service)
        return;

    // ✅ 启动 DeviceService 内部 std::thread
    m_service->start(500);
}

void DeviceManager::stop() {
    qDebug() << "[DEV-MGR] stop()";

    if (!m_service)
        return;

    // ✅ 停止通信线程（join 在内部）
    m_service->stop();

    delete m_service;
    m_service = nullptr;

    delete m_worker;
    m_worker = nullptr;

    qDebug() << "[DEV-MGR] stopped";
}
