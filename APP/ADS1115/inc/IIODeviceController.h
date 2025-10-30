#pragma once
#include <QObject>
#include <QThread>

class IIOReaderThread;

/**
 * IIODeviceController
 * - 管理 IIOReaderThread 跑在独立 QThread 中
 * - 提供 start/stop 接口
 * - 将 worker 的 newData(double) 转发给 UI
 */
class IIODeviceController : public QObject {
    Q_OBJECT
public:
    explicit IIODeviceController(QObject* parent = nullptr);
    ~IIODeviceController();

    // intervalMs: 轮询周期(ms)
    // desiredRateHz: ADS1115 目标 ODR（比如 475）
    // deviceIndex: 选择 iio:deviceX 的 X
    void start(int intervalMs = 50, int desiredRateHz = 475, int deviceIndex = 0);
    void stop();

signals:
    void newData(double value);

private:
    QThread workerThread;
    IIOReaderThread* worker{nullptr};
};
