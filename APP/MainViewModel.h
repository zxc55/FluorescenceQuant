#pragma once
#include <QObject>

#include "ADS1115/inc/IIODeviceController.h"

/**
 * MainViewModel
 * - 暴露给 QML 的 VM
 * - 转发 newData 到 QML（用于画实时曲线/数值）
 */
class MainViewModel : public QObject {
    Q_OBJECT
public:
    explicit MainViewModel(QObject* parent = nullptr);

    Q_INVOKABLE void startReading();  // QML按钮 -> 开始采集
    Q_INVOKABLE void stopReading();   // QML按钮 -> 停止采集

signals:
    void newData(double value);  // 给 QML 的信号

private:
    IIODeviceController controller;  // 后台采集控制器
};
