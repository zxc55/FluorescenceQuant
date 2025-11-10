#include "HistoryRepo.h"

#include <QSqlError>

bool HistoryRepo::insert(QSqlDatabase& db, const HistoryRow& r) {
    if (!db.isOpen()) {
        qWarning() << "[HistoryRepo] DB not open";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(R"SQL(
        INSERT INTO project_info (
            project_id, sample_no, sample_source, sample_name,
            standard_curve, batch_code, detected_conc, reference_value,
            result, detected_time, detected_unit, detected_person, dilution_info
        ) VALUES (
            :project_id, :sample_no, :sample_source, :sample_name,
            :standard_curve, :batch_code, :detected_conc, :reference_value,
            :result, :detected_time, :detected_unit, :detected_person, :dilution_info
        )
    )SQL");

    q.bindValue(":project_id", r.projectId);
    q.bindValue(":sample_no", r.sampleNo);
    q.bindValue(":sample_source", r.sampleSource);
    q.bindValue(":sample_name", r.sampleName);
    q.bindValue(":standard_curve", r.standardCurve);
    q.bindValue(":batch_code", r.batchCode);
    q.bindValue(":detected_conc", r.detectedConc);
    q.bindValue(":reference_value", r.referenceValue);
    q.bindValue(":result", r.result);
    q.bindValue(":detected_time", r.detectedTime);
    q.bindValue(":detected_unit", r.detectedUnit);
    q.bindValue(":detected_person", r.detectedPerson);
    q.bindValue(":dilution_info", r.dilutionInfo);

    if (!q.exec()) {
        qWarning() << "[HistoryRepo] insert fail:" << q.lastError().text();
        return false;
    }

    qInfo() << "[HistoryRepo] insert ok, id=" << q.lastInsertId();
    return true;
}

bool HistoryRepo::selectAll(QSqlDatabase& db, QVector<HistoryRow>& outRows) {
    outRows.clear();
    if (!db.isOpen()) {
        qWarning() << "[HistoryRepo] DB not open";
        return false;
    }

    QSqlQuery q(db);
    if (!q.exec(R"SQL(
        SELECT id, project_id, sample_no, sample_source, sample_name,
               standard_curve, batch_code, detected_conc, reference_value,
               result, detected_time, detected_unit, detected_person, dilution_info
        FROM project_info ORDER BY id DESC
    )SQL")) {
        qWarning() << "[HistoryRepo] selectAll fail:" << q.lastError().text();
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
        outRows.push_back(r);
    }

    qInfo() << "[HistoryRepo] loaded" << outRows.size() << "rows.";
    return true;
}
