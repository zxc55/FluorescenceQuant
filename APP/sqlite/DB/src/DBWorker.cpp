#include "DBWorker.h"

#include <QDebug>
#include <QMetaObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

#include "Migrations.h"
#include "SettingsRepo.h"
#include "UsersRepo.h"

DBWorker::DBWorker(const QString& dbPath, QObject* parent)
    : QObject(parent), dbPath_(dbPath) {
    running_.store(true);                            // 标记运行
    th_ = std::thread(&DBWorker::threadLoop, this);  // 启动线程
}

DBWorker::~DBWorker() {
    running_.store(false);  // 停止
    cv_.notify_all();       // 唤醒
    if (th_.joinable())
        th_.join();  // 等待退出
}

void DBWorker::postEnsureAllSchemas() {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push({DBTaskType::EnsureAllSchemas});
    }
    cv_.notify_one();
}
void DBWorker::postLoadSettings() {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push({DBTaskType::LoadSettings});
    }
    cv_.notify_one();
}
void DBWorker::postUpdateSettings(const AppSettingsRow& row) {
    {
        std::lock_guard<std::mutex> lk(m_);
        pendingSettings_ = row;
        hasPendingSettings_ = true;
        q_.push({DBTaskType::UpdateSettings});
    }
    cv_.notify_one();
}

// 用户任务入队
void DBWorker::postAuthLogin(const QString& u, const QString& p) {
    DBTask t;
    t.type = DBTaskType::AuthLogin;
    t.username = u;
    t.password = p;
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(t);
    }
    cv_.notify_one();
}
void DBWorker::postLoadUsers() {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push({DBTaskType::LoadUsers});
    }
    cv_.notify_one();
}
void DBWorker::postAddUser(const QString& u, const QString& p, const QString& role, const QString& note) {
    DBTask t;
    t.type = DBTaskType::AddUser;
    t.username = u;
    t.password = p;
    t.roleName = role;
    t.note = note;
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(t);
    }
    cv_.notify_one();
}
void DBWorker::postDeleteUser(const QString& u) {
    DBTask t;
    t.type = DBTaskType::DeleteUser;
    t.username = u;
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(t);
    }
    cv_.notify_one();
}
void DBWorker::postResetPassword(const QString& u, const QString& p) {
    DBTask t;
    t.type = DBTaskType::ResetPassword;
    t.username = u;
    t.password = p;
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(t);
    }
    cv_.notify_one();
}

