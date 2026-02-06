#include "WiFiController.h"

#include <QDebug>
#include <QProcess>

WiFiController::WiFiController(QObject* parent)
    : QObject(parent) {
}

// ===== QML 接口实现 =====

void WiFiController::turnOn() {
    Task task{TaskType::TurnOn};
    m_worker.post([=]() { executeTask(task); });
}

void WiFiController::turnOff() {
    Task task{TaskType::TurnOff};
    m_worker.post([=]() { executeTask(task); });
}

void WiFiController::disconnect() {
    Task task{TaskType::Disconnect};
    m_worker.post([=]() { executeTask(task); });
}
void WiFiController::statrUdcpc() {
    Task task{TaskType::StartUdh};
    m_worker.post([=]() { executeTask(task); });
}
void WiFiController::connectSaved(const QString& ssid) {
    Task task;
    task.type = TaskType::ConnectSaved;
    task.ssid = ssid.toStdString();

    m_worker.post([=]() { executeTask(task); });
}

void WiFiController::connectWithPassword(const QString& ssid,
                                         const QString& password) {
    Task task;
    task.type = TaskType::ConnectWithPassword;
    task.ssid = ssid.toStdString();
    task.password = password.toStdString();

    m_worker.post([=]() { executeTask(task); });
}
void WiFiController::connect(const QString& ssid) {
    Task task;
    task.type = TaskType::Connect;
    task.ssid = ssid.toStdString();

    m_worker.post([=]() { executeTask(task); });
}

void WiFiController::scan() {
    Task task{TaskType::Scan};
    m_worker.post([=]() { executeTask(task); });
}

QString WiFiController::currentSSID() {
    return QString::fromStdString(m_wifi.getCurrentWiFiSSID());
}

// ===== 后台线程：任务执行（case 核心） =====

void WiFiController::executeTask(const Task& task) {
    switch (task.type) {
    case TaskType::TurnOn:
        m_wifi.turnOnWiFi();
        break;

    case TaskType::TurnOff:
        m_wifi.turnOffWiFi();
        break;

    case TaskType::Disconnect:
        m_wifi.disconnectWiFi();
        break;

    case TaskType::ConnectSaved: {
        int ret = m_wifi.connectToWiFi(task.ssid);
        if (ret == 0) {
            emit connected(QString::fromStdString(task.ssid));
        } else {
            emit connectFailed("连接已保存网络失败");
        }
        break;
    }

    case TaskType::ConnectWithPassword: {
        int ret = m_wifi.connectToWiFi(task.ssid, task.password);
        if (ret == 0) {
            emit connected(QString::fromStdString(task.ssid));
        } else {
            // emit connectFailed("连接失败或密码错误");
        }
        break;
    }
    case TaskType::Connect: {
        int ret = m_wifi.connectToWiFi(task.ssid);
        if (ret == 0) {
            emit connected(QString::fromStdString(task.ssid));
            qDebug() << "已连接" << QString::fromStdString(task.ssid);
        } else {
            emit connectFailed_1(
                QString::fromStdString(task.ssid),
                QStringLiteral("auth_failed"));
            qDebug() << "连接失败" << QString::fromStdString(task.ssid);
        }
        break;
    }
    case TaskType::Scan: {
        std::unordered_map<std::string, int> result;
        m_wifi.scanWiFi(result);

        QVariantList list;
        for (const auto& it : result) {
            QVariantMap item;
            item["ssid"] = QString::fromStdString(it.first);
            item["signal"] = it.second;
            list.append(item);
        }

        emit scanFinished(list);
        break;
    }
    case TaskType::StartUdh: {
        m_wifi.statrt_uchcpc(3000, 10);
        break;
    }
    }
}

QString WiFiController::currentIp() {
    QProcess p;
    p.start("/bin/sh", {"-c",
                        // 优先 wlan0，没有再 eth1
                        "ip -4 addr show wlan0 | grep 'inet ' | awk '{print $2}' | cut -d/ -f1 || "
                        "ip -4 addr show eth1  | grep 'inet ' | awk '{print $2}' | cut -d/ -f1"});
    p.waitForFinished(1000);

    QString ip = QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
    return ip;
}
bool WiFiController::isWifiConnected() {
    return m_wifi.isWifiConnected();
}