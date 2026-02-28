#include "./wifiManage.h"

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "qdebug.h"

#define WIFI_LOG

// =====================
// 仅 cpp 内部用的小工具（不改你类的接口）
// =====================

// =====================
// cpp 内部 helper：不要调用 self->executeCommand（private）
// =====================
static std::string _execCmdRaw(const std::string& command) {
    char buffer[128];
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
#ifdef WIFI_LOG
        qDebug("执行命令失败: %s", command.c_str());
#endif
        return "";
    }
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
#ifdef WIFI_LOG
    // qDebug("执行命令: %s 结果: %s", command.c_str(), result.c_str());
#endif
    return result;
}

static bool _wpaPingOk(WiFiManage* /*self*/) {
    std::string out = _execCmdRaw("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 ping 2>/dev/null");
    return out.find("PONG") != std::string::npos;
}

static bool _wlan0LinkLowerUp(WiFiManage* /*self*/) {
    std::string lk = _execCmdRaw("ip link show dev wlan0 2>/dev/null");
    if (lk.find("LOWER_UP") == std::string::npos)
        return false;
    if (lk.find("UP") == std::string::npos)
        return false;
    return true;
}

static void _cleanupWlan0NetState(WiFiManage* /*self*/) {
    _execCmdRaw("pkill -f \"udhcpc.*-i wlan0\" 2>/dev/null || true");
    _execCmdRaw("ip route del default dev wlan0 2>/dev/null || true");
    _execCmdRaw("ip addr flush dev wlan0 2>/dev/null || true");
}

static void _cleanupWpaCtrlStale(WiFiManage* /*self*/) {
    _execCmdRaw("rm -f /tmp/lock/wpa_supplicant/wlan0 2>/dev/null || true");
}

// Helper function to execute a command and capture the output
std::string WiFiManage::executeCommand(const std::string& command) {
    char buffer[128];
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        qDebug("执行命令失败: %s", command.c_str());
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
#ifdef WIFI_LOG
    // qDebug("执行命令: %s 结果: %s", command.c_str(), result.c_str());
#endif
    return result;
}

void WiFiManage::parseResults(iw_event* event, int length, std::unordered_map<std::string, int>& wlanMap) {
    char* currentPos = reinterpret_cast<char*>(event);
    char* end = currentPos + length;
    std::string lastSSID;

    while (currentPos < end) {
        iw_event* iwe = reinterpret_cast<iw_event*>(currentPos);

        if (iwe->cmd == SIOCGIWESSID) {
            iw_point* iwp = reinterpret_cast<iw_point*>(&iwe->u);
            lastSSID = std::string((char*)(iwp), iwe->len - 4);
            std::string sanitizedSSID;
            for (char c : lastSSID) {
                if (!std::iscntrl(static_cast<unsigned char>(c))) {
                    sanitizedSSID += c;
                }
            }
            lastSSID = sanitizedSSID;
            if (!lastSSID.empty()) {
                wlanMap[lastSSID] = 0;
            }

        } else if (iwe->cmd == IWEVQUAL && !lastSSID.empty()) {
            int signalStrength = iwe->u.qual.level;
            wlanMap[lastSSID] = signalStrength;
        }
        currentPos += iwe->len;
    }
}

// Check if a network with the given SSID already exists
std::string WiFiManage::findNetworkId(const std::string& ssid) {
    std::string networks = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 list_networks");
    std::istringstream iss(networks);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find(ssid) != std::string::npos) {
            return line.substr(0, line.find_first_of(" "));
        }
    }
    return "";
}

// Remove a network by ID
void WiFiManage::removeNetwork(const std::string& networkId) {
    executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 remove_network " + networkId);
    executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 save_config");
}

void WiFiManage::statrt_uchcpc(int timeout, int num_retries) {
#ifdef WIFI_LOG
    qDebug("开启udhcpc");
#endif
    executeCommand("udhcpc -i wlan0 >/dev/null 2>&1 &");
    qDebug("-------开启udhcpc-------");
}

