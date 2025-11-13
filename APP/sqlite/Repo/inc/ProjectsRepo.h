#pragma once
#include <QSqlDatabase>
#include <QVariantMap>
#include <QVector>

#include "DTO.h"

namespace ProjectsRepo {

bool selectAll(QSqlDatabase& db, QVector<ProjectRow>& rows);
bool deleteById(QSqlDatabase& db, int id);

// 统一入口：位置占位符 13 个，兼容 snake/camel 列名
bool insertProjectInfo(QSqlDatabase& db, const QVariantMap& data);

}  // namespace ProjectsRepo
