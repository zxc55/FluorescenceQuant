#include "ProjectsRepo.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ProjectsRepo {

static inline void logSqlError(const char* tag, const QSqlQuery& q) {
    qWarning() << "[ProjectsRepo]" << tag << "error:"
               << q.lastError().text();
}

bool selectAll(QSqlDatabase& db, QVector<ProjectRow>& rows) {
    rows.clear();
    if (!db.isOpen()) {
        qWarning() << "[ProjectsRepo] selectAll fail: db not open";
        return false;
    }

    // 列名要与你的表结构一致：id / name / batch / updated_at
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
        r.updatedAt = q.value(3).toString();  // 直接拿字符串给 QML 显示
        rows.push_back(r);
    }

    return true;
}

bool deleteById(QSqlDatabase& db, int id) {
    if (!db.isOpen()) {
        qWarning() << "[ProjectsRepo] deleteById fail: db not open";
        return false;
    }
    QSqlQuery q(db);
    q.prepare("DELETE FROM projects WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        logSqlError("deleteById", q);
        return false;
    }
    // 影响行数>0 认为删除成功
    return (q.numRowsAffected() > 0);
}
bool insertProjectInfo(QSqlDatabase& db, const QVariantMap& data) {
    if (!db.isOpen()) {
        qWarning() << "[ProjectsRepo] insertProjectInfo fail: db not open";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(R"SQL(
        INSERT INTO project_info (
            project_id,
            sample_no,
            sample_source,
            sample_name,
            standard_curve,
            batch_code,
            detected_conc,
            reference_value,
            result,
            detected_time,
            detected_unit,
            detected_person,
            dilution_info
        ) VALUES (
            :project_id,
            :sample_no,
            :sample_source,
            :sample_name,
            :standard_curve,
            :batch_code,
            :detected_conc,
            :reference_value,
            :result,
            :detected_time,
            :detected_unit,
            :detected_person,
            :dilution_info
        );
    )SQL");

    // 一一绑定参数
    q.bindValue(":project_id", data.value("project_id"));
    q.bindValue(":sample_no", data.value("sample_no"));
    q.bindValue(":sample_source", data.value("sample_source"));
    q.bindValue(":sample_name", data.value("sample_name"));
    q.bindValue(":standard_curve", data.value("standard_curve"));
    q.bindValue(":batch_code", data.value("batch_code"));
    q.bindValue(":detected_conc", data.value("detected_conc"));
    q.bindValue(":reference_value", data.value("reference_value"));
    q.bindValue(":result", data.value("result"));
    q.bindValue(":detected_time", data.value("detected_time"));
    q.bindValue(":detected_unit", data.value("detected_unit"));
    q.bindValue(":detected_person", data.value("detected_person"));
    q.bindValue(":dilution_info", data.value("dilution_info"));

    if (!q.exec()) {
        logSqlError("insertProjectInfo", q);
        return false;
    }

    qInfo() << "[ProjectsRepo] 插入 project_info 成功";
    return true;
}
}  // namespace ProjectsRepo