void WiFiManage::turnOnWiFi() {
#ifdef WIFI_LOG
    qDebug("打开WiFi");
#endif
    // ★重点：不要 killall udhcpc（那会把 eth1 的 udhcpc 也杀掉）
    // 只杀 wlan0 的 udhcpc
    try {
        executeCommand("pkill -f \"udhcpc.*-i wlan0\" 2>/dev/null || true");
    } catch (...) {
    }

    // 先把 wlan0 拉起来（幂等）
    executeCommand("ifconfig wlan0 up 2>/dev/null || true");

    // ★如果控制接口已经能 PONG，说明 wpa_supplicant 已经起来了 -> 不要重复启动
    if (_wpaPingOk(this)) {
#ifdef WIFI_LOG
        qDebug("[WiFi] wpa_supplicant already alive (PONG), skip start");
#endif
        return;
    }

    // 准备启动前：清理可能的残留（ctrl socket + wlan0 残留网络状态）
    _cleanupWpaCtrlStale(this);
    _cleanupWlan0NetState(this);

    // 启动 wpa_supplicant（只启动一次）
    executeCommand("wpa_supplicant -D wext -c /etc/wpa_supplicant.conf -i wlan0 -B 2>/dev/null || true");

#ifdef WIFI_LOG
    // 如果你希望：只有保存过网络才认为“会自动连接”，否则就不再做额外动作
    //  bool has = this->hasSavedWiFiNetwork();
    // qDebug("[WiFi] hasSavedWiFiNetwork=%d", has ? 1 : 0);
#endif
}

// Turn off WiFi (bring down the wlan0 interface)
void WiFiManage::turnOffWiFi() {
#ifdef WIFI_LOG
    qDebug("关闭WiFi");
#endif
    // 1) 先断开并清理 wlan0（避免残留 inet / 默认路由误判）
    try {
        executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 disconnect 2>/dev/null || true");
    } catch (...) {
    }

    _cleanupWlan0NetState(this);

    // 2) 关 wlan0 + 停 wpa_supplicant + 清 ctrl socket
    executeCommand("ifconfig wlan0 down 2>/dev/null || true");
    executeCommand("killall wpa_supplicant 2>/dev/null || true");
    _cleanupWpaCtrlStale(this);

    // 3) 拉起 eth1 并 DHCP（你要求默认 eth1）
    executeCommand("ifconfig eth1 up 2>/dev/null || true; udhcpc -i eth1 -n -q -T 3 -t 8 2>/dev/null || true");
}

// Disconnect from the current WiFi
void WiFiManage::disconnectWiFi() {
#ifdef WIFI_LOG
    qDebug("断开WiFi");
#endif
    // disconnect 语义：不断电 WiFi 模块也行，但你这里需求是“断开后走 eth1”
    try {
        executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 disconnect 2>/dev/null || true");
    } catch (...) {
    }

    _cleanupWlan0NetState(this);

    executeCommand("ifconfig eth1 up 2>/dev/null || true; udhcpc -i eth1 -n -q -T 3 -t 8 2>/dev/null || true");
}

int WiFiManage::connectToWiFi(const std::string& ssid) {
    std::string networkId = findNetworkId(ssid);
    if (!networkId.empty()) {
#ifdef WIFI_LOG
        qDebug("Network found. Network ID: %s", networkId.c_str());
#endif
        std::string enable = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 enable_network all");
        if (enable.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 启用网络失败");
#endif
            removeNetwork(networkId);
            return -1;
        }

        std::string select = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 select_network " + networkId);
        if (select.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 选择网络失败");
#endif
            removeNetwork(networkId);
            return -1;
        }
        executeCommand("ip addr flush dev wlan0");
        int n = 10;
        while (n > 0) {
            std::string selects = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 status ");
            if (selects.find("wpa_state=COMPLETED") != std::string::npos) {
                executeCommand("udhcpc -i wlan0 -n -q -T 3 -t 8");  //
#ifdef WIFI_LOG
                qDebug("连接WiFi成功 %s", ssid.c_str());
#endif
                return 0;
            }
            if (selects.find("wpa_state=INTERFACE_DISABLED") != std::string::npos ||
                selects.find("wpa_state=INACTIVE") != std::string::npos) {
                break;
            }
            n--;
            sleep(1);
        }
#ifdef WIFI_LOG
        qDebug("连接WiFi失败: 选择网络失败");
#endif
        removeNetwork(networkId);
        return -1;
    } else {
        return -1;
    }

#ifdef WIFI_LOG
    qDebug("连接WiFi成功 %s", ssid.c_str());
#endif
    return 0;
}

