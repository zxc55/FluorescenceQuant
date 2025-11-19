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

    // 选择风格（你的项目本来就是 camelCase）
    const char* sql =
        "INSERT INTO project_info ("
        " projectId, projectName, sampleNo, sampleSource, sampleName, standardCurve,"
        " batchCode, detectedConc, referenceValue, result, detectedTime,"
        " detectedUnit, detectedPerson, dilutionInfo"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        logSqlError("prepare(insertProjectInfo)", q);
        return false;
    }

    q.addBindValue(data.value("projectId"));
    q.addBindValue(data.value("projectName"));  // ★ 新增写入项目名称
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

    qInfo() << "[ProjectsRepo][DEBUG] bound fields = " << q.boundValues().size();

    if (!q.exec()) {
        logSqlError("exec(insertProjectInfo)", q);
        return false;
    }

    qInfo() << "✅ 插入 project_info 成功（已写入 projectName）";
    return true;
}

}  // namespace ProjectsRepo
