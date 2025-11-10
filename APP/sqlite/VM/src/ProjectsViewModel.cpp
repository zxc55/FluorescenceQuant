#include "ProjectsViewModel.h"

// === 构造函数 ===
ProjectsViewModel::ProjectsViewModel(QObject* parent)
    : QAbstractListModel(parent) {
    openDatabase();
    loadData();
}

// === 打开数据库 ===
bool ProjectsViewModel::openDatabase() {
    // ✅ 数据库存放路径（SD 卡）
    const QString dbPath = "/mnt/SDCARD/app/db/app.db";

    // 连接名固定，防止重复连接
    if (QSqlDatabase::contains("app_connection"))
        m_db = QSqlDatabase::database("app_connection");
    else
        m_db = QSqlDatabase::addDatabase("QSQLITE", "app_connection");

    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "❌ 数据库打开失败:" << m_db.lastError().text();
        return false;
    }

    qDebug() << "✅ 数据库打开成功:" << dbPath;
    return true;
}

// === 加载项目列表数据 ===
void ProjectsViewModel::loadData() {
    beginResetModel();  // 通知视图：模型数据将被重新加载
    m_list.clear();     // 清空旧数据

    if (!m_db.isOpen() && !openDatabase()) {
        endResetModel();
        return;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, batch, updated_at FROM projects ORDER BY id ASC");

    if (!query.exec()) {
        qWarning() << "❌ 查询 projects 表失败:" << query.lastError().text();
        endResetModel();
        return;
    }

    while (query.next()) {
        ProjectItem item;
        item.rid = query.value(0).toInt();
        item.name = query.value(1).toString();
        item.batch = query.value(2).toString();
        item.updatedAt = query.value(3).toString();
        m_list.append(item);
    }

    qDebug() << "✅ 加载项目数量:" << m_list.size();

    endResetModel();      // 通知视图数据加载完成
    emit countChanged();  // 更新 QML 侧绑定
}

// === QML 调用：刷新 ===
void ProjectsViewModel::refresh() {
    qDebug() << "[ProjectsViewModel] 刷新项目列表";
    loadData();
}

// === QML 调用：删除指定项目 ===
void ProjectsViewModel::deleteById(int rid) {
    if (!m_db.isOpen() && !openDatabase())
        return;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM projects WHERE id = :id");
    query.bindValue(":id", rid);

    if (!query.exec()) {
        qWarning() << "❌ 删除项目失败:" << query.lastError().text();
        return;
    }

    qInfo() << "🗑 已删除项目 id =" << rid;
    loadData();
}

// === QAbstractListModel: 行数 ===
int ProjectsViewModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return m_list.size();
}

// === QAbstractListModel: 数据 ===
QVariant ProjectsViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_list.size())
        return QVariant();

    const ProjectItem& item = m_list.at(index.row());
    switch (role) {
    case RidRole:
        return item.rid;
    case NameRole:
        return item.name;
    case BatchRole:
        return item.batch;
    case UpdatedAtRole:
        return item.updatedAt;
    default:
        return QVariant();
    }
}

// === QAbstractListModel: 角色名 ===
QHash<int, QByteArray> ProjectsViewModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[RidRole] = "rid";
    roles[NameRole] = "name";
    roles[BatchRole] = "batch";
    roles[UpdatedAtRole] = "updatedAt";
    return roles;
}

// === QML 调用：按行索引取数据 ===
QVariantMap ProjectsViewModel::get(int index) const {
    QVariantMap map;
    if (index < 0 || index >= m_list.size())
        return map;

    const ProjectItem& item = m_list.at(index);
    map["rid"] = item.rid;
    map["name"] = item.name;
    map["batch"] = item.batch;
    map["updatedAt"] = item.updatedAt;
    return map;
}

// === QML 调用：按 ID 获取项目名称 ===
QString ProjectsViewModel::getNameById(int id) const {
    for (const auto& item : m_list) {
        if (item.rid == id)
            return item.name;
    }
    return QString();
}

// === QML 调用：按 ID 获取批次 ===
QString ProjectsViewModel::getBatchById(int id) const {
    for (const auto& item : m_list) {
        if (item.rid == id)
            return item.batch;
    }
    return QString();
}
// === QML 调用：插入检测记录 ===
bool ProjectsViewModel::insertProjectInfo(const QVariantMap& info) {
    if (!m_db.isOpen() && !openDatabase()) {
        qWarning() << "❌ 数据库未打开，无法写入 project_info";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(R"SQL(
        INSERT INTO project_info (
            project_id,
            sample_no,
            sample_source,
            sample_name,
            standard_curve,
            batch_code,
            detected_conc,
            reference_value,
            result,
            detected_time,
            detected_unit,
            detected_person,
            dilution_info
        ) VALUES (
            :project_id,
            :sample_no,
            :sample_source,
            :sample_name,
            :standard_curve,
            :batch_code,
            :detected_conc,
            :reference_value,
            :result,
            :detected_time,
            :detected_unit,
            :detected_person,
            :dilution_info
        );
    )SQL");

    // 一一绑定字段（与 Migration 表定义严格匹配）
    q.bindValue(":project_id", info.value("project_id"));
    q.bindValue(":sample_no", info.value("sample_no"));
    q.bindValue(":sample_source", info.value("sample_source"));
    q.bindValue(":sample_name", info.value("sample_name"));
    q.bindValue(":standard_curve", info.value("standard_curve"));
    q.bindValue(":batch_code", info.value("batch_code"));
    q.bindValue(":detected_conc", info.value("detected_conc", 0.0));
    q.bindValue(":reference_value", info.value("reference_value", 0.0));
    q.bindValue(":result",
                info.contains("result") && !info.value("result").toString().isEmpty()
                    ? info.value("result")
                    : QVariant("合格"));
    q.bindValue(":detected_time", info.value("detected_time"));
    q.bindValue(":detected_unit", info.value("detected_unit", "μg/kg"));
    q.bindValue(":detected_person", info.value("detected_person"));
    q.bindValue(":dilution_info", info.value("dilution_info", "1倍"));

    if (!q.exec()) {
        qWarning() << "❌ 插入 project_info 失败:" << q.lastError().text();
        return false;
    }

    qInfo() << "✅ 成功写入 project_info 一条记录";
    return true;
}