int WiFiManage::connectToWiFi(const std::string& ssid, const std::string& password) {
#ifdef WIFI_LOG
    qDebug("连接WiFi: %s 密码: %s", ssid.c_str(), password.c_str());
#endif
    std::string networkId = findNetworkId(ssid);
    if (!networkId.empty()) {
#ifdef WIFI_LOG
        qDebug("Network found. Network ID: %s", networkId.c_str());
#endif
        if (!password.empty()) {
            std::string setPSK = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 set_network " + networkId +
                                                " psk '\"" + password + "\"'");
            if (setPSK.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
                qDebug("连接WiFi失败: 设置密码失败");
#endif
                removeNetwork(networkId);
                return -2;
            }
            executeCommand(" ");
        }
    } else {
        networkId = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 add_network");
        networkId = networkId.substr(0, networkId.find_first_of("\n"));

        if (networkId.empty()) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 添加网络失败");
#endif
            return -1;
        }

        std::string setSSID = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 set_network " + networkId +
                                             " ssid '\"" + ssid + "\"'");
        if (setSSID.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 设置SSID失败");
#endif
            removeNetwork(networkId);
            return -1;
        }

        std::string setPSK = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 set_network " + networkId +
                                            " psk '\"" + password + "\"'");
        if (setPSK.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 设置密码失败");
#endif
            removeNetwork(networkId);
            return -2;
        }

        std::string enable = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 enable_network all");
        if (enable.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 启用网络失败");
#endif
            removeNetwork(networkId);
            return -1;
        }

        executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 save_config");
    }

    std::string select = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 select_network " + networkId);
    if (select.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
        qDebug("连接WiFi失败: 选择网络失败");
#endif
        removeNetwork(networkId);
        return -1;
    }

    executeCommand("ip addr flush dev wlan0");

    int n = 10;
    while (n > 0) {
        std::string selects = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 status ");
        if (selects.find("wpa_state=COMPLETED") != std::string::npos) {
            executeCommand("udhcpc -i wlan0 -n -q -T 3 -t 8");
#ifdef WIFI_LOG
            qDebug("连接WiFi成功 %s", ssid.c_str());
#endif
            return 0;
        }
        if (selects.find("wpa_state=INTERFACE_DISABLED") != std::string::npos ||
            selects.find("wpa_state=INACTIVE") != std::string::npos) {
            break;
        }
        n--;
        sleep(1);
    }

#ifdef WIFI_LOG
    qDebug("连接WiFi失败: 选择网络失败");
#endif
    removeNetwork(networkId);
    return -1;
}

// Scan for available WiFi networks
void WiFiManage::scanWiFi(std::unordered_map<std::string, int>& wlanMap) {
#ifdef WIFI_LOG
    qDebug("扫描WiFi");
#endif
    wlanMap.clear();

    struct iwreq wrq1;
    memset(&wrq1, 0, sizeof(struct iwreq));

    strncpy(wrq1.ifr_name, "wlan0", IFNAMSIZ);
    wrq1.ifr_name[IFNAMSIZ - 1] = '\0';
    wrq1.u.data.pointer = NULL;
    wrq1.u.data.flags = 0;
    wrq1.u.data.length = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int tryN = 10;
    while (tryN--) {
        int r = ioctl(sock, SIOCSIWSCAN, &wrq1);
        if (r == 0) {
            break;
        }
        sleep(1);
    }

    unsigned int buflen = IW_SCAN_MAX_DATA * 10;
    char* scanbuffer = new char[buflen];
    struct iwreq scanwrq;
    int scansock = socket(AF_INET, SOCK_DGRAM, 0);
    strncpy(scanwrq.ifr_name, "wlan0", IFNAMSIZ);
    scanwrq.ifr_name[IFNAMSIZ - 1] = '\0';
    scanwrq.u.data.pointer = scanbuffer;
    scanwrq.u.data.length = buflen;
    scanwrq.u.data.flags = 0;
    tryN = 10;
    while (tryN--) {
        int r = ioctl(scansock, SIOCGIWSCAN, &scanwrq);
        if (r == 0) {
            parseResults((iw_event*)scanwrq.u.data.pointer, scanwrq.u.data.length, wlanMap);
#ifdef WIFI_LOG
            for (auto&& i : wlanMap) {
                qDebug("WiFi SSID: %s Signal: %d", i.first.c_str(), i.second);
            }
#endif
            break;
        } else {
#ifdef WIFI_LOG
            qDebug("扫描WiFi失败");
#endif
        }
        sleep(1);
    }
    delete[] scanbuffer;
    close(sock);
    close(scansock);
}

