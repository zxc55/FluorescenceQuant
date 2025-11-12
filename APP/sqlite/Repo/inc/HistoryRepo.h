#pragma once
#include <QSqlDatabase>
#include <QVector>

#include "DTO.h"

class HistoryRepo {
public:
    static bool selectAll(QSqlDatabase db, QVector<HistoryRow>& out);  // 查询全部
    static bool insert(QSqlDatabase db, const HistoryRow& row);        // 插入一条
    static bool deleteById(QSqlDatabase db, int id);                   // 删除指定 ID
};
