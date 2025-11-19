#include "HistoryViewModel.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include "HistoryRepo.h"

HistoryViewModel::HistoryViewModel(DBWorker* worker, QObject* parent)
    : QAbstractListModel(parent), m_worker(worker) {
    if (m_worker) {
        // 加载完成信号
        connect(m_worker, &DBWorker::historyLoaded,
                this, [this](const QVector<HistoryRow>& rows) {
                    beginResetModel();
                    m_rows = rows;
                    endResetModel();
                    emit countChanged();
                    qInfo() << "[HistoryViewModel] got rows =" << m_rows.size();
                });

        // 删除完成信号
        connect(m_worker, &DBWorker::historyDeleted,
                this, [this](bool ok, int id) {
                    qInfo() << "[HistoryViewModel] historyDeleted id =" << id << ", ok =" << ok;
                    if (ok)
                        refresh();
                });
    }
}

// === 模型接口 ===
int HistoryViewModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return m_rows.size();
}

QVariant HistoryViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();

    const HistoryRow& r = m_rows.at(index.row());

    switch (role) {
    case IdRole:
        return r.id;
    case ProjectIdRole:
        return r.projectId;
    case ProjectNameRole:
        return r.projectName;
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
        return QVariant();
    }
}

QHash<int, QByteArray> HistoryViewModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[ProjectIdRole] = "projectId";
    roles[ProjectNameRole] = "projectName";
    roles[SampleNoRole] = "sampleNo";
    roles[SampleSourceRole] = "sampleSource";
    roles[SampleNameRole] = "sampleName";
    roles[StandardCurveRole] = "standardCurve";
    roles[BatchCodeRole] = "batchCode";
    roles[DetectedConcRole] = "detectedConc";
    roles[ReferenceValueRole] = "referenceValue";
    roles[ResultRole] = "result";
    roles[DetectedTimeRole] = "detectedTime";
    roles[DetectedUnitRole] = "detectedUnit";
    roles[DetectedPersonRole] = "detectedPerson";
    roles[DilutionInfoRole] = "dilutionInfo";
    return roles;
}

// === 刷新 ===
void HistoryViewModel::refresh() {
    if (!m_worker) {
        qWarning() << "[HistoryViewModel] refresh: worker null";
        return;
    }
    m_worker->postLoadHistory();
}

// === 导出 CSV ===
bool HistoryViewModel::exportCsv(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[HistoryViewModel] exportCsv open fail:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    // 表头
    out << "ID,项目ID,样品编号,样品来源,样品名称,标准曲线,批次号,检测浓度,参考值,结果,检测时间,单位,检测人,稀释信息\n";

    // 写数据
    for (const auto& r : m_rows) {
        // QString::replace() 非 const，必须用临时变量
        QString sNo = r.sampleNo;
        sNo.replace('"', "\"\"");
        QString sSrc = r.sampleSource;
        sSrc.replace('"', "\"\"");
        QString sName = r.sampleName;
        sName.replace('"', "\"\"");
        QString sCurve = r.standardCurve;
        sCurve.replace('"', "\"\"");
        QString sBatch = r.batchCode;
        sBatch.replace('"', "\"\"");
        QString sRes = r.result;
        sRes.replace('"', "\"\"");
        QString sTime = r.detectedTime;
        sTime.replace('"', "\"\"");
        QString sUnit = r.detectedUnit;
        sUnit.replace('"', "\"\"");
        QString sPerson = r.detectedPerson;
        sPerson.replace('"', "\"\"");
        QString sDil = r.dilutionInfo;
        sDil.replace('"', "\"\"");

        out << r.id << ','
            << r.projectId << ','
            << '"' << sNo << '"' << ','
            << '"' << sSrc << '"' << ','
            << '"' << sName << '"' << ','
            << '"' << sCurve << '"' << ','
            << '"' << sBatch << '"' << ','
            << r.detectedConc << ','
            << r.referenceValue << ','
            << '"' << sRes << '"' << ','
            << '"' << sTime << '"' << ','
            << '"' << sUnit << '"' << ','
            << '"' << sPerson << '"' << ','
            << '"' << sDil << '"' << '\n';
    }

    file.close();
    qInfo() << "[HistoryViewModel] exportCsv success ->" << filePath;
    return true;
}

// === 删除一条记录 ===
bool HistoryViewModel::deleteById(int id) {
    if (!m_worker) {
        qWarning() << "[HistoryViewModel] deleteById: worker null";
        return false;
    }
    if (id <= 0) {
        qWarning() << "[HistoryViewModel] deleteById: invalid id" << id;
        return false;
    }
    m_worker->postDeleteHistory(id);
    return true;
}
// === QML 调用：根据 ID 获取详细信息 ===
QVariantMap HistoryViewModel::getById(int id) const {
    QVariantMap map;

    for (const auto& r : m_rows) {
        if (r.id == id) {
            map["id"] = r.id;
            map["projectId"] = r.projectId;
            map["projectName"] = r.projectName;
            map["sampleNo"] = r.sampleNo;
            map["sampleSource"] = r.sampleSource;
            map["sampleName"] = r.sampleName;
            map["standardCurve"] = r.standardCurve;
            map["batchCode"] = r.batchCode;
            map["detectedConc"] = r.detectedConc;
            map["referenceValue"] = r.referenceValue;
            map["result"] = r.result;
            map["detectedTime"] = r.detectedTime;
            map["detectedUnit"] = r.detectedUnit;
            map["detectedPerson"] = r.detectedPerson;
            map["dilutionInfo"] = r.dilutionInfo;
            break;
        }
    }

    if (map.isEmpty())
        qWarning() << "[HistoryViewModel] getById: not found id=" << id;

    return map;
}
