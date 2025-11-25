#include "QrScanner.h"

#include <QDebug>

// QrScanner.cpp
QrScanner::QrScanner(QObject* parent)
    : QObject(parent) {
    m_worker = new DecodeWorker([this](const QString& text) {
        emit qrDecoded(text);
    });

    m_grabber.setFrameCallback([this](const QImage& img) {
        {
            QMutexLocker locker(&m_mutex);
            m_lastFrame = img;
        }
        emit frameUpdated();
        m_worker->enqueueFrame(img);
    });
}

QrScanner::~QrScanner() {
    stopScan();
    delete m_worker;
}

void QrScanner::startScan() {
    qDebug() << "[QrScanner] startScan";
    m_grabber.start("/dev/video0", 1280, 720, 10);
    m_worker->start();
}

void QrScanner::stopScan() {
    qDebug() << "[QrScanner] stopScan";
    m_grabber.stop();
    m_worker->stop();
}

QImage QrScanner::lastFrame() const {
    QMutexLocker locker(&m_mutex);
    return m_lastFrame;
}
