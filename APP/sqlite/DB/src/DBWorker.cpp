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
// void DBWorker::postLoadQrMethodConfigs() {
//     QMetaObject::invokeMethod(this, "doLoadQrMethodConfigs",  // 投递到 DBWorker 所在线程
//                               Qt::QueuedConnection);          // 队列连接，线程安全
// }
// void DBWorker::doLoadQrMethodConfigs() {
//     QVector<QrMethodConfigRow> rows;  // 返回结果列表

//     QSqlDatabase db = QSqlDatabase::database(connName_);          // 获取连接（必须是 DB 线程创建/打开的连接）
//     if (!db.isValid() || !db.isOpen()) {                          // 检查连接
//         qWarning() << "[DB] doLoadQrMethodConfigs: db not open";  // 打印告警
//         emit qrMethodConfigsLoaded(rows);                         // 发空结果
//         return;                                                   // 结束
//     }

//     QSqlQuery q(db);  // 创建查询对象
//     const QString sql =
//         "SELECT "                 // 查询开始
//         "  id, "                  // 主键 id（给 VM 的 rid 用）
//         "  projectName, "         // 项目名称
//         "  batchCode, "           // 批次编码
//         "  methodData "           // 方法数据（你要求显示到“更新时间”列）
//         "FROM qr_method_config "  // 表名
//         "ORDER BY id DESC";       // 倒序

//     if (!q.exec(sql)) {                                          // 执行 SQL
//         qWarning() << "[DB] doLoadQrMethodConfigs exec failed:"  // 打印错误
//                    << q.lastError().text();                      // 错误详情
//         emit qrMethodConfigsLoaded(rows);                        // 发空
//         return;                                                  // 结束
//     }

//     while (q.next()) {                                      // 遍历结果
//         QrMethodConfigRow r;                                // 一行数据
//         r.id = q.value("id").toInt();                       // 读取 id
//         r.projectName = q.value("projectName").toString();  // 读取 projectName
//         r.batchCode = q.value("batchCode").toString();      // 读取 batchCode
//         r.methodData = q.value("methodData").toString();    // 读取 methodData
//         rows.push_back(r);                                  // 压入列表
//     }

