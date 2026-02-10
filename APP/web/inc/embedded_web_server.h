#ifndef EMBEDDED_WEB_SERVER_H_
#define EMBEDDED_WEB_SERVER_H_

#include <memory>
#include <string>
#include <vector>

struct sqlite3;

namespace embedded {

/**
 * @brief 网卡信息（仅 Linux IPv4）。
 */
struct NetworkInterfaceInfo {
    std::string name;          ///< 网卡名称，例如 wlan0/eth0。
    std::string ipv4;          ///< IPv4 地址。
    bool is_up = false;        ///< 网卡是否处于 UP 状态。
    bool is_loopback = false;  ///< 是否回环网卡。
};

/**
 * @brief 嵌入式 Web 服务器配置。
 */
struct ServerConfig {
    std::string host;            ///< 监听地址，默认 0.0.0.0。
    int port = 8080;             ///< 监听端口。
    std::string web_root;        ///< 静态资源目录。
    std::string browse_root;     ///< 文件浏览根目录（当前版本仅作为运行信息输出）。
    std::string db_path;         ///< SQLite 文件路径。
    std::string bind_interface;  ///< 绑定网卡名（可选）。
};

/**
 * @brief 轻量嵌入式 Web 服务器。
 *
 * 提供：
 * - 项目列表接口 `/api/project/list`
 * - 检测详情接口 `/api/detect/detail`
 * - 曲线接口 `/api/detect/curve`
 * - 运行信息接口 `/api/server/runtime`
 * - 网卡信息接口 `/api/server/interfaces`
 */
class EmbeddedWebServer {
public:
    /**
     * @brief 返回默认配置。
     */
    static ServerConfig defaultConfig();

    /**
     * @brief 列出可用网卡（Linux IPv4）。
     */
    static std::vector<NetworkInterfaceInfo> listNetworkInterfaces();

    /**
     * @brief 构造服务器实例。
     */
    explicit EmbeddedWebServer(ServerConfig config = defaultConfig());

    ~EmbeddedWebServer();

    EmbeddedWebServer(const EmbeddedWebServer&) = delete;
    EmbeddedWebServer& operator=(const EmbeddedWebServer&) = delete;

    /**
     * @brief 获取可修改配置。
     */
    ServerConfig& mutableConfig();

    /**
     * @brief 获取当前配置。
     */
    const ServerConfig& config() const;

    /**
     * @brief 根据网卡名绑定监听地址。
     * @param iface_name 网卡名，如 wlan0/eth0。
     * @param err 失败时返回中文错误。
     */
    bool bindNetworkInterface(const std::string& iface_name, std::string& err);

    /**
     * @brief 启动服务器（非阻塞）。
     * @param err 失败时返回中文错误。
     */
    bool start(std::string& err);

    /**
     * @brief 停止服务器并回收线程。
     */
    void stop();

    /**
     * @brief 等待服务器线程退出。
     */
    void wait();

    /**
     * @brief 查询服务器是否处于运行状态。
     */
    bool isRunning() const;

    /**
     * @brief 打开只读 DB 句柄（调用方负责 sqlite3_close）。
     */
    sqlite3* openDbHandleReadonly(std::string& err) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace embedded

#endif  // EMBEDDED_WEB_SERVER_H_
