#pragma once
#include <QSqlDatabase>

#include "DTO.h"

class SettingsRepo {
public:
    static bool selectOne(QSqlDatabase& db, AppSettingsRow& out);
    static bool updateOne(QSqlDatabase& db, const AppSettingsRow& s);
};
