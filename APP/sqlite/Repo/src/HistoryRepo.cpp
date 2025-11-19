#include "HistoryRepo.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// =============================
// 查询全部历史记录
// =============================
bool HistoryRepo::selectAll(QSqlDatabase db, QVector<HistoryRow>& out) {
    QSqlQuery q(db);
    if (!q.exec(R"(
        SELECT id, projectId, projectName,    -- ★ 新增字段
               sampleNo, sampleSource, sampleName,
               standardCurve, batchCode, detectedConc,
               referenceValue, result, detectedTime,
               detectedUnit, detectedPerson, dilutionInfo
        FROM project_info ORDER BY id DESC
    )")) {
        qWarning() << "HistoryRepo::selectAll failed:" << q.lastError().text();
        return false;
    }

    while (q.next()) {
        HistoryRow r;
        r.id = q.value(0).toInt();
        r.projectId = q.value(1).toInt();
        r.projectName = q.value(2).toString();  // ★ 新字段

        r.sampleNo = q.value(3).toString();
        r.sampleSource = q.value(4).toString();
        r.sampleName = q.value(5).toString();
        r.standardCurve = q.value(6).toString();
        r.batchCode = q.value(7).toString();
        r.detectedConc = q.value(8).toDouble();
        r.referenceValue = q.value(9).toDouble();
        r.result = q.value(10).toString();
        r.detectedTime = q.value(11).toString();
        r.detectedUnit = q.value(12).toString();
        r.detectedPerson = q.value(13).toString();
        r.dilutionInfo = q.value(14).toString();

        out.append(r);
    }
    return true;
}

// =============================
// 插入历史记录
// =============================
bool HistoryRepo::insert(QSqlDatabase db, const HistoryRow& row) {
    QSqlQuery q(db);

    q.prepare(R"SQL(
INSERT INTO project_info (
    projectId, projectName, sampleNo, sampleSource, sampleName,
    standardCurve, batchCode, detectedConc,
    referenceValue, result, detectedTime,
    detectedUnit, detectedPerson, dilutionInfo
) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?);
)SQL");

    q.addBindValue(row.projectId);
    q.addBindValue(row.projectName);  // ★ 新增字段绑定
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

// =============================
// 删除历史记录（含 adc_data）
// =============================
bool HistoryRepo::deleteById(QSqlDatabase db, int id) {
    QString sampleNo;

    // 1️⃣ 先查 sampleNo
    {
        QSqlQuery q(db);
        q.prepare("SELECT sampleNo FROM project_info WHERE id = ?");
        q.addBindValue(id);

        if (!q.exec() || !q.next()) {
            qWarning() << "❌ deleteById: 找不到 id =" << id;
            return false;
        }
        sampleNo = q.value(0).toString();
    }

    // 2️⃣ 删 project_info
    {
        QSqlQuery q(db);
        q.prepare("DELETE FROM project_info WHERE id = ?");
        q.addBindValue(id);

        if (!q.exec()) {
            qWarning() << "❌ 删除 project_info 失败:" << q.lastError().text();
            return false;
        }
    }

    // 3️⃣ 删 adc_data（按 sampleNo）
    {
        QSqlQuery q(db);
        q.prepare("DELETE FROM adc_data WHERE sampleNo = ?");
        q.addBindValue(sampleNo);

        if (!q.exec()) {
            qWarning() << "❌ 删除 adc_data 失败:" << q.lastError().text();
            return false;
        }
    }

    qInfo() << "🗑 删除成功 → id =" << id
            << ", sampleNo =" << sampleNo
            << "（project_info + adc_data 已全部清理）";

    return true;
}
