#include "ProjectsViewModel.h"  // 自己的头文件

#include <QDebug>  // 用于 qDebug / qWarning / qInfo

#include "DBWorker.h"  // DBWorker 的完整定义

// =====================
// 构造函数
// =====================
ProjectsViewModel::ProjectsViewModel(DBWorker* worker, QObject* parent)
    : QAbstractListModel(parent), m_worker(worker)  // 保存 DBWorker 指针
{
    qDebug() << "[ProjectsVM] constructed, worker =" << m_worker;

    if (m_worker) {
        // 当 DBWorker 发射 projectsLoaded 时，更新本地列表
        connect(m_worker, &DBWorker::projectsLoaded,
                this, &ProjectsViewModel::onProjectsLoaded,
                Qt::QueuedConnection);

        // 当 DBWorker 发射 projectDeleted 时，根据结果刷新
        connect(m_worker, &DBWorker::projectDeleted,
                this, &ProjectsViewModel::onProjectDeleted,
                Qt::QueuedConnection);
    }
}

// =====================
// 必须实现：返回总行数
// =====================
int ProjectsViewModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)       // 不使用 parent
    return m_list.size();  // 当前项目数量
}

// =====================
// 必须实现：根据 index + role 返回数据
// =====================
QVariant ProjectsViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()                  // index 无效
        || index.row() < 0                // 行号小于 0
        || index.row() >= m_list.size())  // 行号越界
        return QVariant();                // 返回空值

    const auto& item = m_list.at(index.row());  // 取出对应行的数据

    switch (role) {
    case RidRole:
        return item.rid;  // 返回项目 id
    case NameRole:
        return item.name;  // 返回项目名称
    case BatchRole:
        return item.batch;  // 返回批次
    case UpdatedAtRole:
        return item.updatedAt;  // 返回更新时间
    default:
        return QVariant();  // 未知 role 返回空
    }
}

// =====================
// 必须实现：role -> 名称，用于 QML 访问 model.xxx
// =====================
QHash<int, QByteArray> ProjectsViewModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[RidRole] = "rid";              // QML: model.rid
    roles[NameRole] = "name";            // QML: model.name
    roles[BatchRole] = "batch";          // QML: model.batch
    roles[UpdatedAtRole] = "updatedAt";  // QML: model.updatedAt
    return roles;
}

// =====================
// QML 调用：刷新列表
// =====================
void ProjectsViewModel::refresh() {
    if (!m_worker) {  // 没有 worker，打印警告
        qWarning() << "[ProjectsVM] refresh() no worker!";
        return;
    }
    m_worker->postLoadProjects();  // 向 DBWorker 发送“加载项目列表”任务
}

// =====================
// QML 调用：删除指定 id 的项目
// =====================
void ProjectsViewModel::deleteById(int rid) {
    if (!m_worker) {  // 没有 worker
        qWarning() << "[ProjectsVM] deleteById() no worker!";
        return;
    }
    m_worker->postDeleteProject(rid);  // 让 DBWorker 去删
}

// =====================
// 槽函数：DBWorker 加载项目完成
// =====================
void ProjectsViewModel::onProjectsLoaded(const QVector<ProjectRow>& rows) {
    beginResetModel();            // 通知视图：模型要整体重置
    m_list.clear();               // 清理旧数据
    m_list.reserve(rows.size());  // 预分配空间，避免多次 realloc

    for (const auto& r : rows) {
        ProjectItem item;
        item.rid = r.id;  // 从 ProjectRow 拷贝字段
        item.name = r.name;
        item.batch = r.batch;
        item.updatedAt = r.updatedAt;
        m_list.append(item);
    }

    endResetModel();      // 通知视图数据重载完毕
    emit countChanged();  // 通知 QML count 发生变化
    emit projectsReady();
    qInfo() << "[ProjectsVM] Loaded" << m_list.size() << "rows";
}

// =====================
// 槽函数：DBWorker 删除项目完成
// =====================
void ProjectsViewModel::onProjectDeleted(bool ok, int id) {
    qInfo() << "[ProjectsVM] delete result id =" << id << "ok =" << ok;
    if (ok)
        refresh();  // 删除成功后重新加载一遍
}

// =====================
// QML 调用：按行号取一行数据
// =====================
QVariantMap ProjectsViewModel::get(int index) const {
    QVariantMap map;
    if (index < 0 || index >= m_list.size())  // 越界保护
        return map;

    const auto& item = m_list.at(index);
    map["rid"] = item.rid;
    map["name"] = item.name;
    map["batch"] = item.batch;
    map["updatedAt"] = item.updatedAt;
    return map;
}

// =====================
// QML 调用：根据 id 获取项目名称
// =====================
QString ProjectsViewModel::getNameById(int id) const {
    for (const auto& item : m_list) {  // 遍历当前列表
        if (item.rid == id)            // 找到匹配 id
            return item.name;          // 返回名称
    }
    return QString();  // 没找到返回空字符串
}

// =====================
// QML 调用：根据 id 获取批次
// =====================
QString ProjectsViewModel::getBatchById(int id) const {
    for (const auto& item : m_list) {  // 遍历当前列表
        if (item.rid == id)            // 找到匹配 id
            return item.batch;         // 返回批次
    }
    return QString();  // 没找到返回空字符串
}
bool ProjectsViewModel::insertProjectInfo(const QVariantMap& info) {
    if (!m_worker) {
        qWarning() << "[ProjectsVM] insertProjectInfo no worker!";
        return false;
    }
    m_worker->postInsertProjectInfo(info);
    return true;
}
