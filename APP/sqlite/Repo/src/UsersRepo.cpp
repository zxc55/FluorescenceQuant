#include "UsersRepo.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

// ========== 登录验证（明文密码版） ==========
bool UsersRepo::auth(QSqlDatabase db,
                     const QString& username,
                     const QString& password,
                     QString& outRole) {
    if (!db.isOpen()) {
        qWarning() << "[UsersRepo] db not open";
        return false;
    }

    QSqlQuery q(db);

    q.prepare(
        "SELECT role, password_hash "
        "FROM users WHERE username = :u LIMIT 1");
    q.bindValue(":u", username);

    if (!q.exec()) {
        qWarning() << "[UsersRepo] auth fail:" << q.lastError().text();
        return false;
    }

    if (!q.next()) {
        qWarning() << "[UsersRepo] 用户不存在, 输入用户名 =" << username;
        return false;
    }

    QString role = q.value(0).toString();
    QString passDb = q.value(1).toString();

    // // 打印调试信息
    // qDebug() << "========== 登录调试 ==========";
    // qDebug() << "输入用户名:" << username;
    // qDebug() << "输入密码  :" << password;
    // qDebug() << "数据库密码:" << passDb;
    // qDebug() << "================================";

    if (password == passDb) {
        outRole = role;
        return true;
    }

    //   qWarning() << "[UsersRepo] 密码不一致 → 登录失败";
    return false;
}

// ========== 查询所有用户 ==========
bool UsersRepo::selectAll(QSqlDatabase db, QVector<UserRow>& out) {
    out.clear();

    if (!db.isOpen())
        return false;

    QSqlQuery q(db);

    // 按你当前表结构，只返回以下字段
    if (!q.exec(
            "SELECT id, username, role, note "
            "FROM users ORDER BY id ASC")) {
        qWarning() << "[UsersRepo] selectAll failed:" << q.lastError().text();
        return false;
    }

    while (q.next()) {
        UserRow r;
        r.id = q.value(0).toInt();
        r.username = q.value(1).toString();
        r.role = q.value(2).toString();
        r.roleName = r.role;  // 你可以改成中文显示
        r.note = q.value(3).toString();
        out.push_back(r);
    }

    return true;
}
