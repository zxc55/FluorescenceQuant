#pragma once
#include <QObject>
#include <QVector>

#include "IIOReaderThread.h"

// 设备层批数据 → 透传给上层（MainViewModel/QML）
class IIODeviceController : public QObject {
    Q_OBJECT
public:
    explicit IIODeviceController(QObject* parent = nullptr);

public slots:
    void start();
    void stop();

    // ====== 转发配置（给 MainViewModel 调用）======
    // 采样/批次
    void setDeviceIndex(int idx) { reader.setDeviceIndex(idx); }
    void setDesiredRateHz(int hz) { reader.setDesiredRateHz(hz); }
    void setBatchPeriodMs(int ms) { reader.setBatchPeriodMs(ms); }
    void setSamplesPerBatch(int count) { reader.setSamplesPerBatch(count); }

    // 滤波
    void setBypassFiltering(bool on) { reader.setBypassFiltering(on); }
    void setMedianEnabled(bool on) { reader.setMedianEnabled(on); }
    void setEma(bool enabled, double alpha = 0.25) { reader.setEma(enabled, alpha); }
    void setMovingAverage(bool enabled, int w = 5) { reader.setMovingAverage(enabled, w); }

    // 两点校准
    void setCalibrationEnabled(bool on) { reader.setCalibrationEnabled(on); }
    void setTwoPointCalib(double x1, double y1, double x2, double y2) { reader.setTwoPointCalib(x1, y1, x2, y2); }

signals:
    // 每 50ms 推送一批（~25 点），单位：伏
    void newDataBatch(const QVector<double>& values);

private:
    IIOReaderThread reader;
};
