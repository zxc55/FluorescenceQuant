#pragma once

#include <QObject>

class WiFiController;

class WifiDetectWorker : public QObject {
    Q_OBJECT
public:
    explicit WifiDetectWorker(WiFiController* wifi,
                              QObject* parent = nullptr);

public slots:
    void check();  // 子线程里调用

signals:
    void wifiState(bool connected);

private:
    WiFiController* m_wifi;  // 不负责释放
};
