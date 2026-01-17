#ifndef QRREPOMODEL_H_
#define QRREPOMODEL_H_
#include <QObject>      // QObject 基类
#include <QPointer>     // QPointer 安全指针
#include <QString>      // QString
#include <QVariantMap>  // QVariantMap

class DBWorker;  // 前置声明 DBWorker

class QrRepoModel : public QObject  // 主线程代理：给 QML 用
{
Q_OBJECT                                                                          // Qt 元对象宏
    public :                                                                      // 公共区
              explicit QrRepoModel(DBWorker* worker, QObject* parent = nullptr);  // 构造：绑定 worker
    ~QrRepoModel() override = default;                                            // 析构

    Q_INVOKABLE void postLookupQrMethodConfig(const QString& qrText);  // QML 调用：投递查存在性任务
    Q_INVOKABLE void postSaveQrMethodConfig(const QVariantMap& cfg);

signals:                                     // 信号区（给 QML）
    void ready();                            // 转发：DB ready
    void errorOccurred(const QString& msg);  // 转发：DB error
    void saveQrMethodConfigDone(bool ok, const QString& err);

    void qrMethodConfigLookedUp(const QVariantMap& result);  // 转发：查询结果（ok/error/字段）

private:                         // 私有成员
    QPointer<DBWorker> worker_;  // 保存 DBWorker 指针（防悬空）
};
#endif  // QRREPOMODEL_H_