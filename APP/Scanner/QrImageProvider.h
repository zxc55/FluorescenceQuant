#ifndef QRIMAGEPROVIDER_H
#define QRIMAGEPROVIDER_H

#include <QQuickImageProvider>

#include "QrScanner.h"

// 简单版：直接拿一个 QrScanner 指针
class QrImageProvider : public QQuickImageProvider {
public:
    // 构造时传入 scanner
    explicit QrImageProvider(QrScanner* scanner)
        : QQuickImageProvider(QQuickImageProvider::Image), m_scanner(scanner) {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        Q_UNUSED(id)
        Q_UNUSED(requestedSize)
        if (!m_scanner)
            return QImage();

        QImage img = m_scanner->lastFrame();
        if (size)
            *size = img.size();
        return img;
    }

private:
    QrScanner* m_scanner;
};

#endif  // QRIMAGEPROVIDER_H
