#include "WifiDetectWorker.h"

#include "WiFiController.h"

WifiDetectWorker::WifiDetectWorker(WiFiController* wifi, QObject* parent)
    : QObject(parent),
      m_wifi(wifi) {
}

void WifiDetectWorker::check() {
    if (!m_wifi) {
        emit wifiState(false);
        return;
    }

    // ====================================
    // ① 如果 WiFi 已连但还没 IP → 自动 DHCP
    //    （内部已做条件判断，不会乱跑）
    // ====================================
    try {
        m_wifi->autoDhcpIfNeeded();
    } catch (...) {
        // DHCP 尝试失败不算致命，继续判断状态
    }

    // ====================================
    // ② 判断最终是否“真正在线”
    // ====================================
    bool connected = false;
    try {
        connected = m_wifi->isWifiConnected();  // ★ 阻塞点
    } catch (...) {
        connected = false;
    }

    // ====================================
    // ③ 抛出最终状态
    // ====================================
    emit wifiState(connected);
}
