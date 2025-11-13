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

    // 探测列名风格
    bool snake = true;  // 默认 snake
    {
        QSqlQuery qi(db);
        if (qi.exec("PRAGMA table_info(project_info);")) {
            bool hasCamel = false, hasSnake = false;
            while (qi.next()) {
                const QString name = qi.value(1).toString();
                if (name == "projectId" || name == "sampleNo")
                    hasCamel = true;
                if (name == "project_id" || name == "sample_no")
                    hasSnake = true;
            }
            snake = hasSnake && !hasCamel;
        } else {
            qWarning() << "[ProjectsRepo] PRAGMA table_info fail:" << qi.lastError();
        }
    }

    const char* sql =
        snake ?
              // snake_case
            "INSERT INTO project_info ("
            " project_id, sample_no, sample_source, sample_name, standard_curve,"
            " batch_code, detected_conc, reference_value, result, detected_time,"
            " detected_unit, detected_person, dilution_info"
            ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"
              :
              // camelCase
            "INSERT INTO project_info ("
            " projectId, sampleNo, sampleSource, sampleName, standardCurve,"
            " batchCode, detectedConc, referenceValue, result, detectedTime,"
            " detectedUnit, detectedPerson, dilutionInfo"
            ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)";

    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        logSqlError("prepare(insertProjectInfo)", q);
        return false;
    }

    // 严格绑定 13 个
    q.addBindValue(pick(data, "projectId", "project_id"));
    q.addBindValue(pick(data, "sampleNo", "sample_no"));
    q.addBindValue(pick(data, "sampleSource", "sample_source"));
    q.addBindValue(pick(data, "sampleName", "sample_name"));
    q.addBindValue(pick(data, "standardCurve", "standard_curve"));
    q.addBindValue(pick(data, "batchCode", "batch_code"));
    q.addBindValue(pick(data, "detectedConc", "detected_conc").toDouble());
    q.addBindValue(pick(data, "referenceValue", "reference_value").toDouble());
    q.addBindValue(pick(data, "result", "result"));
    q.addBindValue(pick(data, "detectedTime", "detected_time"));
    q.addBindValue(pick(data, "detectedUnit", "detected_unit"));
    q.addBindValue(pick(data, "detectedPerson", "detected_person"));
    q.addBindValue(pick(data, "dilutionInfo", "dilution_info"));

    qInfo() << "[ProjectsRepo][DEBUG] placeholders=13 bound=" << q.boundValues().size()
            << (snake ? "(snake_case)" : "(camelCase)");

    if (!q.exec()) {
        logSqlError("exec(insertProjectInfo)", q);
        return false;
    }
    qInfo() << "✅ 插入 project_info 成功";
    return true;
}

}  // namespace ProjectsRepo