// Show the current WiFi connection status
void WiFiManage::showCurrentWiFi() {
#ifdef WIFI_LOG
    qDebug("显示当前WiFi连接状态");
#endif
    std::string status = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 status");
#ifdef WIFI_LOG
    qDebug("%s", status.c_str());
#endif
}

// Get the currently connected WiFi SSID
std::string WiFiManage::getCurrentWiFiSSID() {
    std::string status = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 status");
    std::istringstream iss(status);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("ssid=") == 0) {
            return line.substr(5);
        }
    }
    return "";
}

bool WiFiManage::isWifiConnected() {
    // 1) wpa_state 必须 COMPLETED
    std::string st;
    try {
        st = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 status 2>/dev/null");
    } catch (...) {
        return false;
    }

    if (st.find("wpa_state=COMPLETED") == std::string::npos)
        return false;

    // 2) 必须真实链路 UP（避免 wlan0 DOWN 但 inet 残留导致误判）
    if (!_wlan0LinkLowerUp(this))
        return false;

    // 3) 必须真的有 inet 地址（ip addr 上）
    std::string ip;
    try {
        ip = executeCommand("ip -4 addr show dev wlan0 2>/dev/null || /sbin/ip -4 addr show dev wlan0 2>/dev/null || /usr/sbin/ip -4 addr show dev wlan0 2>/dev/null");
    } catch (...) {
        return false;
    }

    return (ip.find("inet ") != std::string::npos);
}

// =====================
// 你要的：hasSavedWiFiNetwork()
// =====================
bool WiFiManage::hasSavedWiFiNetwork() {
    // wpa_supplicant 刚起来时 control socket 可能还没 ready，做几次重试
    for (int i = 0; i < 5; ++i) {
        std::string out;
        try {
            out = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 list_networks 2>/dev/null");
        } catch (...) {
            out.clear();
        }

        if (!out.empty()) {
            std::istringstream iss(out);
            std::string line;
            int lineNo = 0;
            int netCnt = 0;

            while (std::getline(iss, line)) {
                if (line.empty())
                    continue;

                // 有的会多一行：Selected interface 'wlan0'
                if (line.find("Selected interface") != std::string::npos)
                    continue;

                lineNo++;
                // 表头行
                if (lineNo == 1 && line.find("network id") != std::string::npos)
                    continue;

                // 只要出现一行记录，就认为存在已保存网络
                // 典型：0\tSSID\tany\t[CURRENT]
                if (!line.empty()) {
                    netCnt++;
                }
            }

            return (netCnt > 0);
        }

        usleep(200 * 1000);
    }

    return false;
}
bool WiFiManage::needDhcp() {
    // 1) wpa 必须 COMPLETED
    std::string st;
    try {
        st = executeCommand(
            "wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 status 2>/dev/null");
    } catch (...) {
        return false;
    }

    if (st.find("wpa_state=COMPLETED") == std::string::npos)
        return false;

    // 2) 真实链路 UP
    if (!_wlan0LinkLowerUp(this))
        return false;

    // 3) 是否已有 IPv4
    std::string ip;
    try {
        ip = executeCommand(
            "ip -4 addr show dev wlan0 2>/dev/null");
    } catch (...) {
        return false;
    }

    // 没有 inet，才需要 dhcpc
    return (ip.find("inet ") == std::string::npos);
}
void WiFiManage::autoDhcpIfNeeded() {
    if (!needDhcp())
        return;

    qDebug("[WiFi] need DHCP, start udhcpc");

    // 先清理残留（你已经有）
    _cleanupWlan0NetState(this);

    // 跑 DHCP
    executeCommand("udhcpc -i wlan0 -n -q -T 3 -t 8");

    // 再次确认
    if (isWifiConnected()) {
        qDebug("[WiFi] DHCP success");
    } else {
        qDebug("[WiFi] DHCP failed");
        // ❗这里什么都不要做，等下一次状态变化
    }
}