#include "QrRepo.h"  // 头文件

#include <QDebug>  // qWarning/qInfo
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>    // QSqlError
#include <QSqlQuery>    // QSqlQuery
#include <QStringList>  // QStringList
// =========================
// 解析二维码字符串：id:项目id;bn:批次编码;
// =========================
bool QrRepo::parseQrText(const QString& qrText, QString& projectId, QString& batchCode, QString& err)  // 解析二维码
{
    projectId.clear();  // 初始化 projectId
    batchCode.clear();  // 清空 batchCode
    err.clear();        // 清空错误

    QString s = qrText;   // 拷贝二维码字符串
    s = s.trimmed();      // 去掉首尾空白
    s.replace("\r", "");  // 去掉回车
    s.replace("\n", "");  // 去掉换行

    if (s.isEmpty()) {           // 空内容判断
        err = "二维码内容为空";  // 设置错误
        return false;            // 返回失败
    }

    const QStringList parts = s.split(";", QString::SkipEmptyParts);  // 按 ';' 分割

    if (parts.isEmpty()) {                 // 分割后为空
        err = "二维码格式错误：缺少字段";  // 设置错误
        return false;                      // 返回失败
    }

    bool hasId = false;  // 是否解析到 id
    bool hasBn = false;  // 是否解析到 bn

    for (const QString& one : parts) {       // 遍历每段
        const QString item = one.trimmed();  // 去掉空白
        const int pos = item.indexOf(":");   // 找 ':'
        if (pos <= 0)                        // 没有 ':' 或 key 为空
            continue;                        // 跳过该段

        const QString key = item.left(pos).trimmed().toLower();  // 取 key 并转小写（容错）
        const QString val = item.mid(pos + 1).trimmed();         // 取 value

        if (key == "id") {        // id 字段
            if (val.isEmpty()) {  // id 不能为空
                err = "二维码 id 字段为空";
                return false;
            }
            projectId = val;                 // 直接使用字符串，如 "test"
            hasId = true;                    // 标记找到 id
        } else if (key == "bn") {            // bn 字段
            if (val.isEmpty()) {             // bn 为空
                err = "二维码 bn 字段为空";  // 设置错误
                return false;                // 返回失败
            }
            batchCode = val;  // 输出 batchCode
            hasBn = true;     // 标记找到 bn
        }
    }

    if (!hasId) {                    // 缺少 id
        err = "二维码缺少 id 字段";  // 设置错误
        return false;                // 返回失败
    }

    if (!hasBn) {                    // 缺少 bn
        err = "二维码缺少 bn 字段";  // 设置错误
        return false;                // 返回失败
    }

    batchCode = batchCode.trimmed();  // 再次去空白
    if (batchCode.isEmpty()) {        // 再校验
        err = "二维码 bn 字段为空";   // 设置错误
        return false;                 // 返回失败
    }

    return true;  // 解析成功
}

// =========================
// 按 projectId + batchCode 查询一条
// 查到返回 true，查不到返回 false（out.error 会写原因）
// =========================
bool QrRepo::selectOne(QSqlDatabase db, QString& projectId, const QString& batchCode, QVariantMap& out)  // 查询一条
{
    out.clear();                         // 清空输出
    out.insert("ok", false);             // 默认 ok=false
    out.insert("projectId", projectId);  // 回填 projectId
    out.insert("batchCode", batchCode);  // 回填 batchCode

    const QString pid = projectId.trimmed();
    if (pid.isEmpty()) {
        out.insert("error", "projectId 为空");
        return false;
    }

    const QString bc = batchCode.trimmed();     // 去掉空白
    if (bc.isEmpty()) {                         // 校验 batchCode
        out.insert("error", "batchCode 为空");  // 写错误
        return false;                           // 返回失败
    }

    if (!db.isValid() || !db.isOpen()) {      // 校验 db 连接
        out.insert("error", "数据库未打开");  // 写错误
        return false;                         // 返回失败
    }

    QSqlQuery q(db);  // 创建查询对象
    q.prepare(R"SQL(
SELECT
    projectName,
    updated_at,
    methodName,
    methodData,
    temperature,
    timeSec,
    C1, T1, C2, T2
FROM qr_method_config
WHERE projectId = :pid AND batchCode = :bc
LIMIT 1;
)SQL");               // 预编译 SQL

    q.bindValue(":pid", projectId);  // 绑定 projectId
    q.bindValue(":bc", bc);          // 绑定 batchCode

    if (!q.exec()) {                                                        // 执行失败
        out.insert("error", q.lastError().text());                          // 写 SQL 错误
        qWarning() << "QrRepo::selectOne failed:" << q.lastError().text();  // 打印日志
        return false;                                                       // 返回失败
    }

    if (!q.next()) {                                     // 没查到
        out.insert("error", "qr_method_config 未找到");  // 写未找到
        return false;                                    // 返回失败
    }

    out.insert("ok", true);   // ok=true
    out.insert("error", "");  // 清空错误

    out.insert("projectName", q.value(0).toString());  // 项目名称
    out.insert("updated_at", q.value(1).toString());   // 更新时间
    out.insert("methodName", q.value(2).toString());   // 方法名称
    out.insert("methodData", q.value(3).toString());   // 方法数据（JSON）
    out.insert("temperature", q.value(4).toDouble());  // 温度
    out.insert("timeSec", q.value(5).toInt());         // 时间（秒）
    out.insert("C1", q.value(6).toInt());              // C1
    out.insert("T1", q.value(7).toInt());              // T1
    out.insert("C2", q.value(8).toInt());              // C2
    out.insert("T2", q.value(9).toInt());              // T2

    return true;  // 返回成功
}

