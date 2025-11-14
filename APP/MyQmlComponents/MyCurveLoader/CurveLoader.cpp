#include "CurveLoader.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

using namespace QtCharts;

CurveLoader::CurveLoader(QObject* parent)
    : QObject(parent) {
    initDb();
}

// ========================= 初始化数据库连接 =========================
void CurveLoader::initDb() {
    if (QSqlDatabase::contains(connName_))
        QSqlDatabase::removeDatabase(connName_);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName_);
    db.setDatabaseName("/mnt/SDCARD/app/db/app.db");

    if (!db.open()) {
        qWarning() << "❌ CurveLoader: 打开数据库失败:" << db.lastError().text();
    } else {
        qInfo() << "📘 CurveLoader DB OK";
    }
}

// ========================= 从数据库读取数据 =========================
QVector<double> CurveLoader::readAdcData(const QString& sampleNo) {
    QVector<double> result;

    QSqlDatabase db = QSqlDatabase::database(connName_);
    if (!db.isOpen()) {
        qWarning() << "CurveLoader: DB 未打开!";
        return result;
    }

    QSqlQuery q(db);
    q.prepare("SELECT adcValues FROM adc_data WHERE sampleNo=? ORDER BY id ASC");
    q.addBindValue(sampleNo);

    if (!q.exec()) {
        qWarning() << "❌ SQL 错误: " << q.lastError();
        return result;
    }

    while (q.next()) {
        QString json = q.value(0).toString();
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());

        if (!doc.isArray())
            continue;

        for (auto v : doc.array())
            result.append(v.toDouble());
    }

    return result;
}

// ========================= 构建曲线 =========================
void CurveLoader::buildSeries(QChart* chart, const QVector<double>& data, QVariantMap& info) {
    // 清除旧曲线
    for (auto s : chart->series())
        chart->removeSeries(s);

    // 创建新曲线
    QLineSeries* series = new QLineSeries(chart);
    series->setName("ADC 曲线");
    chart->addSeries(series);

    // 获取坐标轴（从 ChartView QML 创建）
    QValueAxis* axisX = nullptr;
    QValueAxis* axisY = nullptr;

    auto xs = chart->axes(Qt::Horizontal);
    auto ys = chart->axes(Qt::Vertical);

    if (!xs.isEmpty())
        axisX = qobject_cast<QValueAxis*>(xs.first());
    if (!ys.isEmpty())
        axisY = qobject_cast<QValueAxis*>(ys.first());

    if (!axisX || !axisY) {
        qWarning() << "❌ 坐标轴不存在，QML 未正确附加!";
        return;
    }

    double ymin = data.first();
    double ymax = data.first();

    for (int i = 0; i < data.size(); i++) {
        double v = data[i];
        series->append(i, v);

        if (v < ymin)
            ymin = v;
        if (v > ymax)
            ymax = v;
    }

    series->attachAxis(axisX);
    series->attachAxis(axisY);

    axisX->setRange(0, data.size());
    axisY->setRange(ymin, ymax);

    info["count"] = data.size();
    info["ymin"] = ymin;
    info["ymax"] = ymax;
}

// ========================= 关键：递归查找 QChart =========================
static QChart* findChart(QObject* obj) {
    QList<QObject*> stack;
    stack.append(obj);

    while (!stack.isEmpty()) {
        QObject* o = stack.takeFirst();

        // 尝试转换为 QChart
        QChart* chart = qobject_cast<QChart*>(o);
        if (chart)
            return chart;

        // 加入子对象继续查找
        for (QObject* c : o->children())
            stack.append(c);
    }
    return nullptr;
}

// ========================= 对外接口：QML 调用此函数 =========================
QVariantMap CurveLoader::loadCurve(const QString& sampleNo, QObject* chartViewObj) {
    QVariantMap info;

    if (!chartViewObj) {
        qWarning() << "❌ loadCurve: chartViewObj is NULL!";
        return info;
    }

    // ⭐⭐ Qt 5.12 专用：递归查找 QChart ⭐⭐
    QChart* chart = findChart(chartViewObj);

    if (!chart) {
        qWarning() << "❌ CurveLoader: 找不到 QChart!（QtCharts内部结构不同）";
        return info;
    }

    // 从 DB 读取数据
    QVector<double> data = readAdcData(sampleNo);

    if (data.isEmpty()) {
        qWarning() << "⚠️ CurveLoader: 数据为空!";
        return info;
    }

    // 构建曲线
    buildSeries(chart, data, info);

    return info;
}
