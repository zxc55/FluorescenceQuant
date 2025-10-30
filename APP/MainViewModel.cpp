#include "MainViewModel.h"

#include <QDebug>

MainViewModel::MainViewModel(QObject* parent)
    : QObject(parent) {
    // 采集数据转发给 QML
    connect(&controller, &IIODeviceController::newData,
            this, &MainViewModel::newData);
}

void MainViewModel::startReading() {
    // 50ms 轮询, 475Hz 采样, iio:device0
    controller.start(50, 475, 0);
    qInfo() << "开始采集：interval=50ms, rate=475Hz, device=iio:device0";
}

void MainViewModel::stopReading() {
    controller.stop();
    qInfo() << "停止采集";
}
