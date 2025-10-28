#ifndef PRINTERMANAGER_H
#define PRINTERMANAGER_H

#include <QObject>
#include <atomic>
#include <chrono>
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
    const printer_t* printer;   // 打印机对象指针
    std::thread worker;         // 后台线程
    std::atomic<bool> running;  // 线程运行标志
};

#endif  // PRINTERMANAGER_H