void DBWorker::threadLoop() {
    if (!openDatabaseInThisThread()) {
        QMetaObject::invokeMethod(this, [this] { emit errorOccurred("打开数据库失败"); }, Qt::QueuedConnection);
        return;
    }
    if (!execPragmas()) {
        QMetaObject::invokeMethod(this, [this] { emit errorOccurred("设置 PRAGMA 失败"); }, Qt::QueuedConnection);
    }

    while (running_.load()) {
        DBTask t;
        {
            std::unique_lock<std::mutex> lk(m_);
            cv_.wait(lk, [&] { return !running_.load() || !q_.empty(); });
            if (!running_.load())
                break;
            t = q_.front();
            q_.pop();
        }

        switch (t.type) {
        case DBTaskType::EnsureAllSchemas: {
            const bool ok = ensureAllSchemas();
            if (!ok)
                QMetaObject::invokeMethod(this, [this] { emit errorOccurred("迁移/初始化失败"); }, Qt::QueuedConnection);
            break;
        }
        case DBTaskType::LoadSettings: {
            AppSettingsRow r;
            const bool ok = loadSettingsInternal(r);
            if (!ok)
                QMetaObject::invokeMethod(this, [this] { emit errorOccurred("加载设置失败"); }, Qt::QueuedConnection);
            else
                QMetaObject::invokeMethod(this, [this, r] { emit settingsLoaded(r); }, Qt::QueuedConnection);
            break;
        }
        case DBTaskType::UpdateSettings: {
            AppSettingsRow row;
            {
                std::lock_guard<std::mutex> lk(m_);
                row = pendingSettings_;
                hasPendingSettings_ = false;
            }
            const bool ok = updateSettingsInternal(row);
            QMetaObject::invokeMethod(this, [this, ok] { emit settingsUpdated(ok); }, Qt::QueuedConnection);
            if (ok) {
                AppSettingsRow r2;
                if (loadSettingsInternal(r2))
                    QMetaObject::invokeMethod(this, [this, r2] { emit settingsLoaded(r2); }, Qt::QueuedConnection);
            }
            break;
        }
        case DBTaskType::AuthLogin: {
            QString role;
            const bool ok = authInternal(t.username, t.password, role);
            QMetaObject::invokeMethod(this, [this, ok, u = t.username, role] { emit authResult(ok, u, role); }, Qt::QueuedConnection);
            break;
        }
        case DBTaskType::LoadUsers: {
            QVector<UserRow> out;
            const bool ok = loadUsersInternal(out);
            if (!ok)
                QMetaObject::invokeMethod(this, [this] { emit errorOccurred("读取用户失败"); }, Qt::QueuedConnection);
            else
                QMetaObject::invokeMethod(this, [this, out] { emit usersLoaded(out); }, Qt::QueuedConnection);
            break;
        }
        case DBTaskType::AddUser: {
            const bool ok = addUserInternal(t.username, t.password, t.roleName, t.note);
            QMetaObject::invokeMethod(this, [this, ok, u = t.username] { emit userAdded(ok, u); }, Qt::QueuedConnection);
            if (ok) {
                QVector<UserRow> out;
                if (loadUsersInternal(out))
                    QMetaObject::invokeMethod(this, [this, out] { emit usersLoaded(out); }, Qt::QueuedConnection);
            }
            break;
        }
        case DBTaskType::DeleteUser: {
            const bool ok = deleteUserInternal(t.username);
            QMetaObject::invokeMethod(this, [this, ok, u = t.username] { emit userDeleted(ok, u); }, Qt::QueuedConnection);
            if (ok) {
                QVector<UserRow> out;
                if (loadUsersInternal(out))
                    QMetaObject::invokeMethod(this, [this, out] { emit usersLoaded(out); }, Qt::QueuedConnection);
            }
            break;
        }
        case DBTaskType::ResetPassword: {
            const bool ok = resetPasswordInternal(t.username, t.password);
            QMetaObject::invokeMethod(this, [this, ok, u = t.username] { emit passwordReset(ok, u); }, Qt::QueuedConnection);
            break;
        }
        }
    }

    closeDatabaseInThisThread();
}

bool DBWorker::openDatabaseInThisThread() {
    connName_ = QStringLiteral("conn_%1").arg(reinterpret_cast<qulonglong>(QThread::currentThreadId()));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName_);
    db.setDatabaseName(dbPath_);
    if (!db.open()) {
        qWarning() << "[DB] open fail:" << db.lastError().text();
        return false;
    }
    return true;
}
void DBWorker::closeDatabaseInThisThread() {
    QSqlDatabase db = QSqlDatabase::database(connName_, false);
    if (db.isValid()) {
        db.close();
        QSqlDatabase::removeDatabase(connName_);
    }
}
bool DBWorker::execPragmas() {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    QSqlQuery q(db);
    bool ok = true;
    ok = ok && q.exec("PRAGMA journal_mode=WAL;");  // WAL 日志
    ok = ok && q.exec("PRAGMA synchronous=FULL;");  // FULL 更抗断电（也可用 NORMAL）
    return ok;
}
bool DBWorker::ensureAllSchemas() {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return migrateAllToV1(db);
}
bool DBWorker::loadSettingsInternal(AppSettingsRow& out) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return SettingsRepo::selectOne(db, out);
}
bool DBWorker::updateSettingsInternal(const AppSettingsRow& in) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return SettingsRepo::updateOne(db, in);
}
bool DBWorker::authInternal(const QString& u, const QString& p, QString& outRole) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return UsersRepo::auth(db, u, p, outRole);
}
bool DBWorker::loadUsersInternal(QVector<UserRow>& out) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return UsersRepo::selectAll(db, out);
}
bool DBWorker::addUserInternal(const QString& u, const QString& p, const QString& role, const QString& note) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return UsersRepo::addUser(db, u, p, role, note);
}
bool DBWorker::deleteUserInternal(const QString& u) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return UsersRepo::deleteUser(db, u);
}
bool DBWorker::resetPasswordInternal(const QString& u, const QString& p) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return UsersRepo::resetPassword(db, u, p);
}
