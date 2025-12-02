#ifndef PROJECTSVIEWMODEL_H  // 头文件防重保护：避免被重复包含
#define PROJECTSVIEWMODEL_H

#include <QAbstractListModel>  // QAbstractListModel 基类
#include <QObject>             // QObject 基类
#include <QVariantMap>         // QVariantMap 用于给 QML 返回 map
#include <QVector>             // QVector 容器

#include "DTO.h"  // 里面有 ProjectRow 等结构体定义

class DBWorker;  // 只做前置声明，不需要完整包含 DBWorker.h

// =====================
// 单个项目的数据结构
// =====================
struct ProjectItem {
    int rid;            // 数据库中项目的 id
    QString name;       // 项目名称
    QString batch;      // 项目批次
    QString updatedAt;  // 更新时间
};

// =====================
// 项目列表 ViewModel
// =====================
class ProjectsViewModel : public QAbstractListModel {
Q_OBJECT  // 必须有 Q_OBJECT 宏，moc 才能工作

// 暴露给 QML 的属性：count 表示行数
Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

    public :

    // ====== QML 中可见的函数（必须 Q_INVOKABLE 且在 public 下） ======
    Q_INVOKABLE QString getNameById(int id) const;   // 根据项目 id 获取项目名称
    Q_INVOKABLE QString getBatchById(int id) const;  // 根据项目 id 获取批次
    Q_INVOKABLE bool insertProjectInfo(const QVariantMap& info);

    // === 数据角色枚举，用于 QML 访问 model.rid / model.name 等字段 ===
    enum Role {
        RidRole = Qt::UserRole + 1,  // 项目 id
        NameRole,                    // 名称
        BatchRole,                   // 批次
        UpdatedAtRole                // 更新时间
    };

    // 构造函数：需要传入 DBWorker 指针，用来发起数据库任务
    explicit ProjectsViewModel(DBWorker* worker = nullptr,
                               QObject* parent = nullptr);

    ~ProjectsViewModel() override = default;  // 默认析构

    // ============ QAbstractListModel 必须实现的接口 ============
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ============ 给 QML 调用的接口 ============
    Q_INVOKABLE void refresh();                    // 刷新项目列表（让 DBWorker 去加载）
    Q_INVOKABLE void deleteById(int rid);          // 根据 id 删除项目
    Q_INVOKABLE QVariantMap get(int index) const;  // 按行索引获取一行数据

signals:
    void countChanged();  // 行数变化时通知 QML
    void projectsReady();
private slots:
    // DBWorker 加载完项目之后，会发射 projectsLoaded 信号，进到这里
    void onProjectsLoaded(const QVector<ProjectRow>& rows);

    // DBWorker 删除项目完成后，发射 projectDeleted 信号，进到这里
    void onProjectDeleted(bool ok, int id);

private:
    DBWorker* m_worker = nullptr;  // 保存 DBWorker 指针，用来发任务
    QVector<ProjectItem> m_list;   // 当前内存中的项目列表
};

#endif  // PROJECTSVIEWMODEL_H
