#ifndef DBWORKER_H_
#define DBWORKER_H_
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "DBTasks.h"
#include "DTO.h"

class DBWorker : public QObject {
    Q_OBJECT
public:
    explicit DBWorker(const QString& dbPath, QObject* parent = nullptr);
    ~DBWorker();

    // —— 任务投递（线程安全）——
    Q_INVOKABLE void postEnsureAllSchemas();                         // 建表/默认数据
    Q_INVOKABLE void postLoadSettings();                             // 读设置
    Q_INVOKABLE void postUpdateSettings(const AppSettingsRow& row);  // 写设置

    // 用户相关任务
    Q_INVOKABLE void postAuthLogin(const QString& u, const QString& p);                                          // 登录
    Q_INVOKABLE void postLoadUsers();                                                                            // 列出
    Q_INVOKABLE void postAddUser(const QString& u, const QString& p, const QString& role, const QString& note);  // 新增
    Q_INVOKABLE void postDeleteUser(const QString& u);                                                           // 删除
    Q_INVOKABLE void postResetPassword(const QString& u, const QString& p);                                      // 重置

signals:
    // 设置回调
    void settingsLoaded(const AppSettingsRow& row);
    void settingsUpdated(bool ok);
    void errorOccurred(const QString& msg);

    // 用户回调
    void authResult(bool ok, const QString& username, const QString& role);
    void usersLoaded(const QVector<UserRow>& users);
    void userAdded(bool ok, const QString& username);
    void userDeleted(bool ok, const QString& username);
    void passwordReset(bool ok, const QString& username);

private:
    void threadLoop();                 // 线程主函数
    bool openDatabaseInThisThread();   // 本线程创建连接
    void closeDatabaseInThisThread();  // 关闭连接
    bool execPragmas();                // PRAGMA
    // 设置内部
    bool ensureAllSchemas();                             // 迁移/初始化
    bool loadSettingsInternal(AppSettingsRow& out);      // 读
    bool updateSettingsInternal(const AppSettingsRow&);  // 写
    // 用户内部
    bool authInternal(const QString& u, const QString& p, QString& outRole);
    bool loadUsersInternal(QVector<UserRow>& out);
    bool addUserInternal(const QString& u, const QString& p, const QString& role, const QString& note);
    bool deleteUserInternal(const QString& u);
    bool resetPasswordInternal(const QString& u, const QString& p);

private:
    QString dbPath_;                    // DB 文件路径
    QString connName_;                  // 连接名（线程内唯一）
    std::thread th_;                    // 后台线程
    std::atomic<bool> running_{false};  // 运行标志
    std::mutex m_;                      // 队列锁
    std::condition_variable cv_;        // 条件变量
    std::queue<DBTask> q_;              // 任务队列
    AppSettingsRow pendingSettings_;    // 等待写入的设置
    bool hasPendingSettings_ = false;   // 标记
};
#endif  // DBWORKER_H_