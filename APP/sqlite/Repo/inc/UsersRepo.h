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

    // 新增用户
    static bool addUser(QSqlDatabase db,
                        const QString& username,
                        const QString& password,
                        const QString& role,
                        const QString& note);

    // 删除用户（按用户名）
    static bool deleteUser(QSqlDatabase db, const QString& username);

    // 重置密码
    static bool resetPassword(QSqlDatabase db,
                              const QString& username,
                              const QString& newPassword);
};
