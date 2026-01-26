#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "DBTasks.h"
#include "DTO.h"
#include "HistoryRepo.h"
struct QrMethodConfigRow  // DB 返回行结构体（对标 ProjectRow）
{
    int id = 0;           // 表主键 id
    QString projectName;  // projectName
    QString batchCode;    // batchCode
    QString updatedAt;
    int C1 = 0;  // C1
    int T1 = 0;  // T1
    int C2 = 0;  // C2
    int T2 = 0;  // T2
    QString methodData;
};
class DBWorker : public QObject {
    Q_OBJECT
public:
    explicit DBWorker(const QString& dbPath, QObject* parent = nullptr);
    ~DBWorker() override;

    // === 初始化 ===
    Q_INVOKABLE void initialize();

    // === 设置 ===
    Q_INVOKABLE void postEnsureAllSchemas();
    Q_INVOKABLE void postLoadSettings();
    Q_INVOKABLE void postUpdateSettings(const AppSettingsRow& row);

    // === 用户 ===
    Q_INVOKABLE void postAuthLogin(const QString& u, const QString& p);
    Q_INVOKABLE void postLoadUsers();
    Q_INVOKABLE void postAddUser(const QString& u, const QString& p, const QString& role, const QString& note);
    Q_INVOKABLE void postDeleteUser(const QString& u);
    Q_INVOKABLE void postResetPassword(const QString& u, const QString& p);

    // === 项目 ===
    Q_INVOKABLE void postLoadProjects();
    Q_INVOKABLE void postDeleteProject(int id);

    // === 历史记录 ===
    Q_INVOKABLE void postLoadHistory();
    Q_INVOKABLE void postInsertHistory(const HistoryRow& row);
    Q_INVOKABLE void postDeleteHistory(int id);
    Q_INVOKABLE void postExportHistory(const QString& csvPath);
    Q_INVOKABLE void postInsertProjectInfo(const QVariantMap& info);
    //
    Q_INVOKABLE void postLookupQrMethodConfig(const QString& qrText);
    Q_INVOKABLE void postSaveQrMethodConfig(const QVariantMap& cfg);

signals:
    void ready();
    void errorOccurred(const QString& msg);

    void projectInfoInserted(bool ok);

    // === Settings ===
    void settingsLoaded(const AppSettingsRow& row);
    void settingsUpdated(bool ok);

    // === Users ===
    void authResult(bool ok, const QString& username, const QString& role);
    void usersLoaded(const QVector<UserRow>& rows);
    void userAdded(bool ok, const QString& username);
    void userDeleted(bool ok, const QString& username);
    void passwordReset(bool ok, const QString& username);

    // === Projects ===
    void projectsLoaded(const QVector<ProjectRow>& rows);
    void projectDeleted(bool ok, int id);

    // === History ===
    void historyLoaded(const QVector<HistoryRow>& rows);
    void historyInserted(bool ok);
    void historyDeleted(bool ok, int id);
    void historyExported(bool ok, const QString& path);
    // === 二维码识别===
    void qrMethodConfigLookedUp(const QVariantMap& result);  // ✅ 查询完成回调：result 里带 ok / error / 字段
    void saveQrMethodConfigDone(bool ok, const QString& err);

    void qrMethodConfigsLoaded(const QVector<QrMethodConfigRow>& rows);  // 加载完成信号
    void qrMethodConfigDeleted(bool ok, int id);                         // 删除完成信号

private:
    // === 内部逻辑 ===
    void threadLoop();
    bool openDatabaseInThisThread();
    void closeDatabaseInThisThread();
    bool execPragmas();
    bool ensureAllSchemas();

    // === 内部操作 ===
    bool loadSettingsInternal(AppSettingsRow& out);
    bool updateSettingsInternal(const AppSettingsRow& in);

    bool authInternal(const QString& u, const QString& p, QString& outRole);
    bool loadUsersInternal(QVector<UserRow>& out);

    bool loadProjectsInternal(QVector<ProjectRow>& out);
    bool deleteProjectInternal(int id);

    // === 历史记录 ===
    bool loadHistoryInternal(QVector<HistoryRow>& out);
    bool insertHistoryInternal(const HistoryRow& row);
    bool deleteHistoryInternal(int id);
    bool exportHistoryInternal(const QString& csvPath);
    // === 二维码识别===
    bool lookupQrMethodConfigInternal(const QString& qrText, QVariantMap& out);  // ✅ 内部：查 qr_method_config
public slots:
    void postLoadQrMethodConfigs();                              // 对外：投递加载任务（你项目里一般用 invokeMethod）
    void postDeleteQrMethodConfig(int id);                       // 对外：投递删除任务
    void postInsertQrMethodConfigInfo(const QVariantMap& info);  // 对外：投递插入任务
private slots:
    void doLoadQrMethodConfigs();                              // 真正执行 SQL 的函数（在 DB 线程）
    void doDeleteQrMethodConfig(int id);                       // 真正执行删除
    void doInsertQrMethodConfigInfo(const QVariantMap& info);  // 真正执行插入
private:
    QString dbPath_;
    QString connName_;

    std::thread th_;
    std::atomic<bool> running_{false};

    std::mutex m_;
    std::condition_variable cv_;
    std::queue<DBTask> q_;

    AppSettingsRow pendingSettings_;
    bool hasPendingSettings_ = false;
};
