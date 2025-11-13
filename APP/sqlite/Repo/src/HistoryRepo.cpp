#include "HistoryRepo.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool HistoryRepo::selectAll(QSqlDatabase db, QVector<HistoryRow>& out) {
    QSqlQuery q(db);
    if (!q.exec(R"(
        SELECT id, projectId, sampleNo, sampleSource, sampleName, standardCurve,
               batchCode, detectedConc, referenceValue, result,
               detectedTime, detectedUnit, detectedPerson, dilutionInfo
        FROM project_info ORDER BY id DESC
    )")) {
        qWarning() << "HistoryRepo::selectAll failed:" << q.lastError().text();
        return false;
    }

    while (q.next()) {
        HistoryRow r;
        r.id = q.value(0).toInt();
        r.projectId = q.value(1).toInt();
        r.sampleNo = q.value(2).toString();
        r.sampleSource = q.value(3).toString();
        r.sampleName = q.value(4).toString();
        r.standardCurve = q.value(5).toString();
        r.batchCode = q.value(6).toString();
        r.detectedConc = q.value(7).toDouble();
        r.referenceValue = q.value(8).toDouble();
        r.result = q.value(9).toString();
        r.detectedTime = q.value(10).toString();
        r.detectedUnit = q.value(11).toString();
        r.detectedPerson = q.value(12).toString();
        r.dilutionInfo = q.value(13).toString();
        out.append(r);
    }
    return true;
}

bool HistoryRepo::insert(QSqlDatabase db, const HistoryRow& row) {
    QSqlQuery q(db);
    q.prepare(R"SQL(
INSERT INTO project_info (
    projectId, sampleNo, sampleSource, sampleName,
    standardCurve, batchCode, detectedConc,
    referenceValue, result, detectedTime,
    detectedUnit, detectedPerson, dilutionInfo
) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?);
)SQL");

    q.addBindValue(row.projectId);
    q.addBindValue(row.sampleNo);
    q.addBindValue(row.sampleSource);
    q.addBindValue(row.sampleName);
    q.addBindValue(row.standardCurve);
    q.addBindValue(row.batchCode);
    q.addBindValue(row.detectedConc);
    q.addBindValue(row.referenceValue);
    q.addBindValue(row.result);
    q.addBindValue(row.detectedTime);
    q.addBindValue(row.detectedUnit);
    q.addBindValue(row.detectedPerson);
    q.addBindValue(row.dilutionInfo);

    if (!q.exec()) {
        qWarning() << "HistoryRepo::insert failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool HistoryRepo::deleteById(QSqlDatabase db, int id) {
    QSqlQuery q(db);
    q.prepare("DELETE FROM project_info WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "HistoryRepo::deleteById failed:" << q.lastError().text();
        return false;
    }
    return true;
}
