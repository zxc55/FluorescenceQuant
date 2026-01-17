#include "DBWorker.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QThread>

#include "DBTasks.h"
#include "HistoryRepo.h"
#include "Migrations.h"
#include "ProjectsRepo.h"
#include "QrRepo.h"
#include "SettingsRepo.h"
#include "UsersRepo.h"
DBWorker::DBWorker(const QString& dbPath, QObject* parent)
    : QObject(parent), dbPath_(dbPath) {}

DBWorker::~DBWorker() {
    running_.store(false);
    cv_.notify_all();
    if (th_.joinable())
        th_.join();
}

void DBWorker::initialize() {
    qInfo() << "[DB] initialize() on thread =" << QThread::currentThread();
    running_.store(true);

    th_ = std::thread([this]() {
        qInfo() << "[DB] worker thread started";
        if (!openDatabaseInThisThread()) {
            emit errorOccurred("open db failed");
            running_.store(false);
            return;
        }
        execPragmas();
        ensureAllSchemas();
        emit ready();

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
            case DBTaskType::EnsureAllSchemas:
                ensureAllSchemas();
                break;
            case DBTaskType::LoadSettings: {
                AppSettingsRow row;
                bool ok = loadSettingsInternal(row);
                ok ? emit settingsLoaded(row)
                   : emit errorOccurred("加载设置失败");
                break;
            }
            case DBTaskType::UpdateSettings:
                emit settingsUpdated(updateSettingsInternal(task.settings));
                break;
            case DBTaskType::AuthLogin: {
                QString role;
                bool ok = authInternal(task.s1, task.s2, role);
                emit authResult(ok, task.s1, role);
                break;
            }
            case DBTaskType::LoadUsers: {
                QVector<UserRow> rows;
                bool ok = loadUsersInternal(rows);
                ok ? emit usersLoaded(rows)
                   : emit errorOccurred("读取用户失败");
                break;
            }
            case DBTaskType::LoadProjects: {
                QVector<ProjectRow> rows;
                bool ok = loadProjectsInternal(rows);
                ok ? emit projectsLoaded(rows)
                   : emit errorOccurred("读取项目失败");
                break;
            }
            case DBTaskType::InsertProjectInfo: {
                QSqlDatabase db = QSqlDatabase::database(connName_);
                bool ok = ProjectsRepo::insertProjectInfo(db, task.info);
                emit projectInfoInserted(ok);
                break;
            }

            case DBTaskType::DeleteProject:
                emit projectDeleted(deleteProjectInternal(task.p1), task.p1);
                break;
            case DBTaskType::LoadHistory: {
                QVector<HistoryRow> rows;
                bool ok = loadHistoryInternal(rows);
                emit historyLoaded(rows);
                break;
            }
            case DBTaskType::InsertHistory:
                emit historyInserted(insertHistoryInternal(task.history));
                break;
            case DBTaskType::DeleteHistory:
                emit historyDeleted(deleteHistoryInternal(task.p1), task.p1);
                break;
            case DBTaskType::ExportHistory:
                emit historyExported(exportHistoryInternal(task.s1), task.s1);
                break;
            case DBTaskType::LookupQrMethodConfig: {
                QVariantMap out;                                       // 输出
                bool ok = lookupQrMethodConfigInternal(task.s1, out);  // ✅ 内部函数做解析+查库
                out.insert("ok", ok);                                  // 回填 ok（保险）
                emit qrMethodConfigLookedUp(out);                      // 发信号给 UI
                break;
            }
            case DBTaskType::UpsertQrMethodConfig: {
                QString err;
                QSqlDatabase db = QSqlDatabase::database(connName_);
                bool ok = QrRepo::upsert(db, task.info);  // task.info 就是 cfg

                if (!ok) {
                    err = "写入方法配置失败";
                    qWarning() << "[DBWorker] upsert failed" << task.info;
                }

                emit saveQrMethodConfigDone(ok, err);
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

// === 外部任务接口 ===
void DBWorker::postLookupQrMethodConfig(const QString& qrText)  // ✅ 投递“查二维码配置”任务
{
    std::lock_guard<std::mutex> lk(m_);             // 加锁保护队列
    q_.push(DBTask::lookupQrMethodConfig(qrText));  // 入队任务
    cv_.notify_one();                               // 唤醒 worker 线程
}
void DBWorker::postEnsureAllSchemas() {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask{DBTaskType::EnsureAllSchemas});
    cv_.notify_one();
}

void DBWorker::postLoadSettings() {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask{DBTaskType::LoadSettings});
    cv_.notify_one();
}

void DBWorker::postUpdateSettings(const AppSettingsRow& row) {
    std::lock_guard<std::mutex> lk(m_);
    DBTask t;
    t.type = DBTaskType::UpdateSettings;
    t.settings = row;
    q_.push(t);
    cv_.notify_one();
}

// === 用户 ===
void DBWorker::postAuthLogin(const QString& u, const QString& p) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::authLogin(u, p));
    cv_.notify_one();
}
void DBWorker::postLoadUsers() {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask{DBTaskType::LoadUsers});
    cv_.notify_one();
}
void DBWorker::postAddUser(const QString& u, const QString& p, const QString& role, const QString& note) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::addUser(u, p, role, note));
    cv_.notify_one();
}
void DBWorker::postDeleteUser(const QString& u) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::deleteUser(u));
    cv_.notify_one();
}
void DBWorker::postResetPassword(const QString& u, const QString& p) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::resetPassword(u, p));
    cv_.notify_one();
}

