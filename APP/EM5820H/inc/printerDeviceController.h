#ifndef PRINTERDEVICECONTROLLER_H_
#define PRINTERDEVICECONTROLLER_H_
#include <QObject>

#include "PrinterManager.h"

class PrinterDeviceController : public QObject {
    Q_OBJECT
public:
    explicit PrinterDeviceController(QObject* parent = nullptr);
    void start();
    void stop();
    void test();
    int a, b, c;

private:
    PrinterManager& redar1;
};
#endif  // PRINTERDEVICECONTROLLER_H_