// =========================
// 按二维码字符串查询一条：id:xx;bn:yy;
// =========================
bool QrRepo::selectOneByQrText(QSqlDatabase db, const QString& qrText, QVariantMap& out)  // 按二维码查
{
    out.clear();                // 清空输出
    out.insert("ok", false);    // 默认 ok=false
    out.insert("raw", qrText);  // 回填原始二维码字符串

    QString projectId;  // projectId
    QString batchCode;  // batchCode
    QString err;        // err

    if (!parseQrText(qrText, projectId, batchCode, err)) {  // 解析失败
        out.insert("error", err);                           // 写错误
        out.insert("projectId", -1);                        // 无效 projectId
        out.insert("batchCode", "");                        // 空 batchCode
        return false;                                       // 返回失败
    }

    // 调用 selectOne 复用查表逻辑
    QVariantMap tmp;                                           // 临时结果
    const bool ok = selectOne(db, projectId, batchCode, tmp);  // 查表

    // 合并输出：保留 raw，同时把查表结果复制出来
    tmp.insert("raw", qrText);  // 回填 raw
    out = tmp;                  // 覆盖 out

    return ok;  // 返回查表结果
}

// =========================
// 保存/覆盖：存在则 UPDATE，不存在则 INSERT
// =========================
bool QrRepo::upsert(QSqlDatabase db, const QVariantMap& cfg)  // 保存或覆盖
{
    if (!db.isValid() || !db.isOpen()) {             // 校验 db
        qWarning() << "QrRepo::upsert db not open";  // 打印错误
        return false;                                // 返回失败
    }

    const QString projectId = cfg.value("projectId").toString().trimmed();
    const QString batchCode = cfg.value("batchCode").toString().trimmed();  // 取 batchCode
    const QString projectName = cfg.value("projectName").toString();        // 取 projectName
    const QString methodName = cfg.value("methodName").toString();          // 取 methodName
    const QString methodData = cfg.value("methodData").toString();          // 取 methodData
    const double temperature = cfg.value("temperature").toDouble();         // 取 temperature
    const int timeSec = cfg.value("timeSec").toInt();                       // 取 timeSec
    const int C1 = cfg.value("C1").toInt();                                 // 取 C1
    const int T1 = cfg.value("T1").toInt();                                 // 取 T1
    const int C2 = cfg.value("C2").toInt();                                 // 取 C2
    const int T2 = cfg.value("T2").toInt();                                 // 取 T2

    if (projectId.isEmpty() || batchCode.isEmpty()) {  // 校验关键键
        qWarning() << "QrRepo::upsert invalid key";    // 打印错误
        return false;                                  // 返回失败
    }

    QSqlQuery q(db);  // 查询对象

    // 1) 先 UPDATE（存在则覆盖）
    q.prepare(R"SQL(
UPDATE qr_method_config
SET
    projectName = :pname,
    updated_at  = datetime('now','localtime'),
    methodName  = :mname,
    methodData  = :mdata,
    temperature = :temp,
    timeSec     = :tsec,
    C1 = :c1, T1 = :t1, C2 = :c2, T2 = :t2
WHERE projectId = :pid AND batchCode = :bc;
)SQL");  // UPDATE 语句

    q.bindValue(":pname", projectName);  // 绑定 projectName
    q.bindValue(":mname", methodName);   // 绑定 methodName
    q.bindValue(":mdata", methodData);   // 绑定 methodData
    q.bindValue(":temp", temperature);   // 绑定 temperature
    q.bindValue(":tsec", timeSec);       // 绑定 timeSec
    q.bindValue(":c1", C1);              // 绑定 C1
    q.bindValue(":t1", T1);              // 绑定 T1
    q.bindValue(":c2", C2);              // 绑定 C2
    q.bindValue(":t2", T2);              // 绑定 T2
    q.bindValue(":pid", projectId);      // 绑定 projectId
    q.bindValue(":bc", batchCode);       // 绑定 batchCode

    if (!q.exec()) {                                                            // 执行 UPDATE
        qWarning() << "QrRepo::upsert UPDATE failed:" << q.lastError().text();  // 打印错误
        return false;                                                           // 返回失败
    }

    if (q.numRowsAffected() > 0) {  // UPDATE 命中
        return true;                // 覆盖成功
    }

    // 2) 没命中则 INSERT（新增）
    q.prepare(R"SQL(
INSERT INTO qr_method_config(
    projectId, projectName, batchCode, updated_at,
    methodName, methodData, temperature, timeSec,
    C1, T1, C2, T2
) VALUES(
    :pid, :pname, :bc, datetime('now','localtime'),
    :mname, :mdata, :temp, :tsec,
    :c1, :t1, :c2, :t2
);
)SQL");  // INSERT 语句

    q.bindValue(":pid", projectId);      // 绑定 projectId
    q.bindValue(":pname", projectName);  // 绑定 projectName
    q.bindValue(":bc", batchCode);       // 绑定 batchCode
    q.bindValue(":mname", methodName);   // 绑定 methodName
    q.bindValue(":mdata", methodData);   // 绑定 methodData
    q.bindValue(":temp", temperature);   // 绑定 temperature
    q.bindValue(":tsec", timeSec);       // 绑定 timeSec
    q.bindValue(":c1", C1);              // 绑定 C1
    q.bindValue(":t1", T1);              // 绑定 T1
    q.bindValue(":c2", C2);              // 绑定 C2
    q.bindValue(":t2", T2);              // 绑定 T2

    if (!q.exec()) {                                                            // 执行 INSERT
        qWarning() << "QrRepo::upsert INSERT failed:" << q.lastError().text();  // 打印错误
        return false;                                                           // 返回失败
    }

    return true;  // 新增成功
}

