#include "printerDeviceController.h"

#include <QDebug>

#include "HistoryRepo.h"
#include "SettingsViewModel.h"
PrinterDeviceController::PrinterDeviceController(SettingsViewModel* settings,
                                                 QObject* parent)
    : QObject(parent), m_settings(settings), redar1(PrinterManager::instance())  // ⬅️ 这里保持你原来的初始化方式
{
}

void PrinterDeviceController::start() {
    qDebug() << "[PrinterDeviceController] start()";
    redar1.start();
}

void PrinterDeviceController::stop() {
    qDebug() << "[PrinterDeviceController] stop()";
    redar1.stop();
}

void PrinterDeviceController::printText(const QString& text) {
    qDebug() << "[PrinterDeviceController] printText:" << text;
    QByteArray utf8 = text.toUtf8();
    //  redar1.testText();
    // ⭐ 使用队列投递打印任务（访问 printerManager 的内部 printer）
    redar1.postPrint([utf8]() {
        auto& mgr = PrinterManager::instance();
        if (!mgr.printer) {
            qWarning() << "[Printer] printer not initialized!";
            return;
        }

        // 使用已有的编码（例如 CP936，支持中文）
        mgr.printer->text()
            ->encoding(ENCODING_CP936)
            ->utf8_text((uint8_t*)utf8.data())
            ->print();
    });
}
void PrinterDeviceController::printRecord(const QVariantMap& record) {
    PrinterManager& mgr = PrinterManager::instance();
    auto* p = PrinterManager::instance().getPrinter();
    qDebug() << "PrinterManager::instance().getPrinter()" << p;
    if (!p) {
        qWarning() << "❌ Printer not initialized!";
        return;
    }

    QStringList lines;

    lines << "******** 检测结果 ********";
    lines << ("项目名称: " + record.value("projectName").toString());
    lines << ("样品编号: " + record.value("sampleNo").toString());
    if (m_settings && m_settings->printSampleSource()) {
        lines << ("样品来源: " + record.value("sampleSource").toString());
    }
    lines << ("样品名称: " + record.value("sampleName").toString());
    lines << ("标准曲线: " + record.value("standardCurve").toString());
    lines << ("批次编码: " + record.value("batchCode").toString());

    lines << QString("检测结果: %1")
                 .arg(record.value("result").toString());

    lines << QString("检测浓度: %1 %2 ug/kg")
                 .arg(record.value("detectedConc").toDouble(), 0, 'f', 2)
                 .arg(record.value("detectedUnit").toString());
    if (m_settings && m_settings->printReferenceValue()) {
        lines << QString("参考值: %1 %2")
                     .arg(record.value("referenceValue").toDouble(), 0, 'f', 2)
                     .arg(record.value("detectedUnit").toString());
    }
    lines << ("检测时间: " + record.value("detectedTime").toString());
    if (m_settings && m_settings->printDetectedPerson()) {
        lines << ("检测人员: " + record.value("detectedPerson").toString());
    }
    if (m_settings && m_settings->printDilutionInfo()) {
        lines << ("稀释倍数: " + record.value("dilutionInfo").toString());
    }
    if (m_settings && m_settings->manufacturerPrint()) {
        lines << ("青岛普瑞邦生物工程有限公司");
    }
    lines << "******************************";
    lines << "******************************";
    // ---- 严格按你的打印格式处理 ----
    for (int i = 0; i < lines.size(); i++) {
        QByteArray utf8 = lines[i].toUtf8();

        // ⭐ 必须生成 uint8_t 数组，否则无法传给 utf8_text()
        std::vector<uint8_t> buf(utf8.begin(), utf8.end());
        buf.push_back(0);  // null-terminator

        uint8_t* ptr = buf.data();

        if (i < lines.size() - 1) {
            p->text()->encoding(ENCODING_CP936)->utf8_text(ptr)->newline();
        } else {
            p->text()->encoding(ENCODING_CP936)->utf8_text(ptr)->print();
        }
    }
}
