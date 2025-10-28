#include "printerDeviceController.h"

#include <QDebug>

#include "PrinterManager.h"

PrinterDeviceController::PrinterDeviceController(QObject* parent)
    : QObject(parent), redar1(PrinterManager::instance()), a(3) {
}

void PrinterDeviceController::start() {
    qDebug() << "[PrinterDeviceController] start()";
    redar1.start();  // 调用底层 PrinterManager::start()
}

void PrinterDeviceController::stop() {
    qDebug() << "[PrinterDeviceController] stop()";
    redar1.stop();  // 调用底层 PrinterManager::stop()
}

void PrinterDeviceController::test() {
    qDebug() << "[PrinterDeviceController] test()";
    // 这里用组合测试；若只想测设置，可改为 reader1.testSettings();
    redar1.testText();
    // reader1.testText();  // 调用底层 PrinterManager::test()
}
