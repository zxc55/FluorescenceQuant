#include "Migrations.h"

#include <QDebug>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "SqlUtil.h"

static bool execSql(QSqlDatabase db, const char* sql) {
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning() << "[Migrations] SQL failed:" << q.lastError() << "\nSQL:" << sql;
        return false;
    }
    return true;
}

static bool applyV1(QSqlDatabase db) {
    // 基础表
    if (!execSql(db, R"SQL(
        CREATE TABLE IF NOT EXISTS app_settings(
            id INTEGER PRIMARY KEY CHECK(id=1),
            manufacturer TEXT DEFAULT '',
            engineer_mode INTEGER DEFAULT 0,
            auto_print INTEGER DEFAULT 1,
            backlight INTEGER DEFAULT 80,
            lang TEXT DEFAULT 'zh_CN',
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )SQL"))
        return false;

    if (!execSql(db, R"SQL(
        CREATE TABLE IF NOT EXISTS users(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            role TEXT NOT NULL,
            salt_hex TEXT NOT NULL,
            pass_hash_hex TEXT NOT NULL,
            active INTEGER DEFAULT 1,
            note TEXT DEFAULT ''
        );
    )SQL"))
        return false;

    // 初始化 app_settings id=1
    {
        QSqlQuery q(db);
        if (!q.exec("SELECT COUNT(*) FROM app_settings WHERE id=1"))
            return false;
        if (q.next() && q.value(0).toInt() == 0) {
            QSqlQuery ins(db);
            ins.prepare(
                "INSERT INTO app_settings(id, manufacturer, engineer_mode, auto_print, backlight, lang) "
                "VALUES(1,'Pribolab',0,1,80,'zh_CN')");
            if (!ins.exec()) {
                qWarning() << "[Migrations] insert app_settings failed:" << ins.lastError();
                return false;
            }
        }
    }

    // 初始化默认用户 admin/engineer/operator（密码 123456 / 234567 / 345678）
    auto ensureUser = [&](const QString& u, const QString& role, const QString& pass, const QString& note) -> bool {
        QSqlQuery q(db);
        q.prepare("SELECT id FROM users WHERE username=:u LIMIT 1");
        q.bindValue(":u", u);
        if (!q.exec())
            return false;
        if (q.next())
            return true;

        const QString salt = genSaltHex();
        const QString hash = hashPassword(salt, pass);
        QSqlQuery ins(db);
        ins.prepare(
            "INSERT INTO users(username, role, salt_hex, pass_hash_hex, active, note) "
            "VALUES(:u,:r,:s,:h,1,:n)");
        ins.bindValue(":u", u);
        ins.bindValue(":r", role);
        ins.bindValue(":s", salt);
        ins.bindValue(":h", hash);
        ins.bindValue(":n", note);
        if (!ins.exec()) {
            qWarning() << "[Migrations] insert user failed:" << ins.lastError();
            return false;
        }
        return true;
    };
    return ensureUser("admin", "admin", "123456", QStringLiteral("内置管理员")) && ensureUser("engineer", "engineer", "234567", QStringLiteral("工程师")) && ensureUser("operator", "operator", "345678", QStringLiteral("操作员"));
}

bool migrateAllToV1(QSqlDatabase db) { return applyV1(db); }
bool applyMigrations(QSqlDatabase db) { return applyV1(db); }
