#include "Migrations.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// 小工具：执行一条 SQL，并在失败时打日志
static bool execOne(QSqlQuery& q, const QString& sql) {
    if (!q.exec(sql)) {
        qWarning() << "[MIGRATE] SQL fail:" << q.lastError().text();
        qWarning() << "SQL:" << sql;
        return false;
    }
    return true;
}

// 若某列不存在则新增（SQLite 的 ADD COLUMN 不支持 IF NOT EXISTS，用 pragma table_info 检测）
static bool ensureColumnExists(QSqlDatabase db,
                               const QString& table,
                               const QString& colDef,   // 例如 "batch TEXT NOT NULL DEFAULT ''"
                               const QString& colName)  // 例如 "batch"
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1);").arg(table))) {
        qWarning() << "[MIGRATE] pragma table_info fail:" << q.lastError().text();
        return false;
    }
    bool found = false;
    while (q.next()) {
        const QString name = q.value(1).toString();  // cid,name,type,notnull,dflt_value,pk
        if (name.compare(colName, Qt::CaseInsensitive) == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        const QString sql = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2;").arg(table, colDef);
        QSqlQuery q2(db);
        if (!q2.exec(sql)) {
            qWarning() << "[MIGRATE] add column fail:" << q2.lastError().text();
            qWarning() << "SQL:" << sql;
            return false;
        }
        qInfo() << "[MIGRATE]" << table << "ADD COLUMN" << colName << "ok";
    }
    return true;
}

