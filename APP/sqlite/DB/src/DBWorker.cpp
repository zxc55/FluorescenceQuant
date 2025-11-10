#include "DBWorker.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

#include "DBTasks.h"
#include "Migrations.h"
#include "Migrations.h"    // migrateAllToV1(QSqlDatabase)
#include "ProjectsRepo.h"  // ProjectsRepo::selectAll / deleteById
#include "SettingsRepo.h"  // SettingsRepo::selectOne / updateOne
#include "UsersRepo.h"     // UsersRepo::auth / selectAll / addUser / deleteUser / resetPassword
// ========== 构造 / 析构 ==========

DBWorker::DBWorker(const QString& dbPath, QObject* parent)
    : QObject(parent), dbPath_(dbPath) {
}

DBWorker::~DBWorker() {
    running_.store(false);
    cv_.notify_all();
    if (th_.joinable())
        th_.join();
}

// 仅在工作线程中调用（main.cpp 里通过 QMetaObject::invokeMethod 触发）
void DBWorker::initialize() {
    // 不在这个 QThread 里开库了；只负责启动内部 std::thread
    qInfo() << "[DB] initialize() on thread =" << QThread::currentThread();

    running_.store(true);
    th_ = std::thread([this]() {
        qInfo() << "[DB] worker thread started";

        // 在内部线程里创建专属连接（线程绑定）
        if (!openDatabaseInThisThread()) {
            emit errorOccurred(QStringLiteral("open db failed"));
            running_.store(false);
            return;
        }
        if (!execPragmas()) {
            // 可忽略，不退出
            qWarning() << "[DB] execPragmas failed (ignored)";
        }
        if (!ensureAllSchemas()) {
            emit errorOccurred(QStringLiteral("ensureAllSchemas 失败"));
            // 也可以选择 return，这里继续；
        }

        emit ready();  // ← 只有内部线程发一次 ready

        // 进入主循环
        while (running_.load()) {
            DBTask task;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [&] { return !q_.empty() || !running_.load(); });
                if (!running_.load())
                    break;
                task = std::move(q_.front());
                q_.pop();
            }

            switch (task.type) {
            case DBTaskType::EnsureAllSchemas: {
                ensureAllSchemas();
                break;
            }
            case DBTaskType::LoadSettings: {
                AppSettingsRow row;
                const bool ok = loadSettingsInternal(row);
                ok ? emit settingsLoaded(row)
                   : emit errorOccurred(QStringLiteral("加载设置失败"));
                break;
            }
            case DBTaskType::UpdateSettings: {
                const bool ok = updateSettingsInternal(task.settings);
                emit settingsUpdated(ok);
                break;
            }
            // ==== 用户 ====
            case DBTaskType::AuthLogin: {
                QString role;
                const bool ok = authInternal(task.s1, task.s2, role);
                emit authResult(ok, task.s1, role);
                break;
            }
            case DBTaskType::LoadUsers: {
                QVector<UserRow> rows;
                const bool ok = loadUsersInternal(rows);
                ok ? emit usersLoaded(rows)
                   : emit errorOccurred(QStringLiteral("读取用户失败"));
                break;
            }
            case DBTaskType::AddUser: {
                const bool ok = addUserInternal(task.s1, task.s2, task.s3, task.s4);
                emit userAdded(ok, task.s1);
                break;
            }
            case DBTaskType::DeleteUser: {
                const bool ok = deleteUserInternal(task.s1);
                emit userDeleted(ok, task.s1);
                break;
            }
            case DBTaskType::ResetPassword: {
                const bool ok = resetPasswordInternal(task.s1, task.s2);
                emit passwordReset(ok, task.s1);
                break;
            }
            // ==== 项目 ====
            case DBTaskType::LoadProjects: {
                qInfo() << "[DB] do LoadProjects";
                QVector<ProjectRow> rows;
                const bool ok = loadProjectsInternal(rows);
                if (ok) {
                    qInfo() << "[DB] loadProjectsInternal -> rows =" << rows.size();
                    emit projectsLoaded(rows);
                } else {
                    emit errorOccurred(QStringLiteral("读取项目列表失败"));
                }
                break;
            }
            case DBTaskType::DeleteProject: {
                qInfo() << "[DB] do DeleteProject id=" << task.p1;
                const bool ok = deleteProjectInternal(task.p1);
                emit projectDeleted(ok, task.p1);
                break;
            }

            case DBTaskType::LoadHistory: {
                QVector<HistoryRow> rows;
                QSqlDatabase db = QSqlDatabase::database(connName_);
                bool ok = HistoryRepo::selectAll(db, rows);
                emit historyLoaded(rows);
                break;
            }
            case DBTaskType::InsertHistory: {
                QSqlDatabase db = QSqlDatabase::database(connName_);
                bool ok = HistoryRepo::insert(db, task.history);
                emit historyInserted(ok);
                break;
            }

            default:
                qWarning() << "[DB] unknown task:" << int(task.type);
                break;
            }
        }

        closeDatabaseInThisThread();
        qInfo() << "[DB] worker thread stopped";
    });
}
// ========== 任务入队（对外接口） ==========