// =========================
// 删除一条：按 projectId + batchCode
// =========================
bool QrRepo::deleteOne(QSqlDatabase db, QString& projectId, const QString& batchCode)  // 删除一条
{
    if (!db.isValid() || !db.isOpen())  // 校验 db
        return false;                   // 返回失败

    const QString pid = projectId.trimmed();
    if (pid.isEmpty())
        return false;

    const QString bc = batchCode.trimmed();  // 去空白
    if (bc.isEmpty())                        // 校验 batchCode
        return false;                        // 返回失败

    QSqlQuery q(db);                                                                  // 查询对象
    q.prepare("DELETE FROM qr_method_config WHERE projectId = ? AND batchCode = ?");  // 删除 SQL
    q.addBindValue(projectId);                                                        // 绑定 projectId
    q.addBindValue(bc);                                                               // 绑定 batchCode

    if (!q.exec()) {                                                        // 执行失败
        qWarning() << "QrRepo::deleteOne failed:" << q.lastError().text();  // 打印错误
        return false;                                                       // 返回失败
    }

    return true;  // 返回成功
}
bool QrRepo::exists(QSqlDatabase db, QString& projectId, const QString& batchCode, QString& err)  // ✅ 判断是否存在
{
    err.clear();  // 清空错误信息

    const QString pid = projectId.trimmed();
    if (pid.isEmpty())
        return false;

    const QString bc = batchCode.trimmed();  // 去空格
    if (bc.isEmpty()) {                      // 校验 batchCode
        err = "batchCode 为空";              // 设置错误
        return false;                        // 返回失败/不存在
    }

    if (!db.isValid() || !db.isOpen()) {  // 校验数据库连接
        err = "数据库未打开";             // 设置错误
        return false;                     // 返回失败/不存在
    }

    QSqlQuery q(db);  // 创建查询对象
    q.prepare(R"SQL(
SELECT 1
FROM qr_method_config
WHERE projectId = :pid AND batchCode = :bc
LIMIT 1;
)SQL");               // 只查存在性（最快）

    q.bindValue(":pid", projectId);  // 绑定 projectId
    q.bindValue(":bc", bc);          // 绑定 batchCode

    if (!q.exec()) {                                    // 执行失败
        err = q.lastError().text();                     // 返回 SQL 错误
        qWarning() << "QrRepo::exists failed:" << err;  // 打印错误
        return false;                                   // 返回失败/不存在
    }

    return q.next();  // 有一行就存在
}
bool QrRepo::existsByQrText(QSqlDatabase db, const QString& qrText, QVariantMap& out)  // ✅ 输入二维码字符串判断是否存在
{
    out.clear();                // 清空输出
    out.insert("raw", qrText);  // 回填原始二维码字符串
    out.insert("ok", false);    // 默认不存在

    QString projectId;  // 项目ID
    QString batchCode;  // 批次编码
    QString perr;       // 解析错误

    if (!parseQrText(qrText, projectId, batchCode, perr)) {  // 解析失败
        out.insert("projectId", -1);                         // 无效 projectId
        out.insert("batchCode", "");                         // 空 batchCode
        out.insert("error", perr);                           // 写解析错误
        return false;                                        // 返回失败
    }

    out.insert("projectId", projectId);  // 回填 projectId
    out.insert("batchCode", batchCode);  // 回填 batchCode

    QString derr;                                            // 数据库错误
    const bool ok = exists(db, projectId, batchCode, derr);  // ✅ 只判断存在性

    out.insert("ok", ok);                                                              // 写 ok
    out.insert("error", ok ? "" : (derr.isEmpty() ? "未找到该项目+批次配置" : derr));  // 写 error

    return ok;  // 返回是否存在
}
static bool parseJsonString(const QString& jsonText, QJsonObject& outObj) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    outObj = doc.object();
    return true;
}

