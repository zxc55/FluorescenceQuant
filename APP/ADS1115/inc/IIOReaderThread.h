#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <deque>

class IIOReaderThread : public QObject {
    Q_OBJECT
public:
    explicit IIOReaderThread(QObject* parent = nullptr);
    ~IIOReaderThread();

    // 启动/停止（intervalMs 轮询周期；samplesPerRead 预留未用）
    void start(int intervalMs = 50, int samplesPerRead = 1);
    void stop();

    // 配置：目标 ODR 与设备序号（iio:deviceX 的 X）
    void setDesiredRateHz(int hz) { desiredRateHz = hz; }
    void setDeviceIndex(int idx) { deviceIndex = idx; }

    // 滤波控制（默认开启，窗口=5）
    void setMovAvgEnabled(bool on) {
        filterEnabled = on;
        clearFilter();
    }
    void setMovAvgWindow(int w) {
        filterWindow = (w > 1 ? w : 1);
        clearFilter();
    }

signals:
    void newData(double value);  // 推送折算后的电压(单位：伏)

private slots:
    void readIIODevice();  // 定时轮询
    void watchdogTick();   // 看门狗自愈

private:
    // ---------- 工具 ----------
    bool ensureSysfsReady();  // 找目录、关buffer、设采样率、定位 raw/scale
    void recoverSysfs();      // 自愈流程
    bool writeSysfsString(const QString& path, const QByteArray& s);
    bool readSysfsIntPosix(const QString& path, int& out);   // 稳健读整数
    bool readSysfsDouble(const QString& path, double& out);  // 读浮点
    static bool parseFirstIntLoose(const QByteArray& bytes, int& out);
    void warmupReads(const QString& rawPath, int tries, int sleepMs);
    static double voltageFromRawAndScale(int raw, double scale);

    // ---------- 滤波 ----------
    double applyMovingAverage(double x);
    void clearFilter() {
        fifo.clear();
        fifoSum = 0.0;
    }

private:
    // 定时器
    QTimer* timer{nullptr};
    QTimer* watchdog{nullptr};

    // 状态
    bool running{false};
    int intervalMs_{50};
    int samplesPerRead_{1};

    // IIO sysfs 路径缓存
    QString sysfsDevDir;          // /sys/bus/iio/devices/iio:deviceX
    QString rawAttrPathCached;    // in_voltage0_raw / in_voltage0_input
    QString scaleAttrPathCached;  // in_voltage0_scale
    QString powerControlPath;     // power/control

    // 设备选择与速率
    int deviceIndex{0};      // iio:deviceX 的 X
    int desiredRateHz{475};  // ADS1115 的 475Hz 逼近 500Hz

    // 卡滞检测
    int sameCount{0};
    const int stallThreshold{100};
    int lastRaw{INT_MIN};
    QElapsedTimer lastEmitTick;

    // 滤波（滑动平均）
    bool filterEnabled{false};
    int filterWindow{5};
    std::deque<double> fifo;
    double fifoSum{0.0};
    bool scaleIsMilli_ = true;      // true 表示 scale 是 mV/LSB
    double lastGoodScale = 0.0625;  // 默认兜底：0.0625 mV/LSB（ADS1115 ±2.048V）
    // —— 最近一次有效的量，用于兜底 ——
    //  double lastGoodScale{0.0001875};  // 0.1875mV/LSB => 0.0001875V
    double lastGoodValue{0.0};
    int invalidLogCounter{0};  // 限频打印
};