void DBWorker::postEnsureAllSchemas() {
    {
        std::lock_guard<std::mutex> lk(m_);
        DBTask t;
        t.type = DBTaskType::EnsureAllSchemas;
        q_.push(t);
    }
    cv_.notify_one();
}

void DBWorker::postLoadSettings() {
    {
        std::lock_guard<std::mutex> lk(m_);
        DBTask t;
        t.type = DBTaskType::LoadSettings;
        q_.push(t);
    }
    cv_.notify_one();
}

void DBWorker::postUpdateSettings(const AppSettingsRow& row) {
    {
        std::lock_guard<std::mutex> lk(m_);
        pendingSettings_ = row;
        hasPendingSettings_ = true;
        DBTask t;
        t.type = DBTaskType::UpdateSettings;
        q_.push(t);
    }
    cv_.notify_one();
}

// 用户
void DBWorker::postAuthLogin(const QString& u, const QString& p) {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(DBTask::authLogin(u, p));
    }
    cv_.notify_one();
}

void DBWorker::postLoadUsers() {
    {
        std::lock_guard<std::mutex> lk(m_);
        DBTask t;
        t.type = DBTaskType::LoadUsers;
        q_.push(t);
    }
    cv_.notify_one();
}

void DBWorker::postAddUser(const QString& u, const QString& p, const QString& role, const QString& note) {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(DBTask::addUser(u, p, role, note));
    }
    cv_.notify_one();
}

void DBWorker::postDeleteUser(const QString& u) {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(DBTask::deleteUser(u));
    }
    cv_.notify_one();
}

void DBWorker::postResetPassword(const QString& u, const QString& p) {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(DBTask::resetPassword(u, p));
    }
    cv_.notify_one();
}

// 项目
void DBWorker::postLoadProjects() {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(DBTask::loadProjects());
    }
    cv_.notify_one();
}

void DBWorker::postDeleteProject(int id) {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(DBTask::deleteProject(id));
    }
    cv_.notify_one();
}
static const char* taskName(DBTaskType t) {
    switch (t) {
    case DBTaskType::EnsureAllSchemas:
        return "EnsureAllSchemas";
    case DBTaskType::LoadSettings:
        return "LoadSettings";
    case DBTaskType::UpdateSettings:
        return "UpdateSettings";
    case DBTaskType::AuthLogin:
        return "AuthLogin";
    case DBTaskType::LoadUsers:
        return "LoadUsers";
    case DBTaskType::AddUser:
        return "AddUser";
    case DBTaskType::DeleteUser:
        return "DeleteUser";
    case DBTaskType::ResetPassword:
        return "ResetPassword";
    case DBTaskType::LoadProjects:
        return "LoadProjects";
    case DBTaskType::DeleteProject:
        return "DeleteProject";
    default:
        return "Unknown";
    }
}
// ========== 线程主循环 ==========
void DBWorker::threadLoop() {
}

bool DBWorker::openDatabaseInThisThread() {
    // 确保目录存在
    QFileInfo fi(dbPath_);
    QDir dir = fi.dir();
    if (!dir.exists())
        dir.mkpath(".");

    connName_ = QStringLiteral("conn_%1").arg(reinterpret_cast<qulonglong>(QThread::currentThreadId()));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName_);
    db.setDatabaseName(dbPath_);
    if (!db.open()) {
        qWarning() << "[DB] open fail:" << db.lastError().text();
        return false;
    }
    qInfo() << "[DB] open OK, connName =" << connName_;
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
    QSqlDatabase db = QSqlDatabase::database(connName_, /* open = */ false);
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "[DB] execPragmas: db not open (conn =" << connName_ << ")";
        return false;
    }
    QSqlQuery q(db);
    bool ok = true;
    ok = ok && q.exec("PRAGMA journal_mode=WAL;");
    ok = ok && q.exec("PRAGMA synchronous=FULL;");
    return ok;
}

bool DBWorker::ensureAllSchemas() {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return migrateAllToV1(db);
}

// ========== 内部实际操作（调用 Repo） ==========

// settings
bool DBWorker::loadSettingsInternal(AppSettingsRow& out) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return SettingsRepo::selectOne(db, out);
}

bool DBWorker::updateSettingsInternal(const AppSettingsRow& in) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return SettingsRepo::updateOne(db, in);
}

// users
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

// projects
bool DBWorker::loadProjectsInternal(QVector<ProjectRow>& out) {
    // 取当前线程绑定的连接（确保 initialize() 里已经 addDatabase 并 open）
    QSqlDatabase db = QSqlDatabase::database(connName_);
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "[DB] loadProjectsInternal: db invalid or not open";
        return false;
    }
    return ProjectsRepo::selectAll(db, out);
}

bool DBWorker::deleteProjectInternal(int id) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "[DB] deleteProjectInternal: db invalid or not open";
        return false;
    }
    return ProjectsRepo::deleteById(db, id);
}
void DBWorker::postLoadHistory() {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::loadHistory());
    cv_.notify_one();
}

void DBWorker::postInsertHistory(const HistoryRow& row) {
    {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(DBTask::insertHistory(row));
    }
    cv_.notify_one();
}
