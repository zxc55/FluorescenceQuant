#pragma once
#include <QtSql/QSqlDatabase>

#include "DTO.h"

class SettingsRepo {
public:
    static bool selectOne(QSqlDatabase db, AppSettingsRow& out);       // 读 id=1
    static bool updateOne(QSqlDatabase db, const AppSettingsRow& in);  // 写 id=1
};
