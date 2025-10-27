// IIODeviceController.h
#ifndef IIODEVICECONTROLLER_H_
#define IIODEVICECONTROLLER_H_

#include <QObject>

#include "IIOReaderThread.h"

class IIODeviceController : public QObject {
    Q_OBJECT
public:
    explicit IIODeviceController(QObject* parent = nullptr);
    void start(int intervalMs = 50, int samples = 10);
    void stop();

signals:
    void newData(double value);

private:
    IIOReaderThread reader;
};

#endif  // IIODEVICECONTROLLER_H_