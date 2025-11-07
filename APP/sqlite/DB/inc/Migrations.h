#pragma once
#include <QSqlDatabase>

// 迁移到 V1（创建所有需要的表、补列、预置数据）
bool migrateAllToV1(QSqlDatabase db);
