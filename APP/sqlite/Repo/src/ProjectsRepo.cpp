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

}  // namespace ProjectsRepo
