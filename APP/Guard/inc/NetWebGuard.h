#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>

namespace embedded {
class EmbeddedWebServer;
}

class WiFiController;
class WifiDetectWorker;

class NetWebGuard : public QObject {
    Q_OBJECT
public:
    explicit NetWebGuard(WiFiController* wifi,
                         embedded::EmbeddedWebServer* web,
                         QObject* parent = nullptr);
    ~NetWebGuard();

private slots:
    void onWifiState(bool connected);  // 回到主线程

private:
    WiFiController* m_wifi;
    embedded::EmbeddedWebServer* m_web;
    bool m_lastWifiConnected;
    bool m_webStarted;

    QThread m_thread;
    WifiDetectWorker* m_worker;
    QTimer* m_timer;
};
