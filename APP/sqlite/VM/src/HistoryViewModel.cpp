#include "HistoryViewModel.h"

#include <QDebug>

HistoryViewModel::HistoryViewModel(DBWorker* worker, QObject* parent)
    : QAbstractListModel(parent), m_worker(worker) {
    if (m_worker) {
        connect(m_worker, &DBWorker::historyLoaded, this, [this](const QVector<HistoryRow>& rows) {
            qDebug() << "[HistoryViewModel] got rows =" << rows.size();
            beginResetModel();
            m_rows = rows;
            endResetModel();
            emit countChanged();
        });
    }
}

void HistoryViewModel::refresh() {
    if (!m_worker) {
        qWarning() << "[HistoryViewModel] No DBWorker";
        return;
    }
    m_worker->postLoadHistory();
}

int HistoryViewModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return m_rows.size();
}

QVariant HistoryViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};

    const auto& r = m_rows[index.row()];
    switch (role) {
    case IdRole:
        return r.id;
    case ProjectIdRole:
        return r.projectId;
    case SampleNoRole:
        return r.sampleNo;
    case SampleSourceRole:
        return r.sampleSource;
    case SampleNameRole:
        return r.sampleName;
    case StandardCurveRole:
        return r.standardCurve;
    case BatchCodeRole:
        return r.batchCode;
    case DetectedConcRole:
        return r.detectedConc;
    case ReferenceValueRole:
        return r.referenceValue;
    case ResultRole:
        return r.result;
    case DetectedTimeRole:
        return r.detectedTime;
    case DetectedUnitRole:
        return r.detectedUnit;
    case DetectedPersonRole:
        return r.detectedPerson;
    case DilutionInfoRole:
        return r.dilutionInfo;
    default:
        return {};
    }
}

QHash<int, QByteArray> HistoryViewModel::roleNames() const {
    return {
        {IdRole, "id"},
        {ProjectIdRole, "projectId"},
        {SampleNoRole, "sampleNo"},
        {SampleSourceRole, "sampleSource"},
        {SampleNameRole, "sampleName"},
        {StandardCurveRole, "standardCurve"},
        {BatchCodeRole, "batchCode"},
        {DetectedConcRole, "detectedConc"},
        {ReferenceValueRole, "referenceValue"},
        {ResultRole, "result"},
        {DetectedTimeRole, "detectedTime"},
        {DetectedUnitRole, "detectedUnit"},
        {DetectedPersonRole, "detectedPerson"},
        {DilutionInfoRole, "dilutionInfo"},
    };
}
