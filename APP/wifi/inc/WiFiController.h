#ifndef WIFI_CONTROLLER_H
#define WIFI_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>

#include "WorkerQueue.h"
#include "wifiManage.h"
/**
 * @brief WiFi 控制器（任务队列 + case 分发）
 *
 * 所有 WiFi 操作：
 *  - 统一丢进后台队列
 *  - 串行执行
 *  - 空闲时沉睡
 */
class WiFiController : public QObject {
    Q_OBJECT
public:
    explicit WiFiController(QObject* parent = nullptr);

    // ===== QML / 上层接口 =====
    Q_INVOKABLE void turnOn();
    Q_INVOKABLE void turnOff();
    Q_INVOKABLE void disconnect();

    Q_INVOKABLE void connectSaved(const QString& ssid);
    Q_INVOKABLE void connectWithPassword(const QString& ssid,
                                         const QString& password);
    Q_INVOKABLE void connect(const QString& ssid);
    Q_INVOKABLE void statrUdcpc();
    Q_INVOKABLE void scan();
    Q_INVOKABLE QString currentSSID();
    Q_INVOKABLE QString currentIp();
    Q_INVOKABLE bool isWifiConnected();
    void autoDhcpIfNeeded();
signals:
    void connected(const QString& ssid);
    void connectFailed_1(const QString& ssid, const QString& reason);
    void connectFailed(const QString& reason);
    void scanFinished(const QVariantList& list);
    void defaultWifiResult(const bool wifi_state);

private:
    // WiFi 任务类型
    enum class TaskType {
        TurnOn,
        TurnOff,
        Disconnect,
        ConnectSaved,
        ConnectWithPassword,
        Connect,
        StartUdh,
        Scan,
        Get_wlan
    };

    // 任务结构
    struct Task {
        TaskType type;
        std::string ssid;
        std::string password;
    };

    // 真正执行任务（后台线程调用）
    void executeTask(const Task& task);

private:
    WiFiManage m_wifi;     // 你已有的 WiFi 实现
    WorkerQueue m_worker;  // 后台任务队列
};

#endif  // WIFI_CONTROLLER_H
