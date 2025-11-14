#ifndef SERIESFEEDER_H_
#define SERIESFEEDER_H_

#include <QObject>
#include <QPointF>
#include <QVector>

namespace QtCharts {
class QXYSeries;
}

class SeriesFeeder : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* series READ seriesObj WRITE setSeriesObj NOTIFY seriesChanged)
public:
    explicit SeriesFeeder(QObject* parent = nullptr);

    QObject* seriesObj() const { return m_seriesObj; }
    void setSeriesObj(QObject* obj);

    Q_INVOKABLE void clear();
    Q_INVOKABLE void buildAndReplace(const QVariantList& data);  // 一次性生成并 replace
                                                                 //  Q_INVOKABLE void buildAndAppendChunked(int count, int chunk);  // 分批 append

signals:
    void seriesChanged();
    void chunkDone(int index, int size, int elapsedMs);
    void finished();  // 所有数据完成
    void error(QString message);

private:
    QtCharts::QXYSeries* seriesPtr() const;
    QObject* m_seriesObj = nullptr;
};
#endif  // SERIESFEEDER_H_