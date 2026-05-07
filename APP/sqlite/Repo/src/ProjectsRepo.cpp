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

static QString textOrDefault(const QVariantMap& m, const char* key,
                             const QString& fallback = QString()) {
    const QVariant v = m.value(key);
    if (!v.isValid() || v.isNull())
        return fallback;
    return v.toString();
}

static int intOrDefault(const QVariantMap& m, const char* key, int fallback = 0) {
    const QVariant v = m.value(key);
    if (!v.isValid() || v.isNull())
        return fallback;
    return v.toInt();
}

static double doubleOrDefault(const QVariantMap& m, const char* key,
                              double fallback = 0.0) {
    const QVariant v = m.value(key);
    if (!v.isValid() || v.isNull())
        return fallback;
    return v.toDouble();
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
    q.addBindValue(intOrDefault(data, "projectId"));
    q.addBindValue(textOrDefault(data, "projectName"));
    q.addBindValue(textOrDefault(data, "sampleNo"));
    q.addBindValue(textOrDefault(data, "sampleSource"));
    q.addBindValue(textOrDefault(data, "sampleName"));
    q.addBindValue(textOrDefault(data, "standardCurve"));
    q.addBindValue(textOrDefault(data, "batchCode"));
    q.addBindValue(doubleOrDefault(data, "detectedConc"));
    q.addBindValue(doubleOrDefault(data, "referenceValue"));
    q.addBindValue(textOrDefault(data, "result"));
    q.addBindValue(textOrDefault(data, "detectedTime"));
    q.addBindValue(textOrDefault(data, "detectedUnit", QStringLiteral("μg/kg")));
    q.addBindValue(textOrDefault(data, "detectedPerson"));
    q.addBindValue(textOrDefault(data, "dilutionInfo", QStringLiteral("1倍")));

    // ===== 新增 3 个字段：C_net / T_net / ratio =====
    const double c = doubleOrDefault(data, "C");
    const double t = doubleOrDefault(data, "T");
    const double r = doubleOrDefault(data, "ratio");
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
