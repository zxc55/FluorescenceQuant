#include "QrMethodConfigViewModel.h"  // 自己的头文件

#include <QDebug>  // 用于 qDebug / qWarning / qInfo

#include "DBWorker.h"  // DBWorker 完整定义（需要信号/槽、结构体）

QrMethodConfigViewModel::QrMethodConfigViewModel(DBWorker* worker, QObject* parent)
    : QAbstractListModel(parent), m_worker(worker)  // 保存 DBWorker 指针
{
    qDebug() << "[QrMethodConfigVM] constructed, worker =" << m_worker;  // 打印构造信息

    if (m_worker) {                                          // 判断 worker 是否有效
        connect(m_worker, &DBWorker::qrMethodConfigsLoaded,  // 连接：加载完成信号
                this, &QrMethodConfigViewModel::onLoaded,    // 槽：更新本地列表
                Qt::QueuedConnection);                       // 跨线程安全队列连接

        connect(m_worker, &DBWorker::qrMethodConfigDeleted,  // 连接：删除完成信号
                this, &QrMethodConfigViewModel::onDeleted,   // 槽：根据结果刷新
                Qt::QueuedConnection);                       // 跨线程安全队列连接
    }
}

int QrMethodConfigViewModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)       // 不使用 parent
    return m_list.size();  // 返回当前行数
}

QVariant QrMethodConfigViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_list.size())  // 越界保护
        return QVariant();                                                    // 返回空值

    const auto& item = m_list.at(index.row());  // 取出一行数据

    switch (role) {               // 根据 role 返回字段
    case RidRole:                 // rid
        return item.rid;          // 返回 id
    case ProjectNameRole:         // projectName
        return item.projectName;  // 返回项目名称
    case BatchCodeRole:           // batchCode
        return item.batchCode;    // 返回批次编码
    case updatedAtRole:           // methodData
        return item.updatedAt;    // 返回 methodData
    case C1Role:
        return item.C1;
    case T1Role:
        return item.T1;
    case C2Role:
        return item.C2;
    case T2Role:
        return item.T2;
    case MethodDataRole:
        return item.methodData;
    case temperatureRole:
        return item.temperature;
    case timeSecRole:
        return item.timeSec;

    default:                // 未知 role
        return QVariant();  // 返回空
    }
}

QHash<int, QByteArray> QrMethodConfigViewModel::roleNames() const {
    QHash<int, QByteArray> roles;            // 创建 role 映射表
    roles[RidRole] = "rid";                  // QML: rid
    roles[ProjectNameRole] = "projectName";  // QML: projectName
    roles[BatchCodeRole] = "batchCode";      // QML: batchCode
    roles[updatedAtRole] = "updatedAt";      // QML:
    roles[temperatureRole] = "temperature";
    roles[timeSecRole] = "timeSec";
    roles[C1Role] = "C1";
    roles[T1Role] = "T1";
    roles[C2Role] = "C2";
    roles[T2Role] = "T2";
    roles[MethodDataRole] = "methodData";
    return roles;  // 返回
}

void QrMethodConfigViewModel::refresh() {
    if (!m_worker) {                                              // worker 空指针保护
        qWarning() << "[QrMethodConfigVM] refresh() no worker!";  // 打印告警
        return;                                                   // 结束
    }
    m_worker->postLoadQrMethodConfigs();  // 发送加载任务（DBWorker 线程执行）
}

void QrMethodConfigViewModel::deleteById(int rid) {
    if (!m_worker) {                                                 // worker 空指针保护
        qWarning() << "[QrMethodConfigVM] deleteById() no worker!";  // 打印告警
        return;                                                      // 结束
    }
    m_worker->postDeleteQrMethodConfig(rid);  // 发送删除任务
}

void QrMethodConfigViewModel::onLoaded(const QVector<QrMethodConfigRow>& rows) {
    beginResetModel();            // 通知视图：整体重置
    m_list.clear();               // 清空旧数据
    m_list.reserve(rows.size());  // 预分配空间

    for (const auto& r : rows) {           // 遍历 DBWorker 返回行
        Item item;                         // 构造一条缓存
        item.rid = r.id;                   // id -> rid
        item.projectName = r.projectName;  // projectName
        item.batchCode = r.batchCode;      // batchCode
        item.updatedAt = r.updatedAt;      // methodData
        item.temperature = r.temperature;
        item.timeSec = r.timeSec;
        item.C1 = r.C1;
        item.T1 = r.T1;
        item.C2 = r.C2;
        item.T2 = r.T2;
        item.methodData = r.methodData;
        qDebug() << "-----[QrMethodConfigVM] Loaded----" << item.projectName << item.batchCode << item.updatedAt << r.methodData << r.C1 << r.T1 << r.C2 << r.T2;
        m_list.append(item);  // 加入列表
    }

    endResetModel();                                                    // 通知视图：重置结束
    emit countChanged();                                                // 通知 QML count 变化
    emit methodConfigsReady();                                          // 通知数据准备好
    qInfo() << "[QrMethodConfigVM] Loaded" << m_list.size() << "rows";  // 打印加载条数
}

