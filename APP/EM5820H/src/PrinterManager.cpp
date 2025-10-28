#include "PrinterManager.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <QDebug>

#include "printer_lib.h"
#include "printer_type.h"
PrinterManager::PrinterManager()
    : printer(nullptr), running(false) {
}

PrinterManager::~PrinterManager() {
    stop();
    // deinitPrinter();
}
void PrinterManager::start() {
    if (running.load()) {
        qWarning() << "Printer thread already running!";
        return;
    }
    running = true;
    worker = std::thread(&PrinterManager::run, this);
    qDebug() << "Printer thread started.";
}
void PrinterManager::stop() {
    if (!running.load())
        return;
    running = false;
    if (worker.joinable())
        worker.join();
    qDebug() << "Printer thread stopped.";
}
bool PrinterManager::initPrinter() {
    // #ifdef DLOCAL_BUILD

    printer = new_printer();
    int err = printer->device()
                  ->usb()
                  ->parm(0x1d81, 0x5721)
                  ->init();

    if (err) {
        qWarning() << "Printer init failed:" << err;
        return false;
    }

    qDebug() << "Printer init success";
    // #endif
    return true;
}
PrinterManager& PrinterManager::instance() {
    static PrinterManager instance;
    return instance;
}
void PrinterManager::setupListeners() {
    // printer->listener()
    //     ->no_paper(1, []() {
    //         qWarning() << "printer no paper !!!";
    //         // 在这里可以加上你的自定义逻辑
    //         // 例如：蜂鸣器报警、上报日志等
    //     })
    //     ->temp_high(1, []() {
    //         qWarning() << "printer temp high !!!";
    //         // 例如：风扇开启或报警提示
    //     })
    //     ->paper_ok(1, []() {
    //         qDebug() << "printer paper ok !!!";
    //     })
    //     ->temp_ok(1, []() {
    //         qDebug() << "printer temp ok !!!";
    //     })
    //     ->usb_disconnect(1, []() {
    //         qWarning() << "usb disconnect event!!!";
    //     })
    //     ->usb_connect(1, []() {
    //         qDebug() << "usb connect event!!!";
    //     })
    //     ->on();
}  // ---------------- 测试设置 ----------------
void PrinterManager::testSettings() {
    execute_ret_t ret;

    printer->setting()->action(Open_Cash_Box, &ret, 500);
    if (!ret.result)
        qDebug() << "open cash ok";

    printer->setting()->assign_number(Encoding_Type, ENCODING_CP949, &ret, 500);
    if (!ret.result)
        qDebug() << "current language:" << ret.data.value;

    printer->setting()->assign_string(Machine_Name, "荧光定量检测", &ret, 500);
    if (!ret.result)
        qDebug() << "machine name:" << ret.data.string;

    printer->setting()->query(Print_Usage_Record, &ret, 500);
    if (!ret.result) {
        if (ret.type == STRING)
            qDebug() << "use log:" << ret.data.string;
        else if (ret.type == NUMBER)
            qDebug() << "use log:" << ret.data.value << "mm";
    }

    printer->setting()->query(Machine_Name, &ret, 500);
    if (!ret.result) {
        if (ret.type == STRING)
            qDebug() << "name:" << ret.data.string;
        else if (ret.type == NUMBER)
            qDebug() << "name:" << ret.data.value;
    }
}

// ---------------- 打印文本 ----------------
void PrinterManager::testText() {
    // if (!initPrinter())
    //     return;
    uint8_t CP936[] = u8"CodePage 936:东为打印机";
    uint8_t CP949[] = u8"CodePage 949:동위프린터테스트";
    uint8_t CP950[] = u8"CodePage 950:小票打印機";
    uint8_t CP932[] = u8"CodePage 932:東為（とうい）プリンターテスト";

    printer->text()->encoding(ENCODING_CP936)->utf8_text(CP936)->newline();
    printer->text()->encoding(ENCODING_CP949)->utf8_text(CP949)->newline();
    printer->text()->encoding(ENCODING_CP950)->utf8_text(CP950)->newline();
    printer->text()->encoding(ENCODING_CP932)->utf8_text(CP932)->print();
    // printer->qr_code()
    //     ->align(ALIGN_RIGHT)
    //     ->set("https://www.bilibili.com/", 5, QR_ECC_H)
    //     ->print();
    qDebug() << "printer 22222" << printer;
}

// ---------------- 打印二维码 ----------------
void PrinterManager::testQRCode() {
    printer->qr_code()
        ->align(ALIGN_RIGHT)
        ->set("https://www.bilibili.com/", 5, QR_ECC_H)
        ->print();
}

// ---------------- 打印条形码 ----------------
void PrinterManager::testBarcode() {
    printer->bar_code()
        ->align(ALIGN_CENTER)
        ->set(BAR_UPC_A, "123456789005", 2, 100, ABOVE_AND_BELOW)
        ->print();
    printer->text()->newline();
    qDebug() << "printer " << printer;
}

// ---------------- 卸载 ----------------
void PrinterManager::deinitPrinter() {
    if (printer) {
        // printer->device()->usb()->deinit();
        qDebug() << "Printer deinitialized";
    }
}

// ---------------- 主线程逻辑 ----------------
void PrinterManager::run() {
    // if (!initPrinter())
    //     return;

    setupListeners();
    testSettings();
    // testText();
    testQRCode();
    // testText();
    qDebug() << "printer1111 " << printer;
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}