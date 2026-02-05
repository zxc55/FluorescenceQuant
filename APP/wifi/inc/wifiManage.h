#ifndef _WIFIMANAGE_H_
#define _WIFIMANAGE_H_
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <string>
#include <unordered_map>

#include "linux/wireless.h"

class WiFiManage {
private:
    // Helper function to execute a command and capture the output
    std::string executeCommand(const std::string& command);

    void parseResults(iw_event* event, int length, std::unordered_map<std::string, int>& wlanMap);
    // Check if a network with the given SSID already exists
    std::string findNetworkId(const std::string& ssid);
    // Remove a network by ID
    void removeNetwork(const std::string& networkId);

public:
    // Turn on WiFiManage (bring up the wlan0 interface)
    void turnOnWiFi();

    // Turn off WiFiManage (bring down the wlan0 interface)
    void turnOffWiFi();

    // Disconnect from the current WiFiManage
    void disconnectWiFi();

    int connectToWiFi(const std::string& ssid);

    int connectToWiFi(const std::string& ssid, const std::string& password);
    // Scan for available WiFiManage networks
    void scanWiFi(std::unordered_map<std::string, int>& wlanMap);
    // Show the current WiFiManage connection status
    void showCurrentWiFi();
    // Get the currently connected WiFi SSID
    std::string getCurrentWiFiSSID();
    void statrt_uchcpc(int timeout, int num_retries);
};
#endif  // _WIFIMANAGE_H_