//     emit qrMethodConfigsLoaded(rows);  // 发信号给 VM
// }
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
                doLoadQrMethodConfigs();
                emit saveQrMethodConfigDone(ok, err);
                break;
            }
            case DBTaskType::LoadQrMethodConfigs: {  // 新增：加载 qr_method_config 列表（每行注释）
                doLoadQrMethodConfigs();             // 调用你已经实现的加载函数（每行注释）
                break;                               // 结束（每行注释）
            }

            case DBTaskType::DeleteQrMethodConfig: {  // 新增：删除 qr_method_config（每行注释）
                doDeleteQrMethodConfig(task.p1);      // p1 存 id（每行注释）
                break;                                // 结束（每行注释）
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
void DBWorker::postLoadQrMethodConfigs()  // 对外：投递加载列表任务（每行注释）
{
    {                                            // 锁作用域开始（每行注释）
        std::lock_guard<std::mutex> lk(m_);      // 加锁保护队列（每行注释）
        q_.push(DBTask::loadQrMethodConfigs());  // 入队：加载列表任务（每行注释）
    }  // 解锁（每行注释）
    cv_.notify_one();  // 唤醒 DB 线程（每行注释）
}

// =====================
// 对外：投递删除任务
// =====================
void DBWorker::postDeleteQrMethodConfig(int id)  // 对外：投递删除任务（每行注释）
{
    {                                               // 锁作用域开始（每行注释）
        std::lock_guard<std::mutex> lk(m_);         // 加锁保护队列（每行注释）
        q_.push(DBTask::deleteQrMethodConfig(id));  // 入队：删除任务（每行注释）
    }  // 解锁（每行注释）
    cv_.notify_one();  // 唤醒 DB 线程（每行注释）
}

// =====================
// 对外：投递插入/更新任务
// =====================
void DBWorker::postInsertQrMethodConfigInfo(const QVariantMap& info)  // 对外：投递插入/更新任务（每行注释）
{
    {                                                 // 锁作用域开始（每行注释）
        std::lock_guard<std::mutex> lk(m_);           // 加锁保护队列（每行注释）
        q_.push(DBTask::upsertQrMethodConfig(info));  // 入队：复用你已有 UpsertQrMethodConfig（每行注释）
    }  // 解锁（每行注释）
    cv_.notify_one();  // 唤醒 DB 线程（每行注释）
}

// =====================
// DB线程：执行加载列表
// =====================
void DBWorker::doLoadQrMethodConfigs() {
    QVector<QrMethodConfigRow> rows;  // 返回给 VM 的方法配置行列表

    // === 1. 获取当前线程的数据库连接 ===
    QSqlDatabase db = QSqlDatabase::database(connName_);
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "[DB] doLoadQrMethodConfigs: db not open";
        emit qrMethodConfigsLoaded(rows);
        return;
    }

    // === 2. 构造 SQL（字段必须查全，顺序必须和下面取值一致） ===
    const QString sql =
        "SELECT "
        "  id, "           // 0  方法配置 id
        "  projectName, "  // 1  项目名称
        "  batchCode, "    // 2  批次编码
        "  updated_at, "   // 3  更新时间
        "  methodData, "   // 4  ★ FourPL JSON
        "  temperature, "  // 5  温度
        "  timeSec, "      // 6  时间
        "  C1, "           // 7  C1
        "  T1, "           // 8  T1
        "  C2, "           // 9  C2
        "  T2 "            // 10  T2
        "FROM qr_method_config "
        "ORDER BY id DESC";
    QSqlQuery q(db);

    // === 3. 执行 SQL ===
    if (!q.exec(sql)) {
        qWarning() << "[DB] doLoadQrMethodConfigs exec failed:"
                   << q.lastError().text();
        emit qrMethodConfigsLoaded(rows);
        return;
    }

    // === 4. 逐行读取结果，填充 QrMethodConfigRow ===
    while (q.next()) {
        QrMethodConfigRow r;

        r.id = q.value(0).toInt();              // id
        r.projectName = q.value(1).toString();  // projectName
        r.batchCode = q.value(2).toString();    // batchCode
        r.updatedAt = q.value(3).toString();    // updated_at
        r.methodData = q.value(4).toString();   // ★ FourPL JSON
        r.temperature = q.value(5).toDouble();
        r.timeSec = q.value(6).toDouble();
        r.C1 = q.value(7).toInt();   // C1
        r.T1 = q.value(8).toInt();   // T1
        r.C2 = q.value(9).toInt();   // C2
        r.T2 = q.value(10).toInt();  // T2

        // // === 调试日志（建议保留，定位 DB 问题非常有用） ===
        qDebug() << "[DBWorker] method row:"
                 << "id=" << r.id
                 << "methodData.len=" << r.methodData.length()
                 << "C1=" << r.C1
                 << "T1=" << r.T1
                 << "C2=" << r.C2
                 << "T2=" << r.T2
                 << "temperature=" << r.temperature
                 << "time_Sec=" << r.timeSec;

        rows.push_back(r);
    }

    // === 5. 发信号给 ViewModel（跨线程 QueuedConnection） ===
    emit qrMethodConfigsLoaded(rows);
}

// =====================
// DB线程：执行删除
// =====================
void DBWorker::doDeleteQrMethodConfig(int id)  // DB线程：真正执行删除（每行注释）
{
    QSqlDatabase db = QSqlDatabase::database(connName_);           // 取当前线程数据库连接（每行注释）
    if (!db.isValid() || !db.isOpen()) {                           // 检查连接（每行注释）
        qWarning() << "[DB] doDeleteQrMethodConfig: db not open";  // 打印告警（每行注释）
        emit qrMethodConfigDeleted(false, id);                     // 通知失败（每行注释）
        return;                                                    // 结束（每行注释）
    }

    QSqlQuery q(db);                                           // 创建 query（每行注释）
    q.prepare("DELETE FROM qr_method_config WHERE id = :id");  // 预编译 SQL（每行注释）
    q.bindValue(":id", id);                                    // 绑定参数（每行注释）

    const bool ok = q.exec();                                // 执行（每行注释）
    if (!ok) {                                               // 失败（每行注释）
        qWarning() << "[DB] doDeleteQrMethodConfig failed:"  // 打印错误（每行注释）
                   << q.lastError().text()                   // 错误详情（每行注释）
                   << "id =" << id;                          // 打印 id（每行注释）
    }
    doLoadQrMethodConfigs();
    emit qrMethodConfigDeleted(ok, id);  // 通知结果（每行注释）
}

