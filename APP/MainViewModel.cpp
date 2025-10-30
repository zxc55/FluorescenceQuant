#include "MainViewModel.h"

#include <QDebug>

#include "ADS1115/inc/IIODeviceController.h"

MainViewModel::MainViewModel(QObject* parent)
    : QObject(parent) {
    deviceController = new IIODeviceController(this);

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
