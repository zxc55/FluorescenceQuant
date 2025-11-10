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

class DBWorker : public QObject {
    Q_OBJECT
public:
    explicit DBWorker(const QString& dbPath, QObject* parent = nullptr);
    ~DBWorker() override;

    // 线程启动后的初始化（在工作线程内调用）
    Q_INVOKABLE void initialize();

    // ========== 任务入队（对外接口） ==========
    Q_INVOKABLE void postEnsureAllSchemas();
    Q_INVOKABLE void postLoadSettings();
    Q_INVOKABLE void postUpdateSettings(const AppSettingsRow& row);

    // 用户
    Q_INVOKABLE void postAuthLogin(const QString& u, const QString& p);
    Q_INVOKABLE void postLoadUsers();
    Q_INVOKABLE void postAddUser(const QString& u, const QString& p, const QString& role, const QString& note);
    Q_INVOKABLE void postDeleteUser(const QString& u);
    Q_INVOKABLE void postResetPassword(const QString& u, const QString& p);

    // **项目**
    Q_INVOKABLE void postLoadProjects();
    Q_INVOKABLE void postDeleteProject(int id);
    // *History**
    Q_INVOKABLE void postLoadHistory();
    Q_INVOKABLE void postInsertHistory(const HistoryRow& row);

signals:
    void ready();
    void errorOccurred(const QString& msg);

    // settings
    void settingsLoaded(const AppSettingsRow& row);
    void settingsUpdated(bool ok);

    // users
    void authResult(bool ok, const QString& username, const QString& role);
    void usersLoaded(const QVector<UserRow>& rows);
    void userAdded(bool ok, const QString& username);
    void userDeleted(bool ok, const QString& username);
    void passwordReset(bool ok, const QString& username);

    // **projects**
    void projectsLoaded(const QVector<ProjectRow>& rows);
    void projectDeleted(bool ok, int id);
    // *History**
    void historyLoaded(const QVector<HistoryRow>& rows);
    void historyInserted(bool ok);

private:
    // 线程主循环
    void threadLoop();

    // 数据库生命周期仅在该线程里使用
    bool openDatabaseInThisThread();
    void closeDatabaseInThisThread();
    bool execPragmas();
    bool ensureAllSchemas();

    // 内部实际操作
    bool loadSettingsInternal(AppSettingsRow& out);
    bool updateSettingsInternal(const AppSettingsRow& in);

    bool authInternal(const QString& u, const QString& p, QString& outRole);
    bool loadUsersInternal(QVector<UserRow>& out);
    bool addUserInternal(const QString& u, const QString& p, const QString& role, const QString& note);
    bool deleteUserInternal(const QString& u);
    bool resetPasswordInternal(const QString& u, const QString& p);

    // **projects**
    bool loadProjectsInternal(QVector<ProjectRow>& out);
    bool deleteProjectInternal(int id);

private:
    QString dbPath_;
    QString connName_;

    std::thread th_;
    std::atomic<bool> running_{false};

    std::mutex m_;
    std::condition_variable cv_;
    std::queue<DBTask> q_;

    // settings pending
    AppSettingsRow pendingSettings_;
    bool hasPendingSettings_ = false;
};
