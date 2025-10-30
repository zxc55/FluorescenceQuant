#pragma once
#include <QObject>
#include <QVector>

class IIOReaderThread;

class IIODeviceController : public QObject {
    Q_OBJECT
public:
    explicit IIODeviceController(QObject* parent = nullptr);
    ~IIODeviceController();

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    // 可从 MainViewModel 调这些接口做细调
    Q_INVOKABLE void setDeviceIndex(int idx);
    Q_INVOKABLE void setTargetHz(int hz);
    Q_INVOKABLE void setWatermark(int n);
    Q_INVOKABLE void setBufferLength(int n);
    Q_INVOKABLE void setTriggerName(const QString& name);

signals:
    void newDataBatch(const QVector<double>& values);
    void logMessage(const QString& msg);

private:
    IIOReaderThread* reader_ = nullptr;
    int devIndex_ = 0;
    int targetHz_ = 500;
    int watermark_ = 25;
    int bufLen_ = 1024;
    QString trigName_;  // 为空则自动取 trigger0/name
};
