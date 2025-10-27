#ifndef IIOREADERTHREAD_H_
#define IIOREADERTHREAD_H_

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

// 只读通道0（AIN0）的 IIO 轮询采集器（sysfs 轮询，不用触发器/缓冲）
class IIOReaderThread : public QObject {
    Q_OBJECT
public:
    explicit IIOReaderThread(QObject* parent = nullptr);
    ~IIOReaderThread();

    // intervalMs: 轮询周期（默认 50ms）
    void start(int intervalMs = 50, int samplesPerRead = 1);
    void stop();

signals:
    // 新电压值（单位：伏）
    void newData(double value);

private slots:
    void readIIODevice();  // 定时采样
    void watchdogTick();   // 看门狗（卡住时自恢复）

private:
    // 初始化/恢复：定位 iio:deviceX、禁用 autosuspend、关 buffer、设采样率、探测通道0 raw/scale
    bool ensureSysfsReady();
    void recoverSysfs();

    // sysfs 读写工具
    bool writeSysfsString(const QString& path, const QByteArray& s);
    bool readSysfsIntPosix(const QString& path, int& out);   // POSIX 读取 int（稳）
    bool readSysfsDouble(const QString& path, double& out);  // Qt 读取 double（足够）

    // 写完采样率后的预热读取（避免短暂空读）
    void warmupReads(const QString& rawPath, int tries = 6, int sleepMs = 5);

    // 单位换算（scale<0.1→V/LSB，否则按 mV/LSB 或 raw≈mV 处理）
    static double voltageFromRawAndScale(int raw, double scale);

private:
    QTimer* timer = nullptr;
    QTimer* watchdog = nullptr;
    bool running = false;
    int intervalMs_ = 50;

    // 缓存路径
    QString sysfsDevDir;          // /sys/bus/iio/devices/iio:deviceX
    QString rawAttrPathCached;    // 仅通道0：in_voltage0_raw 或 in_voltage0_input
    QString scaleAttrPathCached;  // 仅通道0：in_voltage0_scale
    QString powerControlPath;     // /power/control（禁用 autosuspend）

    // 卡住检测
    int lastRaw = INT_MIN;
    int sameCount = 0;
    int stallThreshold = 100;  // 连续相同100次（约5s@50ms）视为卡住
    QElapsedTimer lastEmitTick;
};
#endif  // IIOREADERTHREAD_H