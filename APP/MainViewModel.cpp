// MainViewModel.cpp
#include "MainViewModel.h"

MainViewModel::MainViewModel(QObject* parent)
    : QObject(parent) {
    connect(&controller, &IIODeviceController::newData, this, &MainViewModel::newData);
}

void MainViewModel::startReading() {
    controller.start(50, 20);
    controller1.start();
}

void MainViewModel::stopReading() {
    controller.stop();
    controller1.test();
}
void MainViewModel::printer_test() {
}