void QrMethodConfigViewModel::onDeleted(bool ok, int id) {
    qInfo() << "[QrMethodConfigVM] delete result id =" << id << "ok =" << ok;  // 打印删除结果
    if (ok)                                                                    // 删除成功
        refresh();                                                             // 重新加载
}

QVariantMap QrMethodConfigViewModel::get(int index) const {
    QVariantMap map;                          // 返回 map
    if (index < 0 || index >= m_list.size())  // 越界保护
        return map;                           // 返回空 map

    const auto& item = m_list.at(index);    // 取出一行
    map["rid"] = item.rid;                  // rid
    map["projectName"] = item.projectName;  // projectName
    map["batchCode"] = item.batchCode;      // batchCode
    map["methodData"] = item.methodData;    // methodData
    map["updatedAt"] = item.updatedAt;
    map["C1"] = item.C1;
    map["T1"] = item.T1;
    map["C2"] = item.C2;
    map["T2"] = item.T2;
    return map;  // 返回
}

QString QrMethodConfigViewModel::getProjectNameById(int id) const {
    for (const auto& item : m_list) {  // 遍历列表
        if (item.rid == id)            // 找到匹配 id
            return item.projectName;   // 返回 projectName
    }
    return QString();  // 未找到返回空
}

QString QrMethodConfigViewModel::getBatchCodeById(int id) const {
    for (const auto& item : m_list) {  // 遍历列表
        if (item.rid == id)            // 找到匹配 id
            return item.batchCode;     // 返回 batchCode
    }
    return QString();  // 未找到返回空
}
// 返回指定 id 的 temperature（REAL → double）
double QrMethodConfigViewModel::getTemperatureById(int id) const {
    for (const auto& item : m_list) {
        if (item.rid == id) {
            return item.temperature;
        }
    }
    return 0.0;  // 未找到返回默认值 0.0（与表默认值一致）
}

// 返回指定 id 的 timeSec（INTEGER → int）
int QrMethodConfigViewModel::getTimeSecById(int id) const {
    for (const auto& item : m_list) {
        if (item.rid == id) {
            return item.timeSec;
        }
    }
    return 0;  // 未找到返回默认值 0（与表默认值一致）
}
QString QrMethodConfigViewModel::getMethodDataById(int id) const {
    for (const auto& item : m_list) {  // 遍历列表
        if (item.rid == id)            // 找到匹配 id
            return item.updatedAt;     // 返回 updatedAt
    }
    return QString();  // 未找到返回空
}

bool QrMethodConfigViewModel::insertMethodConfigInfo(const QVariantMap& info) {
    if (!m_worker) {                                                           // worker 空指针保护
        qWarning() << "[QrMethodConfigVM] insertMethodConfigInfo no worker!";  // 打印告警
        return false;                                                          // 返回失败
    }
    m_worker->postInsertQrMethodConfigInfo(info);  // 发送插入任务给 DBWorker
    return true;                                   // 返回成功（异步插入）
}
QrMethodConfigViewModel::Item
QrMethodConfigViewModel::findItemById(int id) const {
    for (const auto& item : m_list) {
        if (item.rid == id)
            return item;
    }
    return Item();
}
QVariantMap QrMethodConfigViewModel::findItemByIdQml(int id) const {
    QVariantMap map;

    // 复用你已经写好的 C++ 内部函数
    Item item = findItemById(id);

    // 如果没找到（rid=0，按你当前设计）
    if (item.rid == 0)
        return map;

    map["rid"] = item.rid;
    map["projectName"] = item.projectName;
    map["batchCode"] = item.batchCode;

    map["C1"] = item.C1;
    map["T1"] = item.T1;
    map["C2"] = item.C2;
    map["T2"] = item.T2;

    map["methodData"] = item.methodData;
    map["updatedAt"] = item.updatedAt;

    return map;
}
