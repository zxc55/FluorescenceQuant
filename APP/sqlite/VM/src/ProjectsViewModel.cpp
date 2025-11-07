#include "ProjectsViewModel.h"

ProjectsViewModel::ProjectsViewModel(QObject* parent)
    : QAbstractListModel(parent) {
    openDatabase();
    loadData();
}

bool ProjectsViewModel::openDatabase() {
    // ✅ 打开数据库（路径可根据你的系统修改）
    QString dbPath = "/mnt/SDCARD/app/db/app.db";
    if (QSqlDatabase::contains("app_connection"))
        m_db = QSqlDatabase::database("app_connection");
    else
        m_db = QSqlDatabase::addDatabase("QSQLITE", "app_connection");

    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        qWarning() << "数据库打开失败:" << m_db.lastError().text();
        return false;
    }
    qDebug() << "数据库打开成功:" << dbPath;
    return true;
}

void ProjectsViewModel::loadData() {
    beginResetModel();
    m_list.clear();

    if (!m_db.isOpen())
        openDatabase();

    QSqlQuery query(m_db);
    // ✅ 使用你的实际字段名
    query.prepare("SELECT id, name, batch, updated_at FROM projects ORDER BY id ASC");

    if (!query.exec()) {
        qWarning() << "❌ 查询失败:" << query.lastError().text();
    } else {
        while (query.next()) {
            ProjectItem item;
            item.rid = query.value(0).toInt();           // id
            item.name = query.value(1).toString();       // name
            item.batch = query.value(2).toString();      // batch
            item.updatedAt = query.value(3).toString();  // updated_at
            m_list.append(item);
        }
        qDebug() << "✅ 加载数据条数:" << m_list.size();
    }

    endResetModel();
    emit countChanged();
}

// === QML 调用接口 ===
void ProjectsViewModel::refresh() {
    qDebug() << "刷新项目列表";
    loadData();
}

void ProjectsViewModel::deleteById(int rid) {
    if (!m_db.isOpen())
        openDatabase();

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM projects WHERE id = :id");
    query.bindValue(":id", rid);
    if (!query.exec()) {
        qWarning() << "删除失败:" << query.lastError().text();
    }

    loadData();
}

// === QAbstractListModel接口实现 ===
int ProjectsViewModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return m_list.size();
}

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

QHash<int, QByteArray> ProjectsViewModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[RidRole] = "rid";
    roles[NameRole] = "name";
    roles[BatchRole] = "batch";
    roles[UpdatedAtRole] = "updatedAt";
    return roles;
}

// ✅ QML 可安全访问的 get() 函数
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
