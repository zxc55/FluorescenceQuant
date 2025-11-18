#include "MainViewModel.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>  // std::sort
#include <chrono>
#include <cmath>  // std::abs
#include <thread>

#include "IIODeviceController.h"
class ScopedTimer {
public:
    explicit ScopedTimer(const QString& tag)
        : m_tag(tag),
          m_start(QDateTime::currentDateTime()) {
        qDebug().noquote() << QString("【%1】开始: %2")
                                  .arg(m_tag)
                                  .arg(m_start.toString("yyyy-MM-dd HH:mm:ss.zzz"));
    }

    ~ScopedTimer() {
        QDateTime end = QDateTime::currentDateTime();
        qint64 ms = m_start.msecsTo(end);
        qDebug().noquote() << QString("【%1】结束: %2（耗时 %3 ms）")
                                  .arg(m_tag)
                                  .arg(end.toString("yyyy-MM-dd HH:mm:ss.zzz"))
                                  .arg(ms);
    }

private:
    QString m_tag;
    QDateTime m_start;
};
MainViewModel::MainViewModel(QObject* parent)
    : QObject(parent) {
    deviceController = new IIODeviceController(this);

    connect(deviceController, &IIODeviceController::newDataBatch,
            this, &MainViewModel::onNewAdcData);
    connect(deviceController, &IIODeviceController::newDataBatch,
            this, &MainViewModel::newDataBatch);

    // 定时器（1秒唤醒一次写入线程）
    flushTimer_.setInterval(1000);
    connect(&flushTimer_, &QTimer::timeout, this, &MainViewModel::flushBufferToDb);

    // 启动独立线程用于数据库写入
    std::thread(&MainViewModel::dbWriterLoop, this).detach();
    // ⭐⭐ 新增：主线程用来查询曲线的数据库连接 ⭐⭐
    initReaderDb();
}
void MainViewModel::initReaderDb() {
    if (QSqlDatabase::contains(readerConnName_))
        QSqlDatabase::removeDatabase(readerConnName_);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", readerConnName_);
#ifndef LOCAL_BUILD
    db.setDatabaseName("/mnt/SDCARD/app/db/app.db");
#else
    db.setDatabaseName("/home/pribolab/Project/FluorescenceQuant/debugDir/app.db");
#endif
    if (!db.open()) {
        qWarning() << "❌ MainViewModel: reader DB open fail:" << db.lastError().text();
    } else {
        qInfo() << "📖 MainViewModel reader DB OK";
    }
}

MainViewModel::~MainViewModel() {
    flushTimer_.stop();
}

void MainViewModel::setCurrentSample(const QString& sampleNo) {
    currentSampleNo_ = sampleNo;
    qDebug() << "🔖 当前样品编号:" << currentSampleNo_;
}

void MainViewModel::startReading() {
    if (currentSampleNo_.isEmpty()) {
        qWarning() << "⚠️ 未设置样品编号";
        return;
    }
    buffer_.clear();
    flushTimer_.start();
    deviceController->start();
    qDebug() << "🧪 启动连续采集";
}

void MainViewModel::stopReading() {
    flushTimer_.stop();
    flushBufferToDb();  // 手动刷新
    deviceController->stop();
    qDebug() << "⏹ 停止采集";
}

// 收到一批采样数据 → 存入内存缓冲
void MainViewModel::onNewAdcData(const QVector<double>& values) {
    buffer_ += values;
}

// 定时触发 → 将内存缓冲放入写入队列
void MainViewModel::flushBufferToDb() {
    if (buffer_.isEmpty())
        return;

    std::lock_guard<std::mutex> lock(queueMutex_);
    writeQueue_.enqueue(buffer_);
    buffer_.clear();
}
// === 独立线程执行数据库写入 ===
void MainViewModel::dbWriterLoop() {
    // 每个线程必须单独打开自己的数据库连接
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "writer");
#ifndef LOCAL_BUILD
    db.setDatabaseName("/mnt/SDCARD/app/db/app.db");
#else
    db.setDatabaseName("/home/pribolab/Project/FluorescenceQuant/debugDir/app.db");
