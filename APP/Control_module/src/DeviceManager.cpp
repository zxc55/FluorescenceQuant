#include "DeviceManager.h"

#include <QCoreApplication>
#include <QDebug>

#include "DeviceService.h"
#include "ModbusRtuClient.h"

DeviceManager::DeviceManager(QObject* parent)
    : QObject(parent) {
    m_worker = new ModbusRtuClient;
    m_service = new DeviceService(m_worker);

    m_worker->moveToThread(&m_thread);
    m_service->moveToThread(&m_thread);

    connect(&m_thread, &QThread::started,
            m_service, &DeviceService::onThreadStarted);

    connect(&m_thread, &QThread::started, this, [this]() {
        m_service->startPolling(500);
    });
}

DeviceManager::~DeviceManager() {
    stop();
}

QObject* DeviceManager::service() const {
    return m_service;  // m_service 是 DeviceService*
}

void DeviceManager::start() {
    qDebug() << "[DEV-MGR] start() called, thread running=" << m_thread.isRunning();
    if (m_thread.isRunning())
        return;

    m_thread.start();
}

void DeviceManager::stop() {
    qDebug() << "[DEV-MGR] stop() called, thread running=" << m_thread.isRunning();
    if (!m_thread.isRunning())
        return;

    // 让 service 在自己的线程里停 timer
    QMetaObject::invokeMethod(m_service, "stopPolling", Qt::BlockingQueuedConnection);

    m_thread.quit();
    m_thread.wait();

    // 线程停了再删对象（不跨线程 delete）
    delete m_service;
    m_service = nullptr;

    delete m_worker;
    m_worker = nullptr;

    qDebug() << "[DEV-MGR] stopped";
}
