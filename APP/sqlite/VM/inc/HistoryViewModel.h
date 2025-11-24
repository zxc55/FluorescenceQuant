#ifndef HISTORYVIEWMODEL_H
#define HISTORYVIEWMODEL_H

#include <QAbstractListModel>
#include <QDebug>
#include <QVector>

#include "DBWorker.h"
#include "DTO.h"

/**
 * @brief 历史记录视图模型（连接 QML 与数据库）
 * 支持：
 *   - 加载历史记录
 *   - 导出 CSV
 *   - 删除单条记录
 *   - 由 DBWorker 线程异步驱动
 */
class HistoryViewModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ProjectIdRole,
        ProjectNameRole,
        SampleNoRole,
        SampleSourceRole,
        SampleNameRole,
        StandardCurveRole,
        BatchCodeRole,
        DetectedConcRole,
        ReferenceValueRole,
        ResultRole,
        DetectedTimeRole,
        DetectedUnitRole,
        DetectedPersonRole,
        DilutionInfoRole
    };

    explicit HistoryViewModel(DBWorker* worker = nullptr, QObject* parent = nullptr);
    ~HistoryViewModel() override = default;

    // === QAbstractListModel 接口 ===
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // === QML 可调用接口 ===
    Q_INVOKABLE void refresh();                                 // 刷新历史记录
    Q_INVOKABLE bool exportCsv(const QString& filePath) const;  // 导出 CSV 文件
    Q_INVOKABLE bool deleteById(int id);                        // 按 id 删除记录
    Q_INVOKABLE QVariantMap getById(int id) const;              // ✅ 新增：QML可调用
    Q_INVOKABLE QVariantMap getRow(int row) const;

signals:
    void countChanged();

private:
    DBWorker* m_worker = nullptr;
    QVector<HistoryRow> m_rows;
};

#endif  // HISTORYVIEWMODEL_H