#endif
    if (!db.open()) {
        qWarning() << "❌ 数据库线程打开失败:" << db.lastError().text();
        return;
    }

    qInfo() << "💾 数据库写入线程启动";

    while (true) {
        QVector<double> batch;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (!writeQueue_.isEmpty())
                batch = writeQueue_.dequeue();
        }

        if (!batch.isEmpty()) {
            // 转 JSON
            QJsonArray arr;
            double sum = 0;
            for (double v : batch) {
                arr.append(v);
                sum += v;
            }
            double avg = batch.isEmpty() ? 0 : sum / batch.size();
            QString jsonStr = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
            QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

            // ✅ 注意字段名 adcValues（不是 values）
            QSqlQuery q(db);
            q.prepare("INSERT INTO adc_data (sampleNo, timestamp, adcValues, avgValue) VALUES (?, ?, ?, ?)");
            q.addBindValue(currentSampleNo_);
            q.addBindValue(ts);
            q.addBindValue(jsonStr);
            q.addBindValue(avg);

            if (!q.exec()) {
                qWarning() << "❌ 数据库写入失败:" << q.lastError().text()
                           << " SQL:" << q.lastQuery();
            } else {
                qDebug() << "💾 数据库写入成功 点数=" << batch.size();
            }
        }

        // 防止CPU满载
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
QVariantList MainViewModel::getAdcDataBySample(const QString& sampleNo) {
    QVariantList result;

    if (sampleNo.isEmpty()) {
        qWarning() << "getAdcDataBySample: sampleNo empty!";
        return result;
    }

    // ⭐ 使用 reader 连接 ⭐
    QSqlDatabase db = QSqlDatabase::database(readerConnName_);
    if (!db.isOpen()) {
        qWarning() << "getAdcDataBySample: DB not open!";
        return result;
    }
    QSqlQuery q(db);
    {
        ScopedTimer ti("ADC数据查询");

        q.prepare("SELECT adcValues FROM adc_data WHERE sampleNo=? ORDER BY id ASC;");
        q.addBindValue(sampleNo);
        if (!q.exec()) {
            qWarning() << "getAdcDataBySample SQL fail:" << q.lastError();
            return result;
        }
    }

    while (q.next()) {
        QString json = q.value(0).toString();
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        if (!doc.isArray())
            continue;

        QJsonArray arr = doc.array();
        for (auto v : arr) result.append(v.toDouble());
    }

    qDebug() << "📊 加载点数 = " << result.size();
    return result;
}

