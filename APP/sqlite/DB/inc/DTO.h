#pragma once
#include <QMetaType>
#include <QString>

// 系统设置表 app_settings 对应行
struct AppSettingsRow {
    int id = 1;  // 固定为 1

    QString manufacturer;       // 厂家名称
    bool engineerMode = false;  // 工程师模式

    bool autoPrint = false;          // 启动自动打印
    bool autoUpload = false;         // 启动自动上传服务器
    bool autoIdGen = false;          // ID号自动生成
    bool microSwitch = false;        // 微动开关
    bool manufacturerPrint = false;  // ★ 厂家名称是否打印（你的 QML、ViewModel 正在使用这个字段）

    bool printSampleSource = false;          // 是否打印样品来源
    bool printReferenceValue = false;        // 是否打印参考值
    bool printDetectedPerson = false;        // 是否打印检测人员
    bool printDilutionInfo = false;          // 是否打印稀释倍数
    int backlight = 80;                      // 背光 (0~100)
    QString lang = QStringLiteral("zh_CN");  // 语言

    QString updatedAt;  // 更新时间（数据库维护）
    QString lastSampleDate;
    int lastSampleIndex = 0;
};

Q_DECLARE_METATYPE(AppSettingsRow)
// 用户表（users）
struct UserRow {
    int id;
    QString username;
    QString role;
    QString roleName;
    QString note;
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