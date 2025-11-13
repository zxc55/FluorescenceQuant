#ifndef MYLINESERIES_H
#define MYLINESERIES_H

#include <QVector>
#include <QtCharts/QAbstractAxis>
#include <QtCharts/QLineSeries>

QT_CHARTS_USE_NAMESPACE

class MyLineSeries : public QLineSeries {
    Q_OBJECT

    // === 为 QML 提供 axis 属性（必须，否则 QML 会报 axisXChanged 找不到）===
    Q_PROPERTY(QAbstractAxis* axisX READ axisX WRITE setAxisX NOTIFY axisXChanged)
    Q_PROPERTY(QAbstractAxis* axisY READ axisY WRITE setAxisY NOTIFY axisYChanged)
    Q_PROPERTY(QAbstractAxis* axisXTop READ axisXTop WRITE setAxisXTop NOTIFY axisXTopChanged)
    Q_PROPERTY(QAbstractAxis* axisYRight READ axisYRight WRITE setAxisYRight NOTIFY axisYRightChanged)

public:
    explicit MyLineSeries(QObject* parent = nullptr);

    // === 点数据存储（不使用 QLineSeries 自身 append）===
    QVector<QPointF> m_points;

    // === 批量加载点 [[x,y],[x,y],...] ===
    Q_INVOKABLE void loadPoints(const QVariantList& list);

    // === 追加单点 ===
    Q_INVOKABLE void addPoint(double x, double y);

    // === 清空曲线 ===
    Q_INVOKABLE void clear();

    // === 手动刷新到图表（内部调用 replace）===
    Q_INVOKABLE void refresh();

    // === Axis Getters ===
    QAbstractAxis* axisX() const { return m_axisX; }
    QAbstractAxis* axisY() const { return m_axisY; }
    QAbstractAxis* axisXTop() const { return m_axisXTop; }
    QAbstractAxis* axisYRight() const { return m_axisYRight; }

public slots:
    // === Axis Setters ===
    void setAxisX(QAbstractAxis* axis);
    void setAxisY(QAbstractAxis* axis);
    void setAxisXTop(QAbstractAxis* axis);
    void setAxisYRight(QAbstractAxis* axis);

signals:
    void axisXChanged(QAbstractAxis*);
    void axisYChanged(QAbstractAxis*);
    void axisXTopChanged(QAbstractAxis*);
    void axisYRightChanged(QAbstractAxis*);

private:
    QAbstractAxis* m_axisX = nullptr;
    QAbstractAxis* m_axisY = nullptr;
    QAbstractAxis* m_axisXTop = nullptr;
    QAbstractAxis* m_axisYRight = nullptr;
};

#endif  // MYLINESERIES_H
