#include "SettingsRepo.h"

#include <QDebug>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

bool SettingsRepo::selectOne(QSqlDatabase db, AppSettingsRow& out) {
    if (!db.isValid() || !db.isOpen())
        return false;
    QSqlQuery q(db);
    q.prepare(
        "SELECT id, manufacturer, engineer_mode, auto_print, backlight, lang, updated_at "
        "FROM app_settings WHERE id=1");
    if (!q.exec()) {
        qWarning() << "[SettingsRepo] selectOne failed:" << q.lastError();
        return false;
    }
    if (!q.next())
        return false;

    out.id = q.value(0).toInt();
    out.manufacturer = q.value(1).toString();
    out.engineerMode = q.value(2).toBool();
    out.autoPrint = q.value(3).toBool();
    out.backlight = q.value(4).toInt();
    out.lang = q.value(5).toString();
    out.updatedAt = q.value(6).toString();
    return true;
}

bool SettingsRepo::updateOne(QSqlDatabase db, const AppSettingsRow& in) {
    if (!db.isValid() || !db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(
        "UPDATE app_settings SET "
        " manufacturer=:m, engineer_mode=:e, auto_print=:a, "
        " backlight=:b, lang=:l, updated_at=CURRENT_TIMESTAMP "
        "WHERE id=1");
    q.bindValue(":m", in.manufacturer);
    q.bindValue(":e", in.engineerMode);
    q.bindValue(":a", in.autoPrint);
    q.bindValue(":b", in.backlight);
    q.bindValue(":l", in.lang);

    if (!q.exec()) {
        qWarning() << "[SettingsRepo] updateOne failed:" << q.lastError();
        return false;
    }
    return q.numRowsAffected() >= 0;
}