// =====================
// DB线程：执行插入/更新（按 UNIQUE(projectId,batchCode) 做 UPSERT）
// =====================
void DBWorker::doInsertQrMethodConfigInfo(const QVariantMap& info)  // DB线程：真正执行插入/更新（每行注释）
{
    QSqlDatabase db = QSqlDatabase::database(connName_);               // 取当前线程数据库连接（每行注释）
    if (!db.isValid() || !db.isOpen()) {                               // 检查连接（每行注释）
        qWarning() << "[DB] doInsertQrMethodConfigInfo: db not open";  // 打印告警（每行注释）
        emit saveQrMethodConfigDone(false, "db not open");             // 复用你已有的回调信号（每行注释）
        return;                                                        // 结束（每行注释）
    }

    const QString projectId = info.value("projectId").toString();          // projectId（NOT NULL）（每行注释）
    const QString projectName = info.value("projectName").toString();      // projectName（每行注释）
    const QString batchCode = info.value("batchCode").toString();          // batchCode（每行注释）
    const QString methodName = info.value("methodName").toString();        // methodName（每行注释）
    const QString methodData = info.value("methodData").toString();        // methodData（每行注释）
    const double temperature = info.value("temperature", 0.0).toDouble();  // temperature（每行注释）
    const int timeSec = info.value("timeSec", 0).toInt();                  // timeSec（每行注释）
    const int C1 = info.value("C1", 0).toInt();                            // C1（每行注释）
    const int T1 = info.value("T1", 0).toInt();                            // T1（每行注释）
    const int C2 = info.value("C2", 0).toInt();                            // C2（每行注释）
    const int T2 = info.value("T2", 0).toInt();                            // T2（每行注释）

    if (projectId.isEmpty() || batchCode.isEmpty()) {                        // 唯一键不能为空（每行注释）
        emit saveQrMethodConfigDone(false, "projectId or batchCode empty");  // 回调失败（每行注释）
        return;                                                              // 结束（每行注释）
    }

    QSqlQuery q(db);  // 创建 query（每行注释）

    // 先尝试 UPSERT（SQLite 3.24+ 支持），不会破坏 id（每行注释）
    const QString sqlUpsert =
        "INSERT INTO qr_method_config("                                            // 插入字段（每行注释）
        " projectId, projectName, batchCode, updated_at, methodName, methodData,"  // 字段（每行注释）
        " temperature, timeSec, C1, T1, C2, T2"                                    // 字段（每行注释）
        ") VALUES("                                                                // values（每行注释）
        " :projectId, :projectName, :batchCode, datetime('now','localtime'),"      // values（每行注释）
        " :methodName, :methodData, :temperature, :timeSec, :C1, :T1, :C2, :T2"    // values（每行注释）
        ") ON CONFLICT(projectId, batchCode) DO UPDATE SET "                       // 冲突更新（每行注释）
        " projectName=excluded.projectName,"                                       // 更新字段（每行注释）
        " updated_at=datetime('now','localtime'),"                                 // 更新时间刷新（每行注释）
        " methodName=excluded.methodName,"                                         // 更新字段（每行注释）
        " methodData=excluded.methodData,"                                         // 更新字段（每行注释）
        " temperature=excluded.temperature,"                                       // 更新字段（每行注释）
        " timeSec=excluded.timeSec,"                                               // 更新字段（每行注释）
        " C1=excluded.C1, T1=excluded.T1, C2=excluded.C2, T2=excluded.T2";         // 更新字段（每行注释）

    q.prepare(sqlUpsert);                      // prepare（每行注释）
    q.bindValue(":projectId", projectId);      // bind（每行注释）
    q.bindValue(":projectName", projectName);  // bind（每行注释）
    q.bindValue(":batchCode", batchCode);      // bind（每行注释）
    q.bindValue(":methodName", methodName);    // bind（每行注释）
    q.bindValue(":methodData", methodData);    // bind（每行注释）
    q.bindValue(":temperature", temperature);  // bind（每行注释）
    q.bindValue(":timeSec", timeSec);          // bind（每行注释）
    q.bindValue(":C1", C1);                    // bind（每行注释）
    q.bindValue(":T1", T1);                    // bind（每行注释）
    q.bindValue(":C2", C2);                    // bind（每行注释）
    q.bindValue(":T2", T2);                    // bind（每行注释）

    bool ok = q.exec();  // 执行 UPSERT（每行注释）
    QString err;         // 错误信息（每行注释）

    if (!ok) {                       // UPSERT 失败（每行注释）
        err = q.lastError().text();  // 记录错误（每行注释）

        // 兼容旧 SQLite：用“先 UPDATE，再 INSERT”的方式，避免 REPLACE 改 id（每行注释）
        QSqlQuery qu(db);  // update query（每行注释）
        qu.prepare(
            "UPDATE qr_method_config SET "                         // update（每行注释）
            " projectName=:projectName,"                           // 字段（每行注释）
            " updated_at=datetime('now','localtime'),"             // 更新时间（每行注释）
            " methodName=:methodName,"                             // 字段（每行注释）
            " methodData=:methodData,"                             // 字段（每行注释）
            " temperature=:temperature,"                           // 字段（每行注释）
            " timeSec=:timeSec,"                                   // 字段（每行注释）
            " C1=:C1, T1=:T1, C2=:C2, T2=:T2 "                     // 字段（每行注释）
            "WHERE projectId=:projectId AND batchCode=:batchCode"  // 唯一键条件（每行注释）
        );                                                         // 结束 prepare（每行注释）

        qu.bindValue(":projectId", projectId);      // bind（每行注释）
        qu.bindValue(":batchCode", batchCode);      // bind（每行注释）
        qu.bindValue(":projectName", projectName);  // bind（每行注释）
        qu.bindValue(":methodName", methodName);    // bind（每行注释）
        qu.bindValue(":methodData", methodData);    // bind（每行注释）
        qu.bindValue(":temperature", temperature);  // bind（每行注释）
        qu.bindValue(":timeSec", timeSec);          // bind（每行注释）
        qu.bindValue(":C1", C1);                    // bind（每行注释）
        qu.bindValue(":T1", T1);                    // bind（每行注释）
        qu.bindValue(":C2", C2);                    // bind（每行注释）
        qu.bindValue(":T2", T2);                    // bind（每行注释）

        ok = qu.exec();                        // 执行 UPDATE（每行注释）
        if (ok && qu.numRowsAffected() > 0) {  // 更新到一行（每行注释）
            err.clear();                       // 清掉错误（每行注释）
        } else {                               // 没更新到（可能不存在，需要 INSERT）（每行注释）
            QSqlQuery qi(db);                  // insert query（每行注释）
            qi.prepare(
                "INSERT INTO qr_method_config("                                            // insert（每行注释）
                " projectId, projectName, batchCode, updated_at, methodName, methodData,"  // 字段（每行注释）
                " temperature, timeSec, C1, T1, C2, T2"                                    // 字段（每行注释）
                ") VALUES("                                                                // values（每行注释）
                " :projectId, :projectName, :batchCode, datetime('now','localtime'),"      // values（每行注释）
                " :methodName, :methodData, :temperature, :timeSec, :C1, :T1, :C2, :T2"    // values（每行注释）
                ")"                                                                        // 结束（每行注释）
            );                                                                             // 结束 prepare（每行注释）

            qi.bindValue(":projectId", projectId);      // bind（每行注释）
            qi.bindValue(":projectName", projectName);  // bind（每行注释）
            qi.bindValue(":batchCode", batchCode);      // bind（每行注释）
            qi.bindValue(":methodName", methodName);    // bind（每行注释）
            qi.bindValue(":methodData", methodData);    // bind（每行注释）
            qi.bindValue(":temperature", temperature);  // bind（每行注释）
            qi.bindValue(":timeSec", timeSec);          // bind（每行注释）
            qi.bindValue(":C1", C1);                    // bind（每行注释）
            qi.bindValue(":T1", T1);                    // bind（每行注释）
            qi.bindValue(":C2", C2);                    // bind（每行注释）
            qi.bindValue(":T2", T2);                    // bind（每行注释）

            ok = qi.exec();                   // 执行 INSERT（每行注释）
            if (!ok) {                        // 失败（每行注释）
                err = qi.lastError().text();  // 记录错误（每行注释）
            } else {                          // 成功（每行注释）
                err.clear();                  // 清掉错误（每行注释）
            }
        }
    }

    emit saveQrMethodConfigDone(ok, err);  // 复用你已有的保存回调（每行注释）
}
