#pragma once
#include <QSqlDatabase>
#include <QVector>

#include "DTO.h"

// 简单仓库：只负责 projects 的读取与删除
namespace ProjectsRepo {

// 读取所有项目（按 id 升序）
// 返回 true 表示执行成功；rows 输出数据
bool selectAll(QSqlDatabase& db, QVector<ProjectRow>& rows);

// 按 id 删除一条
bool deleteById(QSqlDatabase& db, int id);
// === 新增：插入检测信息记录（project_info） ===
bool insertProjectInfo(QSqlDatabase& db, const QVariantMap& data);

}  // namespace ProjectsRepo
