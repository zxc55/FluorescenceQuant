#ifndef QRSCANNER_H
#define QRSCANNER_H

#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>

#include "DecodeWorker.h"
#include "V4L2MjpegGrabber.h"

class QrScanner : public QObject {
    Q_OBJECT
public:
    explicit QrScanner(QObject* parent = nullptr);
    ~QrScanner();

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();

    QImage lastFrame() const;

signals:
    void frameUpdated();
    void qrDecoded(QString text);

private:
    V4L2MjpegGrabber m_grabber;
    DecodeWorker* m_worker;

    mutable QMutex m_mutex;
    QImage m_lastFrame;
};
#endif  // QRSCANNER_H
