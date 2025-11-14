#ifndef CURVELOADER_H
#define CURVELOADER_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariantMap>
#include <QVector>

// 正确的 QtCharts 前置声明（避免 QChart 二义性）
namespace QtCharts {
class QChart;
class QLineSeries;
class QValueAxis;
}  // namespace QtCharts

class CurveLoader : public QObject {
    Q_OBJECT

public:
    explicit CurveLoader(QObject* parent = nullptr);

    // QML 调用此函数
    Q_INVOKABLE QVariantMap loadCurve(const QString& sampleNo, QObject* chartViewObj);

private:
    void initDb();
    QVector<double> readAdcData(const QString& sampleNo);

    // 正确使用 QtCharts::QChart
    void buildSeries(QtCharts::QChart* chart,
                     const QVector<double>& data,
                     QVariantMap& info);

private:
    QString connName_ = "curve_reader";
};

#endif  // CURVELOADER_H
