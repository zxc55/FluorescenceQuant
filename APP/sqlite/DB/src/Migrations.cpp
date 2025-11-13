#include "Migrations.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static bool execOne(QSqlQuery& q, const QString& sql) {
    if (!q.exec(sql)) {
        qWarning() << "[MIGRATE] SQL fail:" << q.lastError().text();
        qWarning() << "SQL:" << sql;
        return false;
    }
    return true;
}

static bool columnExists(QSqlDatabase db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA table_info(%1);").arg(table));
    while (q.next()) {
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool migrateAllToV1(QSqlDatabase db) {
    if (!db.isOpen()) {
        qWarning() << "[MIGRATE] db not open";
        return false;
    }

    QSqlQuery q(db);

    // === app_settings ===
    execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS app_settings(
    id                  INTEGER PRIMARY KEY CHECK(id=1),
    manufacturer_name   TEXT    NOT NULL DEFAULT '',
    backlight           INTEGER NOT NULL DEFAULT 50,
    language            TEXT    NOT NULL DEFAULT 'zh_CN',
    auto_update         INTEGER NOT NULL DEFAULT 0,
    engineer_mode       INTEGER NOT NULL DEFAULT 0
);
)SQL");

    execOne(q, R"SQL(
INSERT OR IGNORE INTO app_settings(id, manufacturer_name)
VALUES(1, '');
)SQL");

    // === users ===
    execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS users(
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL DEFAULT '',
    salt          TEXT NOT NULL DEFAULT '',
    role          TEXT NOT NULL DEFAULT 'operator',
    note          TEXT NOT NULL DEFAULT ''
);
)SQL");

    execOne(q, R"SQL(
INSERT OR IGNORE INTO users(username,password_hash,salt,role,note)
VALUES ('admin','123456','salt','admin','内置管理员'),
       ('eng','654321','salt','engineer','内置工程师'),
       ('op','111111','salt','operator','内置操作员');
)SQL");

    // === projects ===
    execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS projects(
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT NOT NULL,
    batch      TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
)SQL");

    execOne(q, R"SQL(
INSERT INTO projects(name,batch,updated_at)
SELECT '黄曲霉毒素','B2025-001',datetime('now','localtime')
WHERE NOT EXISTS (SELECT 1 FROM projects);
)SQL");

    execOne(q, R"SQL(
INSERT INTO projects(name,batch,updated_at)
SELECT '呕吐毒素','B2025-002',datetime('now','localtime')
WHERE (SELECT COUNT(1) FROM projects)=1;
)SQL");

    execOne(q, R"SQL(
INSERT INTO projects(name,batch,updated_at)
SELECT '玉米赤霉烯酮','B2025-003',datetime('now','localtime')
WHERE (SELECT COUNT(1) FROM projects)=2;
)SQL");

    // === project_info（新版驼峰字段）===
    execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS project_info(
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    projectId       INTEGER NOT NULL,
    sampleNo        TEXT NOT NULL DEFAULT '',
    sampleSource    TEXT NOT NULL DEFAULT '',
    sampleName      TEXT NOT NULL DEFAULT '',
    standardCurve   TEXT NOT NULL DEFAULT '',
    batchCode       TEXT NOT NULL DEFAULT '',
    detectedConc    REAL NOT NULL DEFAULT 0.0,
    referenceValue  REAL NOT NULL DEFAULT 0.0,
    result          TEXT NOT NULL DEFAULT '',
    detectedTime    TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    detectedUnit    TEXT NOT NULL DEFAULT 'μg/kg',
    detectedPerson  TEXT NOT NULL DEFAULT '',
    dilutionInfo    TEXT NOT NULL DEFAULT '1倍',
    FOREIGN KEY(projectId) REFERENCES projects(id) ON DELETE CASCADE
);
)SQL");

    // ===adc_data 表，用于保存 ADS1115 实时采样结果 ===
    execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS adc_data(
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    sampleNo   TEXT NOT NULL,
    timestamp  TEXT NOT NULL,
    adcValues  TEXT NOT NULL,
    avgValue   REAL NOT NULL DEFAULT 0.0
);
)SQL");

    qInfo() << "[MIGRATE] v1 done ✅";
    return true;
}
