#include "QrRepoModel.h"  // 本类头文件

#include <QDebug>       // qWarning
#include <QMetaObject>  // invokeMethod

#include "DBWorker.h"  // DBWorker 定义（你现有类）
#include "QrRepo.h"

QrRepoModel::QrRepoModel(DBWorker* worker, QObject* parent)  // 构造函数
    : QObject(parent)                                        // 调用父类构造
      ,
      worker_(worker)  // 保存 worker 指针
{
    if (!worker_)  // worker 为空判断
        return;    // 返回

    QObject::connect(worker_, &DBWorker::ready,  // worker ready
                     this, &QrRepoModel::ready,  // 转发到 model
                     Qt::QueuedConnection);      // ✅ 跨线程排队回主线程

    QObject::connect(worker_, &DBWorker::errorOccurred,  // worker error
                     this, &QrRepoModel::errorOccurred,  // 转发到 model
                     Qt::QueuedConnection);              // ✅ 跨线程排队回主线程

    QObject::connect(worker_, &DBWorker::qrMethodConfigLookedUp,  // worker 查询结果
                     this, &QrRepoModel::qrMethodConfigLookedUp,  // 转发到 model
                     Qt::QueuedConnection);                       // ✅ 跨线程排队回主线程
}

void QrRepoModel::postLookupQrMethodConfig(const QString& qrText)  // QML 调用入口
{
    if (!worker_) {                                    // worker 已释放
        qWarning() << "[QrRepoModel] worker is null";  // 打印错误
        return;                                        // 返回
    }

    const bool ok = QMetaObject::invokeMethod(  // 排队到 worker 所在线程执行
        worker_,                                // 目标对象：DBWorker（在 dbThread）
        "postLookupQrMethodConfig",             // 调用的方法名（Q_INVOKABLE）
        Qt::QueuedConnection,                   // ✅ 强制排队
        Q_ARG(QString, qrText)                  // 参数：二维码字符串
    );

    if (!ok) {                                              // invoke 失败
        qWarning() << "[QrRepoModel] invokeMethod failed";  // 打印错误
    }
}
void QrRepoModel::postSaveQrMethodConfig(const QVariantMap& cfg) {
    worker_->postSaveQrMethodConfig(cfg);
}