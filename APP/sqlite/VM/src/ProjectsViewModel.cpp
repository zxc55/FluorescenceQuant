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
    beginResetModel();  // 用来通知 QML 或 View（比如 ListView、Repeater 等）：⚠️ 我的整个模型数据要被重新加载或清空了！
    m_list.clear();

    if (!m_db.isOpen())
        openDatabase();

    QSqlQuery query(m_db);  // 在数据库连接 m_db 上创建一个 SQL 查询对象。m_db 必须是一个已打开的数据库连接。
    // ✅ 使用你的实际字段名
    query.prepare("SELECT id, name, batch, updated_at FROM projects ORDER BY id ASC");  // 准备好一条 SQL 语句（相当于预编译 SQL 命令）。Qt 会检查语法。

    if (!query.exec()) {  // 执行这条 SQL 查询语句。只有 exec() 后才会真正访问数据库。
        qWarning() << "❌ 查询失败:" << query.lastError().text();
    } else {
        while (query.next()) {  // 遍历数据库查询结果。每次循环读取一行数据。如果还有下一行返回 true，否则 false。
            ProjectItem item;
            item.rid = query.value(0).toInt();           // id
            item.name = query.value(1).toString();       // name
            item.batch = query.value(2).toString();      // batch
            item.updatedAt = query.value(3).toString();  // updated_at
            m_list.append(item);                         // 把当前行的数据追加到 m_list 里。
        }
        qDebug() << "✅ 加载数据条数:" << m_list.size();
    }

    endResetModel();  // 告诉视图：数据加载完成，刷新显示
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
