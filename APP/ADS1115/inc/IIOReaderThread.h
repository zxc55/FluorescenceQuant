#pragma once
#include <QObject>
#include <QVector>
#include <atomic>
#include <string>
#include <thread>

class IIOReaderThread : public QObject {
    Q_OBJECT
public:
    explicit IIOReaderThread(QObject* parent = nullptr);
    ~IIOReaderThread();

    void setDeviceIndex(int idx) { deviceIndex_ = idx; }
    void setTargetHz(int hz) { targetHz_ = hz; }
    void setWatermark(int n) { watermark_ = n; }
    void setBufferLength(int n) { bufLen_ = n; }
    void setTriggerName(const QString& name) { triggerName_ = name; }

    bool start();
    void stop();

signals:
    void newDataBatch(const QVector<double>& values);

private:
    bool ensureSysfsReady();
    void teardownSysfs();
    void threadMain();

    static bool writeTextFile(const QString& path, const QByteArray& data);
    static bool readTextFile(const QString& path, QByteArray& out);
    static QString findIioDevDir(int index);
    static QString devNodeFromDir(const QString& devDir);
    static bool fileExists(const QString& path);

private:
    int deviceIndex_ = 0;
    int targetHz_ = 500;
    int watermark_ = 25;
    int bufLen_ = 1024;
    QString triggerName_;

    std::atomic<bool> running_{false};
    std::thread worker_;
    int fd_ = -1;

    QString devDir_;
    QString devNode_;
    QString scanRoot_;
    QString bufRoot_;
    QString trigRoot_;
    QString scalePath_;
    double lastScale_ = 0.0625;  // mV/LSB
    bool configured_ = false;
};
