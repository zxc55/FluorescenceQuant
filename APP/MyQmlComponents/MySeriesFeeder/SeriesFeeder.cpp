#include "SeriesFeeder.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QtCharts/QXYSeries>
#include <QtMath>

using namespace QtCharts;

SeriesFeeder::SeriesFeeder(QObject* parent) : QObject(parent) {}

void SeriesFeeder::setSeriesObj(QObject* obj) {
    if (m_seriesObj == obj)
        return;
    m_seriesObj = obj;
    emit seriesChanged();
}

QXYSeries* SeriesFeeder::seriesPtr() const {
    return qobject_cast<QXYSeries*>(m_seriesObj);
}

void SeriesFeeder::clear() {
    if (auto* s = seriesPtr())
        s->clear();
    else
        emit error(QStringLiteral("SeriesFeeder: 'series' is not a QXYSeries."));
}

void SeriesFeeder::buildAndReplace(const QVariantList& data) {
    QXYSeries* s = seriesPtr();
    if (!s) {
        emit error(QStringLiteral("SeriesFeeder: 'series' is not a QXYSeries."));
        return;
    }

    QVector<QPointF> buf;
    buf.reserve(data.size());

    for (int i = 0; i < data.size(); ++i) {
        // const double y = std::sin(i * 0.01) + 0.1 * std::sin(i * 0.23);
        const double y = data[i].toDouble();
        buf.push_back(QPointF(i, y));
    }

    QElapsedTimer t;
    t.start();
    s->replace(buf);
    qDebug().noquote() << "FEEDER_REPLACE_MS:" << t.elapsed();
    emit finished();
}

// void SeriesFeeder::buildAndAppendChunked(int count, int chunk)
// {
//     QXYSeries* s = seriesPtr();
//     if (!s) { emit error(QStringLiteral("SeriesFeeder: 'series' is not a QXYSeries.")); return; }
//     if (chunk <= 0) chunk = count;

//     QVector<QPointF> buf;
//     buf.reserve(count);
//     for (int i=0;i<count;++i) {
//         const double y = std::sin(i*0.01) + 0.1*std::sin(i*0.23);
//         buf.push_back(QPointF(i, y));
//     }

//     s->clear();
//     int idx = 0;
//     int chunkIdx = 0;
//     auto pushChunk = [this, s, &buf, count, chunk, &idx, &chunkIdx]() mutable {
//         const int end = qMin(idx + chunk, count);
//         const int size = end - idx;
//         QElapsedTimer t; t.start();
//         s->append(buf.constData() + idx, size);
//         const int ms = int(t.elapsed());
//         emit chunkDone(++chunkIdx, size, ms);
//         idx = end;
//         if (idx >= count) {
//             emit finished();
//         } else {
//             // Yield to event loop
//             QMetaObject::invokeMethod(this, "buildAndAppendChunked", Qt::QueuedConnection,
//                                       Q_ARG(int, count), Q_ARG(int, chunk));
//         }
//     };

//     // Kick the first chunk synchronously
//     pushChunk();
// }
