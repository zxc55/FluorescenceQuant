#include "IIODeviceController.h"

#include <QDebug>

IIODeviceController::IIODeviceController(QObject* parent)
    : QObject(parent) {
    // 设备层 → 控制层 透传
    connect(&reader, &IIOReaderThread::newDataBatch,
            this, &IIODeviceController::newDataBatch);
}

void IIODeviceController::start() {
    qDebug() << "[IIODeviceController] start()";
    // 缺省配置：50ms 一批、25 点/批（≈500Hz），无打印
    reader.setDeviceIndex(0);
    reader.setDesiredRateHz(500);
    reader.setBatchPeriodMs(50);
    reader.setSamplesPerBatch(25);

    // 默认开启滑动均值、关闭中值/EMA（你也可在 MainViewModel 覆盖）
    reader.setBypassFiltering(false);
    reader.setMovingAverage(true, 5);
    reader.setMedianEnabled(false);
    reader.setEma(false);

    reader.setCalibrationEnabled(false);

    reader.start();
}

void IIODeviceController::stop() {
    qDebug() << "[IIODeviceController] stop()";
    reader.stop();
}
