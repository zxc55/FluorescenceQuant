#pragma once
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantMap>
#include <QVector>

#include "DTO.h"
// =======================
// 历史记录表（project_info）操作类
// =======================
class HistoryRepo {
public:
    // 插入一条历史记录
    static bool insert(QSqlDatabase& db, const HistoryRow& row);

    // 查询全部历史记录
    static bool selectAll(QSqlDatabase& db, QVector<HistoryRow>& outRows);
};
