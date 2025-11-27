#include "SettingsRepo.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool SettingsRepo::selectOne(QSqlDatabase& db, AppSettingsRow& out) {
    QSqlQuery q(db);

    if (!q.exec(
            "SELECT manufacturer_name, backlight, language, auto_update,"
            "       engineer_mode, auto_print, auto_upload, auto_id_gen,"
            "       micro_switch, manufacturer_print, last_sample_date, last_sample_index  "
            "FROM app_settings WHERE id=1;")) {
        qWarning() << "[SettingsRepo] select fail:" << q.lastError();
        return false;
    }

    if (!q.next())
        return false;

    out.manufacturer = q.value(0).toString();
    out.backlight = q.value(1).toInt();
    out.lang = q.value(2).toString();
    out.engineerMode = q.value(4).toInt();
    out.autoPrint = q.value(5).toInt();
    out.autoUpload = q.value(6).toInt();
    out.autoIdGen = q.value(7).toInt();
    out.microSwitch = q.value(8).toInt();
    out.manufacturerPrint = q.value(9).toInt();
    out.lastSampleDate = q.value(10).toString();
    out.lastSampleIndex = q.value(11).toInt();

    return true;
}

bool SettingsRepo::updateOne(QSqlDatabase& db, const AppSettingsRow& s) {
    QSqlQuery q(db);

    q.prepare(
        "UPDATE app_settings SET "
        "manufacturer_name=?, "
        "backlight=?, "
        "language=?, "
        "auto_update=?, "
        "engineer_mode=?, "
        "auto_print=?, "
        "auto_upload=?, "
        "auto_id_gen=?, "
        "micro_switch=?, "
        "manufacturer_print=?, "
        "last_sample_date=?, "
        "last_sample_index=? "
        "WHERE id=1;");

    q.addBindValue(s.manufacturer);
    q.addBindValue(s.backlight);
    q.addBindValue(s.lang);
    q.addBindValue(0);  // auto_update 暂用固定值，可改
    q.addBindValue(s.engineerMode);
    q.addBindValue(s.autoPrint);
    q.addBindValue(s.autoUpload);
    q.addBindValue(s.autoIdGen);
    q.addBindValue(s.microSwitch);
    q.addBindValue(s.manufacturerPrint);
    q.addBindValue(s.lastSampleDate);
    q.addBindValue(s.lastSampleIndex);

    if (!q.exec()) {
        qWarning() << "[SettingsRepo] update fail:" << q.lastError();
        return false;
    }

    qInfo() << "[SettingsRepo] update OK";
    return true;
}
