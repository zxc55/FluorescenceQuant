#include "SettingsRepo.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// 从数据库中读取一条设置记录(id=1)
bool SettingsRepo::selectOne(QSqlDatabase& db, AppSettingsRow& out) {
    QSqlQuery q(db);  // 创建查询对象，绑定到当前数据库连接 db

    // 执行 SELECT 语句，读取所有需要的字段
    // 注意：后面多加了 4 个打印相关字段
    if (!q.exec(
            "SELECT manufacturer_name, backlight, language, auto_update,"
            "       engineer_mode, auto_print, auto_upload, auto_id_gen,"
            "       micro_switch, manufacturer_print, "
            "       last_sample_date, last_sample_index, "
            "       print_sample_source, print_reference_value, "
            "       print_detected_person, print_dilution_info "
            "FROM app_settings WHERE id=1;")) {
        // 如果执行失败，打印错误信息
        qWarning() << "[SettingsRepo] select fail:" << q.lastError();
        return false;  // 返回 false 表示读取失败
    }

    // 如果没有查到任何行，返回 false
    if (!q.next())
        return false;

    // ===== 把结果逐个填回到 AppSettingsRow 结构体里 =====
    out.manufacturer = q.value(0).toString();  // 厂家名称
    out.backlight = q.value(1).toInt();        // 背光值
    out.lang = q.value(2).toString();          // 语言
    // q.value(3) 是 auto_update，目前我们没用，直接忽略
    out.engineerMode = q.value(4).toInt();        // 工程师模式开关
    out.autoPrint = q.value(5).toInt();           // 启动自动打印
    out.autoUpload = q.value(6).toInt();          // 启动自动上传
    out.autoIdGen = q.value(7).toInt();           // ID号自动生成
    out.microSwitch = q.value(8).toInt();         // 微动开关
    out.manufacturerPrint = q.value(9).toInt();   // 厂家名称打印
    out.lastSampleDate = q.value(10).toString();  // 上次样品日期
    out.lastSampleIndex = q.value(11).toInt();    // 上次样品序号

    // ===== 新增的四个打印选项开关 =====
    out.printSampleSource = q.value(12).toInt();    // 打印样品来源
    out.printReferenceValue = q.value(13).toInt();  // 打印参考值
    out.printDetectedPerson = q.value(14).toInt();  // 打印检测人员
    out.printDilutionInfo = q.value(15).toInt();    // 打印稀释倍数

    return true;  // 全部读取成功
}

// 更新一条设置记录(id=1)
bool SettingsRepo::updateOne(QSqlDatabase& db, const AppSettingsRow& s) {
    QSqlQuery q(db);  // 创建查询对象

    // 预编译 UPDATE 语句，设置需要更新的字段
    // 注意：后面多加了 4 个打印相关字段
    q.prepare(
        "UPDATE app_settings SET "
        "manufacturer_name=?, "      // 0  厂家名称
        "backlight=?, "              // 1  背光
        "language=?, "               // 2  语言
        "auto_update=?, "            // 3  自动更新(这里先写死为 0)
        "engineer_mode=?, "          // 4  工程师模式
        "auto_print=?, "             // 5  启动自动打印
        "auto_upload=?, "            // 6  启动自动上传
        "auto_id_gen=?, "            // 7  ID号自动生成
        "micro_switch=?, "           // 8  微动开关
        "manufacturer_print=?, "     // 9  厂家名称打印
        "last_sample_date=?, "       // 10 最近样品日期
        "last_sample_index=?, "      // 11 最近样品序号
        "print_sample_source=?, "    // 12 打印样品来源
        "print_reference_value=?, "  // 13 打印参考值
        "print_detected_person=?, "  // 14 打印检测人员
        "print_dilution_info=? "     // 15 打印稀释倍数
        "WHERE id=1;");              // 限定只更新 id = 1 这一条

    // ===== 按顺序绑定上面 ? 占位符的值 =====
    q.addBindValue(s.manufacturer.isEmpty() ? "" : s.manufacturer);  // 0 厂家名称
    q.addBindValue(s.backlight);                                     // 1 背光
    q.addBindValue(s.lang);                                          // 2 语言
    q.addBindValue(0);                                               // 3 auto_update 暂时固定为 0
    q.addBindValue(s.engineerMode);                                  // 4 工程师模式
    q.addBindValue(s.autoPrint);                                     // 5 自动打印
    q.addBindValue(s.autoUpload);                                    // 6 自动上传
    q.addBindValue(s.autoIdGen);                                     // 7 ID号自动生成
    q.addBindValue(s.microSwitch);                                   // 8 微动开关
    q.addBindValue(s.manufacturerPrint);                             // 9 厂家名称打印
    q.addBindValue(s.lastSampleDate);                                // 10 最近样品日期
    q.addBindValue(s.lastSampleIndex);                               // 11 最近样品序号

    // ===== 新增的四个打印选项 =====
    q.addBindValue(s.printSampleSource);    // 12 打印样品来源
    q.addBindValue(s.printReferenceValue);  // 13 打印参考值
    q.addBindValue(s.printDetectedPerson);  // 14 打印检测人员
    q.addBindValue(s.printDilutionInfo);    // 15 打印稀释倍数

    // 执行 UPDATE
    if (!q.exec()) {
        qWarning() << "[SettingsRepo] update fail:" << q.lastError();  // 打印错误
        return false;                                                  // 返回失败
    }

    qInfo() << "[SettingsRepo] update OK";  // 日志：更新成功
    return true;                            // 返回成功
}