bool parseMethodLibraryRow(const QJsonObject& row, QrMethodConfig& cfg, QString& err) {
    // 1. type -> projectId
    cfg.projectId = row.value("type").toString();
    if (cfg.projectId.isEmpty()) {
        err = "type 缺失";
        return false;
    }

    // 2. serial -> batchCode
    cfg.batchCode = row.value("serial").toString();
    if (cfg.batchCode.isEmpty()) {
        err = "serial 缺失";
        return false;
    }

    // 3. methodName
    cfg.methodName = row.value("methodName").toString();
    cfg.projectName = cfg.methodName;

    // 4. methodPara
    cfg.methodData = row.value("methodPara").toString();

    // 5. Created
    cfg.updatedAt = row.value("Created").toString();

    // 6. incubationPara
    QJsonObject incubation;
    if (!parseJsonString(row.value("incubationPara").toString(), incubation)) {
        err = "incubationPara 解析失败";
        return false;
    }

    cfg.temperature = incubation.value("temperature").toDouble();
    cfg.timeSec = incubation.value("time").toInt() * 60;

    // 7. C_T_POS
    QJsonObject ctPos;
    if (!parseJsonString(row.value("C_T_POS").toString(), ctPos)) {
        err = "C_T_POS 解析失败";
        return false;
    }

    cfg.C1 = ctPos.value("C1").toInt();
    cfg.C2 = ctPos.value("C2").toInt();
    cfg.T1 = ctPos.value("T1").toInt();
    cfg.T2 = ctPos.value("T2").toInt();

    return true;
}
bool upsertQrMethodConfig(QSqlDatabase& db,
                          const QrMethodConfig& cfg,
                          QString& err) {
    QSqlQuery q(db);

    const char* sql = R"SQL(
INSERT INTO qr_method_config(
    projectId,
    projectName,
    batchCode,
    updated_at,
    methodName,
    methodData,
    temperature,
    timeSec,
    C1, T1, C2, T2
) VALUES (
    :projectId,
    :projectName,
    :batchCode,
    :updated_at,
    :methodName,
    :methodData,
    :temperature,
    :timeSec,
    :C1, :T1, :C2, :T2
)
ON CONFLICT(projectId, batchCode) DO UPDATE SET
    projectName = excluded.projectName,
    updated_at  = excluded.updated_at,
    methodName  = excluded.methodName,
    methodData  = excluded.methodData,
    temperature = excluded.temperature,
    timeSec     = excluded.timeSec,
    C1          = excluded.C1,
    T1          = excluded.T1,
    C2          = excluded.C2,
    T2          = excluded.T2
)SQL";

    if (!q.prepare(sql)) {
        err = q.lastError().text();
        return false;
    }

    q.bindValue(":projectId", cfg.projectId);
    q.bindValue(":projectName", cfg.projectName);
    q.bindValue(":batchCode", cfg.batchCode);
    q.bindValue(":updated_at", cfg.updatedAt);
    q.bindValue(":methodName", cfg.methodName);
    q.bindValue(":methodData", cfg.methodData);
    q.bindValue(":temperature", cfg.temperature);
    q.bindValue(":timeSec", cfg.timeSec);
    q.bindValue(":C1", cfg.C1);
    q.bindValue(":T1", cfg.T1);
    q.bindValue(":C2", cfg.C2);
    q.bindValue(":T2", cfg.T2);

    if (!q.exec()) {
        err = q.lastError().text();
        return false;
    }

    return true;
}
