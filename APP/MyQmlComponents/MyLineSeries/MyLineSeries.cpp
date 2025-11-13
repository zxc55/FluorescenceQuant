#include "MyLineSeries.h"

#include "qdebug.h"
MyLineSeries::MyLineSeries(QObject* parent)
    : QLineSeries(parent) {
    setUseOpenGL(true);  // 开启 OpenGL 加速
}

// ===================== 自己维护点数据 ======================

void MyLineSeries::loadPoints(const QVariantList& list) {
    m_points.clear();
    m_points.reserve(list.size());
    int i = 0;

    for (const QVariant& v : list) {
        m_points.append(QPointF(i, v.toDouble()));
        // qDebug() << "-------MyLineSeries::loadPoints-------" << v.toDouble();

        i++;
    }
    qDebug() << "-------MyLineSeries::loadPoints-------" << list.size() << m_points.size();
    refresh();
}

void MyLineSeries::addPoint(double x, double y) {
    m_points.append(QPointF(x, y));
    refresh();
}

void MyLineSeries::clear() {
    m_points.clear();
    QLineSeries::clear();  // 清空基类的实际绘制数据
}

void MyLineSeries::refresh() {
    // replace() 会直接触发 QLineSeries 内部的 pointsReplaced()
    // ChartView 会自动重绘
    // for (const auto& v : m_points) {
    //     qDebug() << "-------MyLineSeries::loadPoints-------" << v.;
    // }
    replace(m_points);
}

// ===================== Axis 绑定（必须实现） ======================

void MyLineSeries::setAxisX(QAbstractAxis* axis) {
    if (m_axisX == axis)
        return;
    m_axisX = axis;
    QLineSeries::attachAxis(axis);
    emit axisXChanged(axis);
}

void MyLineSeries::setAxisY(QAbstractAxis* axis) {
    if (m_axisY == axis)
        return;
    m_axisY = axis;
    QLineSeries::attachAxis(axis);
    emit axisYChanged(axis);
}

void MyLineSeries::setAxisXTop(QAbstractAxis* axis) {
    if (m_axisXTop == axis)
        return;
    m_axisXTop = axis;
    QLineSeries::attachAxis(axis);
    emit axisXTopChanged(axis);
}

void MyLineSeries::setAxisYRight(QAbstractAxis* axis) {
    if (m_axisYRight == axis)
        return;
    m_axisYRight = axis;
    QLineSeries::attachAxis(axis);
    emit axisYRightChanged(axis);
}
