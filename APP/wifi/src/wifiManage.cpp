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
// #include "src/threads/errorProcess/errorProcess.h"
// #include "src/threads/log/json_logger.hpp"
#define WIFI_LOG
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
    qDebug("执行命令: %s 结果: %s", command.c_str(), result.c_str());
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
                wlanMap[lastSSID] = 0;  // Initialize with 0 signal strength
            }

        } else if (iwe->cmd == IWEVQUAL && !lastSSID.empty()) {
            // Extract signal quality (example: convert to dBm)
            int signalStrength = iwe->u.qual.level;
            wlanMap[lastSSID] = signalStrength;  // Set signal for the last SSID
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
    return "";  // Not found
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
    // executeCommand("ifconfig wlan0 up; wpa_supplicant -D wext -c /etc/wpa_supplicant.conf -i wlan0 -B; udhcpc -i wlan0");
    executeCommand("killall udhcpc; ifconfig wlan0 up; wpa_supplicant -D wext -c /etc/wpa_supplicant.conf -i wlan0 -B;");
}

// Turn off WiFi (bring down the wlan0 interface)
void WiFiManage::turnOffWiFi() {
#ifdef WIFI_LOG
    qDebug("关闭WiFi");
#endif
    executeCommand("ifconfig wlan0 down; killall wpa_supplicant;udhcpc -i eth1 -n -q -T 3 -t 8");
}

// Disconnect from the current WiFi
void WiFiManage::disconnectWiFi() {
#ifdef WIFI_LOG
    qDebug("断开WiFi");
#endif
    executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 disconnect; udhcpc -i eth1 -n -q -T 3 -t 8");
}

int WiFiManage::connectToWiFi(const std::string& ssid) {
    std::string networkId = findNetworkId(ssid);
    if (!networkId.empty()) {
#ifdef WIFI_LOG
        qDebug("Network found. Network ID: %s", networkId.c_str());
#endif
        // Enable and connect
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
                executeCommand("udhcpc -i wlan0 -n -q -T 3 -t 8");
#ifdef WIFI_LOG
                qDebug("连接WiFi成功 %s", ssid.c_str());
#endif
                return 0;
            }
            // 这两种可以认为“立即失败/不会自己变好”
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
#ifdef WIFI_LOG
        //  LOG_WARN("连接WiFi失败: 未找到网络 {}", ssid);
#endif
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
    // Check if the network already exists
    std::string networkId = findNetworkId(ssid);
    if (!networkId.empty()) {
#ifdef WIFI_LOG
        qDebug("Network found. Network ID: %s", networkId.c_str());
#endif
        if (!password.empty()) {
            // Update the password if provided
            std::string setPSK = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 set_network " + networkId +
                                                " psk '\"" +
                                                password +
                                                "\"'");
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
        // Add new network
        networkId = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 add_network");
        networkId = networkId.substr(0, networkId.find_first_of("\n"));

        if (networkId.empty()) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 添加网络失败");
#endif
            return -1;
        }

        // Set SSID and password
        std::string setSSID = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 set_network " + networkId +
                                             " ssid '\"" +
                                             ssid +
                                             "\"'");
        if (setSSID.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 设置SSID失败");
#endif
            removeNetwork(networkId);
            return -1;
        }

        std::string setPSK = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 set_network " + networkId +
                                            " psk '\"" +
                                            password +
                                            "\"'");
        if (setPSK.find("OK") == std::string::npos) {
#ifdef WIFI_LOG
            qDebug("连接WiFi失败: 设置密码失败");
#endif
            removeNetwork(networkId);
            return -2;
        }
        // Enable and connect
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

    //     // Enable and connect
    //     std::string enable = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 enable_network " + networkId);
    //     if (enable.find("OK") == std::string::npos) {
    // #ifdef WIFI_LOG
    //         qDebug("连接WiFi失败: 启用网络失败");
    // #endif
    //         removeNetwork(networkId);
    //         return -1;
    //     }

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
        // 这两种可以认为“立即失败/不会自己变好”
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
    // int sock = socket(AF_INET, SOCK_STREAM, 0);
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int tryN = 10;
    while (tryN--) {
        int r = ioctl(sock, SIOCSIWSCAN, &wrq1);
        if (r == 0) {
            break;
        }
        sleep(1);
#ifdef WIFI_LOG
        //   LOG_WARN("扫描WiFi失败, 重试次数: {}", tryN);
#endif
    }

    unsigned int buflen = IW_SCAN_MAX_DATA * 10;  // TODO
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
            return line.substr(5);  // Extract the SSID value
        }
    }
    return "";  // Return empty string if no SSID is found
}
bool WiFiManage::isWifiConnected() {
    std::string st;
    try {
        st = executeCommand("wpa_cli -p /tmp/lock/wpa_supplicant -i wlan0 status 2>/dev/null");
    } catch (...) {
        return false;
    }

    if (st.find("wpa_state=COMPLETED") == std::string::npos)
        return false;

    std::string ip;
    try {
        ip = executeCommand("ip -4 addr show dev wlan0 2>/dev/null || /sbin/ip -4 addr show dev wlan0 2>/dev/null || /usr/sbin/ip -4 addr show dev wlan0 2>/dev/null");
    } catch (...) {
        return false;
    }

    return (ip.find("inet ") != std::string::npos);
}
