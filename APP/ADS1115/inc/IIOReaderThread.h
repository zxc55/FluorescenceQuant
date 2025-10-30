#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>
#include <thread>
#include <vector>

class IIOReaderThread : public QObject {
    Q_OBJECT
public:
    explicit IIOReaderThread(QObject* parent = nullptr);
    ~IIOReaderThread();

    // 启动/停止
    void start();
    void stop();

    // 设备与速率（仅用于选择/日志）
    void setDeviceIndex(int idx) { deviceIndex = idx; }
    void setDesiredRateHz(int hz) { desiredRateHz = hz; }

    // 批采样配置（本需求关键）：每 batchPeriodMs 采 samplesPerBatch 个点
    void setBatchPeriodMs(int ms) { batchPeriodMs = (ms > 5 ? ms : 5); }
    void setSamplesPerBatch(int count) { samplesPerBatch = (count > 1 ? count : 1); }

    // 两点校准 y = A*x + B（可选）
    void setTwoPointCalib(double x1, double y1, double x2, double y2);
    void setCalibrationEnabled(bool on) { calibEnabled = on; }

    // 滤波链（顺序：中值 → 滑动平均 → EMA）
    void setBypassFiltering(bool on) { bypassFiltering = on; }
    void setMedianEnabled(bool on) { medianEnabled = on; }
    void setEma(bool enabled, double alpha = 0.25);
    void setMovingAverage(bool enabled, int window = 5);

signals:
    // 每批推送（单位：伏），默认每 50ms 推 25 点
    void newDataBatch(const QVector<double>& values);

private:
    void run();
    bool ensureSysfsReady();
    bool readRawValue(int& raw);
    bool readScale(double& scale);
    double rawToVolt(int raw, double scale);
    static bool isLikelyMilliVoltScale(double s);

private:
    std::atomic<bool> running{false};
    std::thread worker;

    // 批采样参数
    int batchPeriodMs{50};    // 每 50ms 一个批次
    int samplesPerBatch{25};  // 每批 25 点 ≈ 500 Hz

    int desiredRateHz{500};
    int deviceIndex{0};

    // sysfs 路径缓存
    QString sysfsDevDir;
    QString rawPath;               // in_voltage0_raw
    QString altRawPath;            // in_voltage0_input
    QString scalePath;             // in_voltage0_scale
    double lastGoodScale{0.0625};  // mV/LSB（PGA=±2.048V 典型）

    // —— 滤波 —— //
    static constexpr int MED_WIN = 5;
    double medBuf[MED_WIN] = {0};
    int medCount = 0;
    bool bypassFiltering = true;
    bool medianEnabled = false;

    // 滑动平均（Moving Average）
    bool movEnabled = false;
    int movWindow = 5;
    std::vector<double> maBuf;
    int maIdx = 0;
    int maCount = 0;
    double maSum = 0.0;

    // EMA
    bool emaEnabled = false;
    double emaAlpha = 0.25;
    double emaState = 0.0;
    bool emaInit = false;

    // 两点校准 y = A*x + B
    bool calibEnabled = false;
    double calibA = 1.0;
    double calibB = 0.0;
};
