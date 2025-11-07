#pragma once
#include <QMetaType>
#include <QString>

// 系统设置（单行表 app_settings）
struct AppSettingsRow {
    int id = 1;  // 永远用 id=1 的单行配置
    QString manufacturer;
    bool engineerMode = false;
    bool autoPrint = true;
    int backlight = 80;  // 0~100
    QString lang = QStringLiteral("zh_CN");
    QString updatedAt;  // 由数据库触发器/代码维护
};
Q_DECLARE_METATYPE(AppSettingsRow)

// 用户表（users）
struct UserRow {
    int id = 0;
    QString username;     // 唯一
    QString role;         // admin/engineer/operator
    QString roleName;     // 友好显示名（例如 “管理员/工程师/操作员”）
    QString saltHex;      // 随机盐（十六进制）
    QString passHashHex;  // SHA256(salt+password) 的十六进制
    bool active = true;
    QString note;  // 备注
};
Q_DECLARE_METATYPE(UserRow)
struct ProjectRow {
    int id = 0;
    QString name;       // 项目名称
    QString batch;      // 批次编码
    QString updatedAt;  // 更新时间（文本，yyyy-MM-dd HH:mm:ss）
};
Q_DECLARE_METATYPE(ProjectRow)