// === QML 调用：根据 sampleNo 查询曲线数据 ===
QVariantList MainViewModel::getAdcData(const QString& sampleNo) {
    QVariantList result;

    if (sampleNo.isEmpty()) {
        qWarning() << "[MainViewModel] getAdcData: sampleNo empty";
        return result;
    }

    QSqlDatabase db = QSqlDatabase::database(readerConnName_);
    if (!db.isOpen()) {
        qWarning() << "[MainViewModel] getAdcData: DB not open";
        return result;
    }

    QSqlQuery q(db);
    {
        ScopedTimer ti("ADC数据查询");
        q.prepare("SELECT adcValues FROM adc_data WHERE sampleNo=? ORDER BY id ASC");
        q.addBindValue(sampleNo);
        if (!q.exec()) {
            qWarning() << "[MainViewModel] getAdcData SQL error:" << q.lastError().text();
            return result;
        }
    }
    {
        ScopedTimer ti("ADC格式转换");
        while (q.next()) {
            QString json = q.value(0).toString();
            QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
            if (!doc.isArray())
                continue;

            QJsonArray arr = doc.array();
            for (auto v : arr) result.append(v.toDouble());
        }
    }

    qInfo() << "[MainViewModel] 曲线点数=" << result.size();
    return result;
}
QVariantMap MainViewModel::calcTC(const QVariantList& adcList) {
    QVariantMap r;

    r["hasT"] = false;
    r["hasC"] = false;
    r["areaT"] = 0.0;
    r["areaC"] = 0.0;
    r["ratioTC"] = 0.0;
    r["valid"] = false;

    const int n = adcList.size();
    if (n < 200)
        return r;

    // ===== 1. 转成 double =====
    QVector<double> y(n);
    for (int i = 0; i < n; ++i)
        y[i] = adcList[i].toDouble();

    // ===== 2. 基线（局部最小值滤波）=====
    const int win = 20;
    QVector<double> baseline(n);

    for (int i = 0; i < n; ++i) {
        int L = qMax(0, i - win);
        int R = qMin(n - 1, i + win);
        double mn = y[L];
        for (int j = L; j <= R; ++j)
            mn = qMin(mn, y[j]);
        baseline[i] = mn;
    }

    // ===== 3. 去基线 corr =====
    QVector<double> corr(n);
    double maxCorr = 0;

    for (int i = 0; i < n; ++i) {
        double v = y[i] - baseline[i];
        if (v < 0)
            v = 0;
        corr[i] = v;
        if (v > maxCorr)
            maxCorr = v;
    }

    if (maxCorr < 1e-6)
        return r;

    // ===== 4. 自动找峰（局部最大）=====
    struct Peak {
        int idx;
        double h;
    };
    QVector<Peak> peaks;

    double thresh = maxCorr * 0.20;  // 过滤很小的噪声峰

    for (int i = 1; i < n - 1; ++i) {
        if (corr[i] > thresh &&
            corr[i] >= corr[i - 1] &&
            corr[i] >= corr[i + 1]) {
            peaks.append({i, corr[i]});
        }
    }

    if (peaks.isEmpty())
        return r;

    // 排序（高度从大到小）
    std::sort(peaks.begin(), peaks.end(),
              [](const Peak& a, const Peak& b) { return a.h > b.h; });

    // ===== 5. 至少间隔 50 点，取 2 个峰 =====
    QVector<Peak> sel;
    const int minDist = 50;

    for (auto& p : peaks) {
        bool ok = true;
        for (auto& s : sel) {
            if (qAbs(p.idx - s.idx) < minDist)
                ok = false;
        }
        if (ok)
            sel.append(p);
        if (sel.size() >= 2)
            break;
    }

    // 按位置排序（左=T，右=C）
    std::sort(sel.begin(), sel.end(),
              [](const Peak& a, const Peak& b) { return a.idx < b.idx; });

    // ===== 6. 峰面积（10%高度阈值，全峰积分）=====
    auto integratePeak = [&](int center) -> double {
        double h10 = corr[center] * 0.10;  // 10% 阈值

        int L = center;
        while (L > 0 && corr[L] > h10)
            L--;

        int R = center;
        while (R < n - 1 && corr[R] > h10)
            R++;

        double area = 0.0;
        for (int i = L + 1; i <= R; ++i)
            area += 0.5 * (corr[i] + corr[i - 1]);

        return area;
    };

    double areaT = 0, areaC = 0;
    bool hasT = false, hasC = false;

    // ===== 7. 单峰 or 双峰 =====
    if (sel.size() == 1) {
        int idx = sel[0].idx;
        if (idx > n * 0.45) {  // 靠右 → C 线
            hasC = true;
            areaC = integratePeak(idx);
        } else {
            hasT = true;
            areaT = integratePeak(idx);
        }
    } else if (sel.size() >= 2) {
        hasT = true;
        hasC = true;
        areaT = integratePeak(sel[0].idx);  // 左峰 → T
        areaC = integratePeak(sel[1].idx);  // 右峰 → C
    }

    // ===== 8. C 线不存在 → 无效卡 =====
    if (!hasC) {
        r["valid"] = false;
        return r;
    }
    r["valid"] = true;

    // ===== 9. 只有 C → 阴性卡 =====
    if (hasC && !hasT) {
        r["hasC"] = true;
        r["areaC"] = areaC;
        r["ratioTC"] = 0;
        return r;
    }

    // ===== 10. T+C 正常卡 =====
    r["hasT"] = hasT;
    r["hasC"] = hasC;
    r["areaT"] = areaT;
    r["areaC"] = areaC;
    r["ratioTC"] = (areaC > 0 ? areaT / areaC : 0.0);

    return r;
}
