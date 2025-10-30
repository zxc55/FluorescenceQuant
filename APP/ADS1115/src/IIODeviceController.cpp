#include "IIODeviceController.h"

#include <QDebug>
#include <QMetaObject>

#include "IIOReaderThread.h"

IIODeviceController::IIODeviceController(QObject* parent)
    : QObject(parent) {}

IIODeviceController::~IIODeviceController() {
    stop();
}

void IIODeviceController::start(int intervalMs, int desiredRateHz, int deviceIndex) {
    // 若已在跑，先停
    if (worker) {
        qWarning() << "[IIODeviceController] already running; restarting...";
        stop();
    }

    // 创建 worker 并放入线程
    worker = new IIOReaderThread();
    worker->moveToThread(&workerThread);

    // 线程结束时清理 worker
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    // 转发采样数据到上层（UI）
    connect(worker, &IIOReaderThread::newData, this, &IIODeviceController::newData);

    // 启动线程
    workerThread.start();

    // 在线程上下文里配置并启动
    QMetaObject::invokeMethod(worker, [=]() {
        // 先设置设备与速率
        worker->setDesiredRateHz(desiredRateHz);
        worker->setDeviceIndex(deviceIndex);

        // 开启滑动平均滤波
        worker->setMovAvgEnabled(false);
        worker->setMovAvgWindow(5);

        // 启动轮询（第二个参数 samplesPerRead 目前未用，传 1 即可）
        worker->start(intervalMs, 1); }, Qt::QueuedConnection);
}

void IIODeviceController::stop() {
    if (!worker) {
        if (workerThread.isRunning()) {
            workerThread.quit();
            workerThread.wait();
        }
        return;
    }

    // 在线程上下文里安全停掉 worker
    QMetaObject::invokeMethod(worker, [this]() { worker->stop(); }, Qt::BlockingQueuedConnection);

    // 退出线程并等待
    workerThread.quit();
    workerThread.wait();

    // 指针交给 finished->deleteLater 清理；这里将本地指针置空
    worker = nullptr;

    qInfo() << "[IIODeviceController] stopped";
}
