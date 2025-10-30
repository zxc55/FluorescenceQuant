#include "MainViewModel.h"

#include <QDebug>

#include "ADS1115/inc/IIODeviceController.h"

MainViewModel::MainViewModel(QObject* parent)
    : QObject(parent) {
    deviceController = new IIODeviceController(this);

    // （可选）在此覆盖设备层滤波策略
    deviceController->setBypassFiltering(false);  // 启用滤波链
    deviceController->setMovingAverage(true, 5);  // 滑动平均窗口=5
    deviceController->setMedianEnabled(false);    // 如需抗毛刺可设 true
    deviceController->setEma(false);              // 如需再平滑可设 true

    // 批数据透传至 QML
    connect(deviceController, &IIODeviceController::newDataBatch,
            this, &MainViewModel::newDataBatch);
}

void MainViewModel::startReading() {
    deviceController->start();
}

void MainViewModel::stopReading() {
    deviceController->stop();
}
