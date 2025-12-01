#ifndef PRINTERMANAGER_H
#define PRINTERMANAGER_H

#include <QObject>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "printer_lib.h"

// 打印机管理类：使用 std::thread 后台运行
class PrinterManager : public QObject {
    Q_OBJECT
public:
    // 启动和停止后台线程
    void start();
    void stop();
    void testSettings();
    void testText();
    bool initPrinter();
    static PrinterManager& instance();
    void postPrint(const std::function<void()>& job);
    printer_t* printer;  // 打印机对象指针
    const printer_t* getPrinter() const { return printer; }

private:
    PrinterManager();
    ~PrinterManager();
    PrinterManager(const PrinterManager&) = delete;
    PrinterManager& operator=(const PrinterManager&) = delete;

    // ❌ 禁止移动与移动赋值（C++11 起）
    PrinterManager(PrinterManager&&) = delete;
    PrinterManager& operator=(PrinterManager&&) = delete;
    // 线程主逻辑
    void run();
    // 各类功能函数

    void setupListeners();
    void testQRCode();
    void testBarcode();
    void deinitPrinter();

private:
    std::thread worker;         // 后台线程
    std::atomic<bool> running;  // 线程运行标志
    std::queue<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
struct PrintData {
    QString projectName;     // 项目名称
    QString sampleNo;        // 样品编号
    QString sampleSource;    // 样品来源
    QString sampleName;      // 样品名称
    QString standardCurve;   // 标准曲线
    QString batchCode;       // 批次编码
    double detectedConc;     // 检测浓度
    double referenceValue;   // 参考值
    QString result;          // 检测结果（合格/不合格）
    QString detectedTime;    // 检测时间
    QString detectedUnit;    // 单位
    QString detectedPerson;  // 检测人
    QString dilutionInfo;    // 稀释倍数
};
Q_DECLARE_METATYPE(PrintData)
#endif  // PRINTERMANAGER_H