// === 项目 ===
void DBWorker::postLoadProjects() {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::loadProjects());
    cv_.notify_one();
}
void DBWorker::postDeleteProject(int id) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::deleteProject(id));
    cv_.notify_one();
}

// === 历史记录 ===
void DBWorker::postLoadHistory() {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::loadHistory());
    cv_.notify_one();
}
void DBWorker::postInsertHistory(const HistoryRow& row) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::insertHistory(row));
    cv_.notify_one();
}
void DBWorker::postDeleteHistory(int id) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::deleteHistory(id));
    cv_.notify_one();
}
void DBWorker::postExportHistory(const QString& path) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::exportHistory(path));
    cv_.notify_one();
}

// === 内部操作实现 ===
bool DBWorker::openDatabaseInThisThread() {
    QFileInfo fi(dbPath_);
    QDir dir = fi.dir();
    if (!dir.exists())
        dir.mkpath(".");
    connName_ = QStringLiteral("conn_%1").arg(reinterpret_cast<qulonglong>(QThread::currentThreadId()));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName_);
    db.setDatabaseName(dbPath_);  // 打开这个文件作为数据库，如果没有就新建
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
    QSqlDatabase db = QSqlDatabase::database(connName_, false);
    if (!db.isValid() || !db.isOpen())
        return false;
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL;");
    q.exec("PRAGMA synchronous=FULL;");
    return true;
}
bool DBWorker::ensureAllSchemas() {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return migrateAllToV1(db);
}

// === 各 Repo 操作 ===
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

bool DBWorker::loadProjectsInternal(QVector<ProjectRow>& out) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return ProjectsRepo::selectAll(db, out);
}
bool DBWorker::deleteProjectInternal(int id) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return ProjectsRepo::deleteById(db, id);
}
bool DBWorker::loadHistoryInternal(QVector<HistoryRow>& out) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return HistoryRepo::selectAll(db, out);
}
bool DBWorker::insertHistoryInternal(const HistoryRow& row) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return HistoryRepo::insert(db, row);
}
bool DBWorker::deleteHistoryInternal(int id) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    return HistoryRepo::deleteById(db, id);
}
bool DBWorker::lookupQrMethodConfigInternal(const QString& qrText, QVariantMap& out)  // ✅ 查二维码配置（内部）
{
    QSqlDatabase db = QSqlDatabase::database(connName_);  // 取本线程连接
    return QrRepo::existsByQrText(db, qrText, out);
}
bool DBWorker::exportHistoryInternal(const QString& csvPath) {
    QSqlDatabase db = QSqlDatabase::database(connName_);
    QVector<HistoryRow> rows;
    if (!HistoryRepo::selectAll(db, rows))
        return false;

    QFile f(csvPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream ts(&f);
    ts << "ID,ProjectId,SampleNo,SampleSource,SampleName,StandardCurve,BatchCode,"
          "DetectedConc,ReferenceValue,Result,DetectedTime,DetectedUnit,DetectedPerson,DilutionInfo\n";

    for (const auto& r : rows) {
        ts << r.id << "," << r.projectId << "," << r.sampleNo << ","
           << r.sampleSource << "," << r.sampleName << "," << r.standardCurve << ","
           << r.batchCode << "," << r.detectedConc << "," << r.referenceValue << ","
           << r.result << "," << r.detectedTime << "," << r.detectedUnit << ","
           << r.detectedPerson << "," << r.dilutionInfo << "\n";
    }
    return true;
}
void DBWorker::postInsertProjectInfo(const QVariantMap& info) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::insertProjectInfo(info));
    cv_.notify_one();
}
void DBWorker::postSaveQrMethodConfig(const QVariantMap& cfg) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(DBTask::upsertQrMethodConfig(cfg));
    cv_.notify_one();
}
