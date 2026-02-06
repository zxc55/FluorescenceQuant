#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#define APP_TITLE "FLA_T113"
#define APP_VERSION "1.0.6"
class OtaManager : public QObject {
    Q_OBJECT
public:
    explicit OtaManager(QObject* parent = nullptr);
    ~OtaManager();
    // QML / UI 调用：检查更新
    Q_INVOKABLE void checkUpdate();
    Q_INVOKABLE void startUpdate();  // 真正升级
    Q_INVOKABLE void startUpdateAsync();
    Q_INVOKABLE void requestStartUpdate();  // 给 QML 调用
    Q_PROPERTY(QString currentVersion READ currentVersion NOTIFY versionChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY versionChanged)

signals:
    void info(const QString& msg);   // 普通提示
    void error(const QString& msg);  // 错误提示
    void finished(bool success);     // 完成回调
    void versionChanged();
    void updateAvailable(QString newVersion);  //
    void updateAvailableChanged();
    void NoUpdate();

private:
    // ===== OTA 流程步骤 =====
    bool loadLocalVersion();  // 读取当前版本
    bool saveLocalVersion();  // 保存当前版本
    bool queryAttributes();   // 查询 TB 共享属性
    bool downloadPackage();   // 下载 OTA 包
    bool verifyPackage();     // 校验 OTA 包
    bool applyUpdate();       // 调用升级脚本
    QString currentVersion() const { return curVersion; }
    QString currentTitle() const { return curTitle; }
    // ===== 工具函数 =====
    QString exec(const QString& cmd);  // 执行 shell 命令

private:
    // ===== 本地当前版本 =====
    QString curTitle;
    QString curVersion;

    // ===== TB 下发的新版本 =====
    QString fwTitle;
    QString fwVersion;
    QString fwChecksum;
    QString fwAlgo;
    bool hasUpdate = false;
    // ===== OTA 工作线程 =====
    std::thread otaThread_;
    std::mutex otaMutex_;
    std::condition_variable otaCv_;
    bool otaTaskPending_ = false;
    std::atomic<bool> otaExit_{false};

    void otaThreadLoop();  // 线程主循环
};
