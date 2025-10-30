#include "IIODeviceController.h"

#include <QDebug>

#include "IIOReaderThread.h"

IIODeviceController::IIODeviceController(QObject* parent)
    : QObject(parent) {
    reader_ = new IIOReaderThread(this);
    connect(reader_, &IIOReaderThread::newDataBatch,
            this, &IIODeviceController::newDataBatch);
}

IIODeviceController::~IIODeviceController() {
    stop();
}

void IIODeviceController::setDeviceIndex(int idx) { devIndex_ = idx; }
void IIODeviceController::setTargetHz(int hz) { targetHz_ = hz; }
void IIODeviceController::setWatermark(int n) { watermark_ = n; }
void IIODeviceController::setBufferLength(int n) { bufLen_ = n; }
void IIODeviceController::setTriggerName(const QString& name) { trigName_ = name; }

void IIODeviceController::start() {
    if (!reader_)
        return;
    reader_->setDeviceIndex(devIndex_);
    reader_->setTargetHz(targetHz_);
    reader_->setWatermark(watermark_);
    reader_->setBufferLength(bufLen_);
    if (!trigName_.isEmpty())
        reader_->setTriggerName(trigName_);

    if (!reader_->start()) {
        emit logMessage("❌ IIO buffer/trigger 启动失败，请检查 trigger 与权限");
    } else {
        emit logMessage(QStringLiteral("✅ IIO 启动: %1Hz, watermark=%2, buf=%3")
                            .arg(targetHz_)
                            .arg(watermark_)
                            .arg(bufLen_));
    }
}

void IIODeviceController::stop() {
    if (reader_)
        reader_->stop();
}
