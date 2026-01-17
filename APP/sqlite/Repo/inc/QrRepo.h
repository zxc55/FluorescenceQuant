#ifndef QRREPO_H_
#define QRREPO_H_
#include <QSqlDatabase>  // QSqlDatabase
#include <QString>       // QString
#include <QVariantMap>   // QVariantMap

#include "DTO.h"
struct QrMethodConfig {
    QString projectId;  // type
    QString batchCode;  // serial

    QString projectName;  // methodName
    QString methodName;   // methodName
    QString methodData;   // methodPara (JSON string)

    QString updatedAt;  // Created

    double temperature;  // incubationPara.temperature
    int timeSec;         // incubationPara.time * 60

    int C1;
    int C2;
    int T1;
    int T2;
};
class QrRepo  // 二维码配置表 Repo
{             // 类开始
public:       // 公共接口开始
    // 按 projectId + batchCode 查询 qr_method_config，查到返回 true，并填充 out                       // 注释
    static bool selectOne(QSqlDatabase db, QString& projectId, const QString& batchCode, QVariantMap& out);  // 按键查询

    // 按二维码字符串查询：格式 id:项目id;bn:批次编码;，解析后查表，查到返回 true，并填充 out            // 注释
    static bool selectOneByQrText(QSqlDatabase db, const QString& qrText, QVariantMap& out);  // 按二维码查

    // 保存/覆盖（存在则更新，不存在则插入），依赖 UNIQUE(projectId,batchCode)                        // 注释
    static bool upsert(QSqlDatabase db, const QVariantMap& cfg);  // 保存/覆盖

    // 删除配置（按 projectId + batchCode）                                                             // 注释
    static bool deleteOne(QSqlDatabase db, QString& projectId, const QString& batchCode);             // 删除一条
    static bool existsByQrText(QSqlDatabase db, const QString& qrText, QVariantMap& out);             // ✅ 输入二维码字符串，只判断是否存在
    static bool exists(QSqlDatabase db, QString& projectId, const QString& batchCode, QString& err);  // ✅ 输入键，只判断是否存在

private:  // 私有接口开始
    // 解析二维码字符串：id:xx;bn:yy;                                                                  // 注释
    static bool parseQrText(const QString& qrText, QString& projectId, QString& batchCode, QString& err);  // 解析函数
};
#endif  // QRREPO_H_