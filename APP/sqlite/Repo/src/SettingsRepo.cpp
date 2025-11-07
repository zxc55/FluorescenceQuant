#include "SettingsRepo.h"

#include <QDebug>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

bool SettingsRepo::selectOne(QSqlDatabase db, AppSettingsRow& out) {
    QSqlQuery q(db);
    if (!q.exec("SELECT id, manufacturer_name, /* 其它列 */ FROM app_settings WHERE id=1")) {
        qWarning() << "[SettingsRepo] selectOne exec fail:" << q.lastError();
        return false;
    }
    if (!q.next()) {  // 没有任何行
        qWarning() << "[SettingsRepo] selectOne empty";
        return false;
    }
    // 读取各列填充 out ...
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
