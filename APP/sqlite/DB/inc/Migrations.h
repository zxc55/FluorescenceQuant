#pragma once
#include <QtSql/QSqlDatabase>

// 旧代码在调用 migrateAllToV1()，做一个同名入口以保持兼容
bool migrateAllToV1(QSqlDatabase db);

// 也提供一个通用入口
bool applyMigrations(QSqlDatabase db);
