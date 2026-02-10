#include "NetWebGuard.h"

#include <QDebug>

#include "NetWebGuard.h"
#include "WiFiController.h"
#include "WifiDetectWorker.h"
#include "embedded_web_server.h"  // ★ 你这个文件
NetWebGuard::NetWebGuard(WiFiController* wifi,
                         embedded::EmbeddedWebServer* web,
                         QObject* parent)
    : QObject(parent),
      m_wifi(wifi),
      m_web(web),
      m_lastWifiConnected(false),
      m_webStarted(false),
      m_worker(nullptr),
      m_timer(nullptr) {
    // ===== 1. 创建 worker（只负责 WiFi 检测）=====
    m_worker = new WifiDetectWorker(m_wifi);
    m_worker->moveToThread(&m_thread);

    // ===== 2. 创建定时器（在子线程）=====
    m_timer = new QTimer;
    m_timer->setInterval(2000);  // 2 秒检测一次
    m_timer->setSingleShot(false);
    m_timer->moveToThread(&m_thread);

    // ===== 3. 线程启动后启动定时器 =====
    connect(&m_thread, &QThread::started,
            m_timer, QOverload<>::of(&QTimer::start));

    // ===== 4. 定时器 → worker（子线程调用阻塞函数）=====
    connect(m_timer, &QTimer::timeout,
            m_worker, &WifiDetectWorker::check);

    // ===== 5. worker → guard（回主线程）=====
    connect(m_worker, &WifiDetectWorker::wifiState,
            this, &NetWebGuard::onWifiState,
            Qt::QueuedConnection);

    // ===== 6. 线程结束时清理 =====
    connect(&m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(&m_thread, &QThread::finished,
            m_timer, &QObject::deleteLater);

    // ===== 7. 启动线程 =====
    m_thread.start();

    qInfo() << "[NetWebGuard] started";
}

NetWebGuard::~NetWebGuard() {
    m_thread.quit();
    m_thread.wait();

    if (m_webStarted) {
        m_web->stop();
        m_webStarted = false;
    }
}

void NetWebGuard::onWifiState(bool connected) {
    if (connected == m_lastWifiConnected)
        return;

    m_lastWifiConnected = connected;

    if (connected) {
        qInfo() << "[NetWebGuard] WiFi connected";

        if (!m_webStarted) {
            std::string err;
            if (m_web->start(err)) {
                m_webStarted = true;
                qInfo() << "[Web] started (wifi ok)";
            } else {
                qWarning() << "[Web] start failed:"
                           << QString::fromStdString(err);
            }
        }
    } else {
        qInfo() << "[NetWebGuard] WiFi disconnected";

        if (m_webStarted) {
            m_web->stop();
            m_webStarted = false;
            qInfo() << "[Web] stopped (wifi lost)";
        }
    }
}
