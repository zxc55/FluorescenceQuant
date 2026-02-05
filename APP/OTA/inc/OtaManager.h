#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class OtaManager : public QObject {
    Q_OBJECT
public:
    explicit OtaManager(QObject* parent = nullptr);

    // QML / UI 调用：检查更新
    Q_INVOKABLE void checkUpdate();
    Q_PROPERTY(QString currentVersion READ currentVersion NOTIFY versionChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY versionChanged)
signals:
    void info(const QString& msg);   // 普通提示
    void error(const QString& msg);  // 错误提示
    void finished(bool success);     // 完成回调
    void versionChanged();

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
};
