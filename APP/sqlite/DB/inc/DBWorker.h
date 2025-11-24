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

signals:
    void ready();
    void errorOccurred(const QString& msg);

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
