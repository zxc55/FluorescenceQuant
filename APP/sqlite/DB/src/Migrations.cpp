#include "Migrations.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// 执行 SQL 并打印错误
static bool execOne(QSqlQuery& q, const QString& sql) {
    if (!q.exec(sql)) {
        qWarning() << "[MIGRATE] SQL fail:" << q.lastError().text();
        qWarning() << "SQL:" << sql;
        return false;
    }
    return true;
}

// 执行 SQL（忽略错误，用于 ADD COLUMN）
static bool execIgnore(QSqlQuery& q, const QString& sql) {
    q.exec(sql);
    return true;
}

// =========================
//  project_info 结构修复
// =========================
static bool migrateProjectInfo(QSqlDatabase& db) {
    QSqlQuery q(db);

    q.exec("PRAGMA table_info(project_info)");
    bool needMigrate = false;
    bool hasProjectName = false;
    bool batchDefaultWrong = false;

    while (q.next()) {
        QString col = q.value(1).toString();
        QString dflt = q.value(4).toString();

        if (col == "projectName")
            hasProjectName = true;

        if (col == "batchCode") {
            if (dflt != "''" && dflt != "")
                batchDefaultWrong = true;
        }
    }

    needMigrate = (!hasProjectName || batchDefaultWrong);

    if (!needMigrate) {
        qInfo() << "[MIGRATE] project_info 表结构正常，无需迁移";
        return true;
    }

    qWarning() << "⚠️ project_info 表结构为旧版，需要迁移修复";

    // 1) 旧表改名
    execOne(q, "ALTER TABLE project_info RENAME TO project_info_old;");

    // 2) 创建新表
    execOne(q, R"SQL(
CREATE TABLE project_info(
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    projectId       INTEGER NOT NULL,
    projectName     TEXT NOT NULL DEFAULT '',
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

    // 3) 迁移旧表数据
    execOne(q, R"SQL(
INSERT INTO project_info(
    id, projectId, sampleNo, sampleSource, sampleName, standardCurve,
    batchCode, detectedConc, referenceValue, result, detectedTime,
    detectedUnit, detectedPerson, dilutionInfo
)
SELECT
    id, projectId, sampleNo, sampleSource, sampleName, standardCurve,
    batchCode, detectedConc, referenceValue, result, detectedTime,
    detectedUnit, detectedPerson, dilutionInfo
FROM project_info_old;
)SQL");

    // 4) 删除旧表
    execOne(q, "DROP TABLE project_info_old;");

    qInfo() << "✅ project_info 表结构迁移完成";
    return true;
}

// =========================
//  主迁移入口
// =========================
bool migrateAllToV1(QSqlDatabase db) {
    if (!db.isOpen()) {
        qWarning() << "[MIGRATE] db not open";
        return false;
    }

    QSqlQuery q(db);

    // ===== app_settings =====
    execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS app_settings(
    id                  INTEGER PRIMARY KEY CHECK(id=1),
    manufacturer_name   TEXT    NOT NULL DEFAULT '',
    backlight           INTEGER NOT NULL DEFAULT 50,
    language            TEXT    NOT NULL DEFAULT 'zh_CN',
    auto_update         INTEGER NOT NULL DEFAULT 0,
    engineer_mode       INTEGER NOT NULL DEFAULT 0,

    -- === 功能设置（旧字段） ===
    auto_print          INTEGER NOT NULL DEFAULT 0,
    auto_upload         INTEGER NOT NULL DEFAULT 0,
    auto_id_gen         INTEGER NOT NULL DEFAULT 0,
    micro_switch        INTEGER NOT NULL DEFAULT 0,
    manufacturer_print  INTEGER NOT NULL DEFAULT 0,
        -- 新增字段（编号持久化）
    last_sample_date    TEXT NOT NULL DEFAULT '',
    last_sample_index   INTEGER NOT NULL DEFAULT 0
);
)SQL");

    execOne(q, R"SQL(
INSERT OR IGNORE INTO app_settings(id, manufacturer_name)
VALUES(1, '');
)SQL");

    // ★★★★★ 新增字段 auto_id_gen（如果存在则忽略） ★★★★★
    execIgnore(q, R"SQL(
ALTER TABLE app_settings ADD COLUMN auto_id_gen INTEGER NOT NULL DEFAULT 0;
)SQL");

    // ===== users =====
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

    // ===== projects =====
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

    // ===== project_info =====
    execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS project_info(
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    projectId       INTEGER NOT NULL,
    projectName     TEXT NOT NULL DEFAULT '',
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

    migrateProjectInfo(db);

    // ===== adc_data =====
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
