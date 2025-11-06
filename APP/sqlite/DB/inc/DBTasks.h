#ifndef DBTASKS_H_
#define DBTASKS_H_
#include <QString>

// —— DB 任务类型 ——
enum class DBTaskType {
    EnsureAllSchemas,  // 确保所有表（settings + roles + users + perms）
    LoadSettings,      // 读设置
    UpdateSettings,    // 写设置 id=1
    // 用户相关：
    AuthLogin,     // 登录（校验用户）
    LoadUsers,     // 列出所有用户（仅管理员）
    AddUser,       // 新增用户
    DeleteUser,    // 删除用户
    ResetPassword  // 重置密码
};

// —— DB 任务载体 ——
struct DBTask {
    DBTaskType type;  // 类型
    // 设置写入字段：
    QString manufacturer;
    bool engineerMode = false;
    bool autoPrint = true;
    int backlight = 80;
    QString lang = "zh_CN";
    // 登录/用户字段：
    QString username;  // 用户名
    QString password;  // 明文密码（进来时，用于计算 hash；不会落库明文）
    QString roleName;  // 角色
    QString note;      // 备注
};
#endif  // DBTASKS_H_