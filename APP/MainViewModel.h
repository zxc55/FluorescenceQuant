// MainViewModel.h
#ifndef MAINVIEWMODEL_H_
#define MAINVIEWMODEL_H_

#include <QObject>

#include "IIODeviceController.h"
#include "printerDeviceController.h"
class MainViewModel : public QObject {
    Q_OBJECT
public:
    explicit MainViewModel(QObject* parent = nullptr);

    Q_INVOKABLE void startReading();
    Q_INVOKABLE void stopReading();
    Q_INVOKABLE void printer_test();

signals:
    void newData(double value);

private:
    IIODeviceController controller;
    PrinterDeviceController controller1;
};
#endif  // MAINVIEWMODEL_H_