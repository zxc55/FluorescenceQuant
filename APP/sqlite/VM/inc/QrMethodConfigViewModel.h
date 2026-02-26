#ifndef QRMETHODCONFIGVIEWMODEL_H_
#define QRMETHODCONFIGVIEWMODEL_H_
#include <QAbstractListModel>  // QAbstractListModel 基类
#include <QString>             // QString 类型
#include <QVariantMap>         // QVariantMap 类型
#include <QVector>             // QVector 容器

class DBWorker;            // 前置声明 DBWorker，避免循环包含
struct QrMethodConfigRow;  // 前置声明：DBWorker 返回的行结构体

class QrMethodConfigViewModel : public QAbstractListModel  // 方法配置列表 VM，供 QML Repeater/ListView 使用
{
Q_OBJECT  // Qt 元对象系统宏

Q_PROPERTY(int count READ rowCount NOTIFY countChanged)  // 暴露 count 给 QML（和你 ProjectsVM 一样）

    public : explicit QrMethodConfigViewModel(DBWorker* worker, QObject* parent = nullptr);  // 构造函数：传入 DBWorker

    enum Roles  // 角色枚举：QML 里直接用 rid/projectName/batchCode/methodData
    {
        RidRole = Qt::UserRole + 1,  // rid 角色（对应表 id）
        ProjectNameRole,             // projectName 角色
        BatchCodeRole,               // batchCode 角色
        updatedAtRole,               // updated角色
        MethodDataRole,
        temperatureRole,
        timeSecRole,
        C1Role,
        T1Role,
        C2Role,
        T2Role
    };

    struct Item  // 内部缓存结构
    {
        int rid = 0;          // 表 id
        QString projectName;  // projectName
        QString batchCode;    // batchCode
        int C1 = 0;           // C1
        int T1 = 0;           // T1
        int C2 = 0;           // C2
        int T2 = 0;           // T2
        QString methodData;
        QString updatedAt;
        double temperature;
        int timeSec;
    };
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;  // 必须实现：返回行数
    QVariant data(const QModelIndex& index, int role) const override;        // 必须实现：按 role 返回数据
    QHash<int, QByteArray> roleNames() const override;                       // 必须实现：返回 role 名称映射给 QML

    Q_INVOKABLE void refresh();                            // QML 调用：刷新列表
    Q_INVOKABLE void deleteById(int rid);                  // QML 调用：按 rid 删除一行
    Q_INVOKABLE QVariantMap get(int index) const;          // QML 调用：按行号获取一行 map
    Q_INVOKABLE QString getProjectNameById(int id) const;  // QML 调用：按 id 获取 projectName
    Q_INVOKABLE QString getBatchCodeById(int id) const;    // QML 调用：按 id 获取 batchCode
    Q_INVOKABLE QString getMethodDataById(int id) const;   // QML 调用：按 id 获取 methodData
    Q_INVOKABLE double getTemperatureById(int id) const;
    Q_INVOKABLE int getTimeSecById(int id) const;
    Item findItemById(int id) const;
    Q_INVOKABLE bool insertMethodConfigInfo(const QVariantMap& info);  // QML 调用：插入一条 method config（走 DBWorker）
    Q_INVOKABLE QVariantMap findItemByIdQml(int id) const;
signals:
    void countChanged();        // count 变化通知
    void methodConfigsReady();  // 数据准备好（对标 projectsReady）

private slots:
    void onLoaded(const QVector<QrMethodConfigRow>& rows);  // 槽函数：DBWorker 加载完成
    void onDeleted(bool ok, int id);                        // 槽函数：DBWorker 删除完成

private:
    DBWorker* m_worker = nullptr;  // DBWorker 指针（不拥有生命周期）
    QVector<Item> m_list;          // 缓存列表（对标 m_list）
};

#endif  // QRMETHODCONFIGVIEWMODEL_H_