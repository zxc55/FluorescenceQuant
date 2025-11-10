#pragma once
#include <QAbstractListModel>
#include <QVector>

#include "DBWorker.h"
#include "DTO.h"

class HistoryViewModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ProjectIdRole,
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
    Q_ENUM(Role)

    explicit HistoryViewModel(DBWorker* worker = nullptr, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

signals:
    void countChanged();

private:
    DBWorker* m_worker = nullptr;
    QVector<HistoryRow> m_rows;
};
