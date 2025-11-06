#include "UsersRepo.h"

#include <QDebug>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "SqlUtil.h"

static QString roleToName(const QString& role) {
    if (role == "admin")
        return QStringLiteral("管理员");
    if (role == "engineer")
        return QStringLiteral("工程师");
    if (role == "operator")
        return QStringLiteral("操作员");
    return role;
}

// users(username UNIQUE, role, salt_hex, pass_hash_hex, active, note)
bool UsersRepo::auth(QSqlDatabase db,
                     const QString& username,
                     const QString& password,
                     QString& outRole) {
    if (!db.isValid() || !db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(
        "SELECT role, salt_hex, pass_hash_hex, active "
        "FROM users WHERE username = :u LIMIT 1");
    q.bindValue(":u", username);
    if (!q.exec()) {
        qWarning() << "[UsersRepo] auth query failed:" << q.lastError();
        return false;
    }
    if (!q.next())
        return false;

    const QString role = q.value(0).toString();
    const QString salt = q.value(1).toString();
    const QString hashDb = q.value(2).toString();
    const bool active = q.value(3).toBool();
    if (!active)
        return false;

    const QString hashTry = hashPassword(salt, password);
    if (hashTry.compare(hashDb, Qt::CaseInsensitive) == 0) {
        outRole = role;
        return true;
    }
    return false;
}

bool UsersRepo::selectAll(QSqlDatabase db, QVector<UserRow>& out) {
    out.clear();
    if (!db.isValid() || !db.isOpen())
        return false;

    QSqlQuery q(db);
    if (!q.exec("SELECT id, username, role, salt_hex, pass_hash_hex, active, note "
                "FROM users ORDER BY id ASC")) {
        qWarning() << "[UsersRepo] selectAll failed:" << q.lastError();
        return false;
    }
    while (q.next()) {
        UserRow r;
        r.id = q.value(0).toInt();
        r.username = q.value(1).toString();
        r.role = q.value(2).toString();
        r.roleName = roleToName(r.role);
        r.saltHex = q.value(3).toString();
        r.passHashHex = q.value(4).toString();
        r.active = q.value(5).toBool();
        r.note = q.value(6).toString();
        out.push_back(r);
    }
    return true;
}

bool UsersRepo::addUser(QSqlDatabase db,
                        const QString& username,
                        const QString& password,
                        const QString& role,
                        const QString& note) {
    if (!db.isValid() || !db.isOpen())
        return false;
    // 已存在？
    {
        QSqlQuery q(db);
        q.prepare("SELECT id FROM users WHERE username=:u LIMIT 1");
        q.bindValue(":u", username);
        if (!q.exec())
            return false;
        if (q.next()) {
            qWarning() << "[UsersRepo] addUser: username exists";
            return false;
        }
    }

    const QString salt = genSaltHex();
    const QString hash = hashPassword(salt, password);

    QSqlQuery ins(db);
    ins.prepare(
        "INSERT INTO users(username, role, salt_hex, pass_hash_hex, active, note) "
        "VALUES(:u,:r,:s,:h,1,:n)");
    ins.bindValue(":u", username);
    ins.bindValue(":r", role);
    ins.bindValue(":s", salt);
    ins.bindValue(":h", hash);
    ins.bindValue(":n", note);
    if (!ins.exec()) {
        qWarning() << "[UsersRepo] addUser failed:" << ins.lastError();
        return false;
    }
    return true;
}

bool UsersRepo::deleteUser(QSqlDatabase db, const QString& username) {
    if (!db.isValid() || !db.isOpen())
        return false;
    QSqlQuery q(db);
    q.prepare("DELETE FROM users WHERE username=:u");
    q.bindValue(":u", username);
    if (!q.exec()) {
        qWarning() << "[UsersRepo] deleteUser failed:" << q.lastError();
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool UsersRepo::resetPassword(QSqlDatabase db,
                              const QString& username,
                              const QString& newPassword) {
    if (!db.isValid() || !db.isOpen())
        return false;

    const QString salt = genSaltHex();
    const QString hash = hashPassword(salt, newPassword);

    QSqlQuery q(db);
    q.prepare("UPDATE users SET salt_hex=:s, pass_hash_hex=:h WHERE username=:u");
    q.bindValue(":s", salt);
    q.bindValue(":h", hash);
    q.bindValue(":u", username);
    if (!q.exec()) {
        qWarning() << "[UsersRepo] resetPassword failed:" << q.lastError();
        return false;
    }
    return q.numRowsAffected() > 0;
}