bool migrateAllToV1(QSqlDatabase db) {
    if (!db.isOpen()) {
        qWarning() << "[MIGRATE] db not open";
        return false;
    }

    QSqlQuery q(db);

    // 1) 版本号（可选）
    if (!execOne(q, "PRAGMA user_version;")) {
        return false;  // 只是探测，不使用结果
    }

    // 2) app_settings
    if (!execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS app_settings(
    id                  INTEGER PRIMARY KEY CHECK(id=1),
    manufacturer_name   TEXT    NOT NULL DEFAULT '',
    backlight           INTEGER NOT NULL DEFAULT 50,
    language            TEXT    NOT NULL DEFAULT 'zh_CN',
    auto_update         INTEGER NOT NULL DEFAULT 0,
    engineer_mode       INTEGER NOT NULL DEFAULT 0
);
)SQL"))
        return false;

    // 兼容旧库：确保 manufacturer_name 列存在
    if (!ensureColumnExists(db, "app_settings",
                            "manufacturer_name TEXT NOT NULL DEFAULT ''",
                            "manufacturer_name"))
        return false;

    // 预置一行（id=1）
    if (!execOne(q, R"SQL(
INSERT OR IGNORE INTO app_settings(id, manufacturer_name)
VALUES(1, '');
)SQL"))
        return false;

    // 3) users
    if (!execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS users(
    username      TEXT PRIMARY KEY,
    password_hash TEXT NOT NULL DEFAULT '',
    salt          TEXT NOT NULL DEFAULT '',
    role          TEXT NOT NULL DEFAULT 'operator',
    note          TEXT NOT NULL DEFAULT ''
);
)SQL"))
        return false;

    // 兼容旧库：确保必要列
    if (!ensureColumnExists(db, "users", "password_hash TEXT NOT NULL DEFAULT ''", "password_hash"))
        return false;
    if (!ensureColumnExists(db, "users", "salt TEXT NOT NULL DEFAULT ''", "salt"))
        return false;
    if (!ensureColumnExists(db, "users", "role TEXT NOT NULL DEFAULT 'operator'", "role"))
        return false;
    if (!ensureColumnExists(db, "users", "note TEXT NOT NULL DEFAULT ''", "note"))
        return false;

    // 预置三种角色帐号（若不存在）
    if (!execOne(q, R"SQL(
INSERT OR IGNORE INTO users(username,password_hash,salt,role,note)
VALUES ('admin','123456','salt','admin','内置管理员'),
       ('eng','654321','salt','engineer','内置工程师'),
       ('op','111111','salt','operator','内置操作员');
)SQL"))
        return false;

    // 4) projects
    // 注意：默认时间使用 localtime，便于在本地时区显示
    if (!execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS projects(
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT NOT NULL,
    batch      TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
)SQL"))
        return false;

    // 兼容旧库：确保列存在
    if (!ensureColumnExists(db, "projects", "batch TEXT NOT NULL DEFAULT ''", "batch"))
        return false;
    if (!ensureColumnExists(db, "projects", "updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))", "updated_at"))
        return false;

    // 预置 3 条常见项目（仅在表为空时）
    if (!execOne(q, R"SQL(
INSERT INTO projects(name,batch,updated_at)
SELECT '黄曲霉毒素','B2025-001',datetime('now','localtime')
WHERE NOT EXISTS (SELECT 1 FROM projects);
)SQL"))
        return false;

    if (!execOne(q, R"SQL(
INSERT INTO projects(name,batch,updated_at)
SELECT '呕吐毒素','B2025-002',datetime('now','localtime')
WHERE (SELECT COUNT(1) FROM projects)=1;
)SQL"))
        return false;

    if (!execOne(q, R"SQL(
INSERT INTO projects(name,batch,updated_at)
SELECT '玉米赤霉烯酮','B2025-003',datetime('now','localtime')
WHERE (SELECT COUNT(1) FROM projects)=2;
)SQL"))
        return false;

    // 5) 热修复：把早期占位“荧光试剂盒A”纠正，并修补 batch / 时间
    {
        // 仅当 id=1 且名称仍是占位值时，改成“呕吐毒素”
        if (!execOne(q, R"SQL(
UPDATE projects
SET name='呕吐毒素',
    batch=CASE WHEN IFNULL(batch,'')='' THEN 'B2025-001' ELSE batch END,
    updated_at=datetime('now','localtime')
WHERE id=1 AND name='荧光试剂盒A';
)SQL"))
            return false;

        // 若存在空 batch，按 id 1/2/3 兜底填充（不覆盖已有值）
        if (!execOne(q, R"SQL(
UPDATE projects
SET batch=CASE id
            WHEN 1 THEN COALESCE(NULLIF(batch,''),'B2025-001')
            WHEN 2 THEN COALESCE(NULLIF(batch,''),'B2025-002')
            WHEN 3 THEN COALESCE(NULLIF(batch,''),'B2025-003')
            ELSE COALESCE(NULLIF(batch,''),'')
          END
WHERE id IN (1,2,3);
)SQL"))
            return false;

        // 把异常时间（1970 或 0000 或空）统一修正为当前本地时间
        if (!execOne(q, R"SQL(
UPDATE projects
SET updated_at = datetime('now','localtime')
WHERE IFNULL(updated_at,'')='' OR
      updated_at LIKE '1970-%' OR
      updated_at LIKE '0000-%';
)SQL"))
            return false;
    }

    {
        // 6) project_info（项目信息表）
        if (!execOne(q, R"SQL(
CREATE TABLE IF NOT EXISTS project_info(
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id        INTEGER NOT NULL,                              -- 外键：对应 projects.id
    sample_no         TEXT NOT NULL DEFAULT '',                       -- 样品编号
    sample_source     TEXT NOT NULL DEFAULT '',                       -- 样品来源
    sample_name       TEXT NOT NULL DEFAULT '',                       -- 样品名称
    standard_curve    TEXT NOT NULL DEFAULT '',                       -- 标准曲线（仅允许：粮食谷物 / 加工副产物 / 配合饲料）
    batch_code        TEXT NOT NULL DEFAULT '',                       -- 批次编码
    detected_conc     REAL NOT NULL DEFAULT 0.0,                       -- 检测浓度
    reference_value   REAL NOT NULL DEFAULT 0.0,                       -- 参考值
    result            TEXT NOT NULL DEFAULT '',                        -- 检测结论（合格 / 超标）
    detected_time     TEXT NOT NULL DEFAULT (datetime('now','localtime')),  -- 检测时间
    detected_unit     TEXT NOT NULL DEFAULT 'μg/kg',                   -- 检测单位
    detected_person   TEXT NOT NULL DEFAULT '',                        -- 检测人员
    dilution_info     TEXT NOT NULL DEFAULT '1倍',                     -- 超曲线范围稀释倍数（1倍 / 5倍）
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE
);
)SQL"))
            return false;
    }
    qInfo() << "[MIGRATE] v1 done";
    return true;
}
