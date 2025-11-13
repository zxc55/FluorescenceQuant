#include "SettingsRepo.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

// 读取唯一的设置行（id=1）
bool SettingsRepo::selectOne(QSqlDatabase db, AppSettingsRow& out) {
    if (!db.isOpen()) {
        qWarning() << "[SettingsRepo] db not open";
        return false;
    }

    QSqlQuery q(db);
    if (!q.exec("SELECT id, manufacturer_name, backlight, language, auto_update, engineer_mode FROM app_settings WHERE id=1;")) {
        qWarning() << "[SettingsRepo] selectOne exec fail:" << q.lastError();
        return false;
    }

    if (!q.next()) {
        qWarning() << "[SettingsRepo] selectOne empty";
        return false;
    }

    out.id = q.value(0).toInt();
    out.manufacturer = q.value(1).toString();
    out.backlight = q.value(2).toInt();
    out.lang = q.value(3).toString();
    out.engineerMode = q.value(5).toInt();
    out.autoPrint = q.value(4).toInt();  // 对应 auto_update，沿用旧字段名逻辑
    return true;
}

// 更新设置行（id=1）
bool SettingsRepo::updateOne(QSqlDatabase db, const AppSettingsRow& in) {
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "[SettingsRepo] updateOne: db invalid";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(R"SQL(
        UPDATE app_settings
        SET manufacturer_name=:m,
            engineer_mode=:e,
            auto_update=:a,
            backlight=:b,
            language=:l
        WHERE id=1;
    )SQL");

    q.bindValue(":m", in.manufacturer);
    q.bindValue(":e", in.engineerMode ? 1 : 0);
    q.bindValue(":a", in.autoPrint ? 1 : 0);
    q.bindValue(":b", in.backlight);
    q.bindValue(":l", in.lang);

    if (!q.exec()) {
        qWarning() << "[SettingsRepo] updateOne failed:" << q.lastError();
        return false;
    }

    return q.numRowsAffected() >= 0;
}
