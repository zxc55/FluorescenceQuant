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
// =======================
// 历史记录表（project_info）
// =======================
struct HistoryRow {
    int id = 0;                   // 主键
    int projectId = 0;            // 对应 projects.id
    QString projectName;          // 项目名称
    QString sampleNo;             // 样品编号
    QString sampleSource;         // 样品来源
    QString sampleName;           // 样品名称
    QString standardCurve;        // 标准曲线类型
    QString batchCode;            // 批次编码
    double detectedConc = 0.0;    // 检测浓度
    double referenceValue = 0.0;  // 参考值
    QString result;               // 检测结论（合格/超标）
    QString detectedTime;         // 检测时间
    QString detectedUnit;         // 检测单位（默认 μg/kg）
    QString detectedPerson;       // 检测人员
    QString dilutionInfo;         // 稀释倍数（1倍 / 5倍）
};
Q_DECLARE_METATYPE(HistoryRow)