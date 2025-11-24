#pragma once
#include <QString>
#include <QVector>
#include <QtSql/QSqlDatabase>

#include "DTO.h"

class UsersRepo {
public:
    // 登录验证
    static bool auth(QSqlDatabase db,
                     const QString& username,
                     const QString& password,
                     QString& outRole);

    // 读取全部用户（用于列表）
    static bool selectAll(QSqlDatabase db, QVector<UserRow>& out);
};
