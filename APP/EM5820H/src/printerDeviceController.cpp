#include "printerDeviceController.h"

#include <QDebug>

PrinterDeviceController::PrinterDeviceController(QObject* parent)
    : QObject(parent) {
}

void PrinterDeviceController::start() {
    qDebug() << "[PrinterDeviceController] start()";
    reader1.start();  // 调用底层 PrinterManager::start()
}

void PrinterDeviceController::stop() {
    qDebug() << "[PrinterDeviceController] stop()";
    reader1.stop();  // 调用底层 PrinterManager::stop()
}

void PrinterDeviceController::test() {
    qDebug() << "[PrinterDeviceController] test()";
    // 这里用组合测试；若只想测设置，可改为 reader1.testSettings();
    reader1.testText();  // 调用底层 PrinterManager::test()
}
