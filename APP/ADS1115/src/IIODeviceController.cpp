// IIODeviceController.cpp
#include "IIODeviceController.h"

IIODeviceController::IIODeviceController(QObject* parent)
    : QObject(parent) {
    connect(&reader, &IIOReaderThread::newData, this, &IIODeviceController::newData);
}

void IIODeviceController::start(int intervalMs, int samples) {
    reader.start(intervalMs, samples);
}

void IIODeviceController::stop() {
    reader.stop();
}
