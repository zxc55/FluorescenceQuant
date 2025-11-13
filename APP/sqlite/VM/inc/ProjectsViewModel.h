#ifndef PROJECTSVIEWMODEL_H
#define PROJECTSVIEWMODEL_H

#include <QAbstractListModel>
#include <QDebug>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

// ✅ 数据结构：单个项目
struct ProjectItem {
    int rid;
    QString name;
    QString batch;
    QString updatedAt;
};

class ProjectsViewModel : public QAbstractListModel {
    Q_OBJECT
    // ✅ 暴露给 QML 可用
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        RidRole = Qt::UserRole + 1,
        NameRole,
        BatchRole,
        UpdatedAtRole
    };

    explicit ProjectsViewModel(QObject* parent = nullptr);
    ~ProjectsViewModel() override = default;

    // === 模型接口 ===
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // === 公开接口 ===
    Q_INVOKABLE void refresh();                    // 从数据库刷新
    Q_INVOKABLE void deleteById(int rid);          // 删除指定 ID
    Q_INVOKABLE QVariantMap get(int index) const;  // 获取某一行内容

    // ✅ 新增接口：供 QML 调用，按 ID 查找项目名称和批次
    Q_INVOKABLE QString getNameById(int id) const;
    Q_INVOKABLE QString getBatchById(int id) const;

    // ✅ 新增接口：写入 project_info 表
    Q_INVOKABLE bool insertProjectInfo(const QVariantMap& info);

signals:
    void countChanged();  // 通知 QML 行数变化

private:
    QList<ProjectItem> m_list;
    QSqlDatabase m_db;

    bool openDatabase();  // 打开数据库
    void loadData();      // 加载数据
};

#endif  // PROJECTSVIEWMODEL_H
