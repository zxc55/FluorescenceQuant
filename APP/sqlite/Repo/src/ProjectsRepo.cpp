#include "ProjectsRepo.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ProjectsRepo {

static inline void logSqlError(const char* tag, const QSqlQuery& q) {
    qWarning() << "[ProjectsRepo]" << tag << "error:" << q.lastError().text();
}

bool selectAll(QSqlDatabase& db, QVector<ProjectRow>& rows) {
    rows.clear();
    if (!db.isOpen()) {
        qWarning() << "[ProjectsRepo] selectAll: db not open";
        return false;
    }
    QSqlQuery q(db);
    if (!q.exec("SELECT id, name, batch, updated_at FROM projects ORDER BY id ASC")) {
        logSqlError("selectAll", q);
        return false;
    }
    while (q.next()) {
        ProjectRow r;
        r.id = q.value(0).toInt();
        r.name = q.value(1).toString();
        r.batch = q.value(2).toString();
        r.updatedAt = q.value(3).toString();
        rows.push_back(r);
    }
    return true;
}

bool deleteById(QSqlDatabase& db, int id) {
    if (!db.isOpen()) {
        qWarning() << "[ProjectsRepo] deleteById: db not open";
        return false;
    }
    QSqlQuery q(db);
    q.prepare("DELETE FROM projects WHERE id=?");
    q.addBindValue(id);
    if (!q.exec()) {
        logSqlError("deleteById", q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

static QVariant pick(const QVariantMap& m, const char* camel, const char* snake) {
    auto it = m.find(camel);
    if (it != m.end())
        return it.value();
    it = m.find(snake);
    if (it != m.end())
        return it.value();
    return QVariant();
}
bool insertProjectInfo(QSqlDatabase& db, const QVariantMap& data) {
    if (!db.isOpen()) {
        qWarning() << "[ProjectsRepo] insertProjectInfo: db not open";
        return false;
    }

    // 注意：C / T 用双引号，避免列名歧义
    const char* sql =
        "INSERT INTO project_info ("
        " projectId, projectName, sampleNo, sampleSource, sampleName, standardCurve,"
        " batchCode, detectedConc, referenceValue, result, detectedTime,"
        " detectedUnit, detectedPerson, dilutionInfo,"
        " \"C\", \"T\", ratio"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        logSqlError("prepare(insertProjectInfo)", q);
        return false;
    }

    // ===== 原有 14 个字段 =====
    q.addBindValue(data.value("projectId"));
    q.addBindValue(data.value("projectName"));
    q.addBindValue(data.value("sampleNo"));
    q.addBindValue(data.value("sampleSource"));
    q.addBindValue(data.value("sampleName"));
    q.addBindValue(data.value("standardCurve"));
    q.addBindValue(data.value("batchCode"));
    q.addBindValue(data.value("detectedConc"));
    q.addBindValue(data.value("referenceValue"));
    q.addBindValue(data.value("result"));
    q.addBindValue(data.value("detectedTime"));
    q.addBindValue(data.value("detectedUnit"));
    q.addBindValue(data.value("detectedPerson"));
    q.addBindValue(data.value("dilutionInfo"));

    // ===== 新增 3 个字段：C_net / T_net / ratio =====
    const double c = data.value("C", 0.0).toDouble();
    const double t = data.value("T", 0.0).toDouble();
    const double r = data.value("ratio", 0.0).toDouble();
    q.addBindValue(c);
    q.addBindValue(t);
    q.addBindValue(r);

    qInfo() << "[ProjectsRepo][DEBUG] bound fields =" << q.boundValues().size();  // 应该是 17

    if (!q.exec()) {
        logSqlError("exec(insertProjectInfo)", q);
        return false;
    }

    qInfo() << "✅ 插入 project_info 成功（含 C/T/ratio）"
            << "C=" << c << "T=" << t << "ratio=" << r;
    return true;
}

}  // namespace ProjectsRepo
