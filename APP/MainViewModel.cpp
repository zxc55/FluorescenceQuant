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
#define PEAK_NEIGHBOR_COUNT 5
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
static double avgPeak(const QVector<double>& y, int idx) {
    int n = y.size();
    double sum = 0;
    int count = 0;

    for (int k = -PEAK_NEIGHBOR_COUNT; k <= PEAK_NEIGHBOR_COUNT; ++k) {
        int p = idx + k;
        if (p >= 0 && p < n) {
            sum += y[p];
            count++;
        }
    }
    return (count > 0 ? sum / count : 0.0);
}
QVariantMap MainViewModel::calcTC(const QVariantList& adcList) {
    QVariantMap r;

    int n = adcList.size();
    if (n < 1500) {  // 数据太短：不可能出现 C/T 峰
        qWarning() << "[calcTC] curve too short, n=" << n;
        return r;
    }

    // -------------------------
    // 1) 转数组
    // -------------------------
    QVector<double> y(n);
    for (int i = 0; i < n; ++i)
        y[i] = adcList[i].toDouble();

    // ============================================================
    // 2) 找 C 峰：从 index >= 666 区间找最大值
    // ============================================================
    int idxC = -1;
    double maxC = -1;

    for (int i = 666; i < n; ++i) {
        if (y[i] > maxC) {
            maxC = y[i];
            idxC = i;
        }
    }

    if (idxC < 0) {
        qWarning() << "[calcTC] ERROR: cannot find C peak!";
        return r;
    }

    // ============================================================
    // 3) 找 T 峰：从 index >= 1250 区间找最大值
    // ============================================================
    int idxT = -1;
    double maxT = -1;

    for (int i = 1250; i < n; ++i) {
        if (y[i] > maxT) {
            maxT = y[i];
            idxT = i;
        }
    }

    if (idxT < 0) {
        qWarning() << "[calcTC] ERROR: cannot find T peak!";
        return r;
    }

    // ============================================================
    // 4) 峰值计算：7 点平均
    // ============================================================
    double C_raw = avgPeak(y, idxC);
    double T_raw = avgPeak(y, idxT);

    // ============================================================
    // 5) 初步 baseline 用于判断 T 是否存在
    //    baseline_est = C~T 区间最小值
    // ============================================================
    int a0 = qMin(idxC, idxT);
    int b0 = qMax(idxC, idxT);

    double baseline_est = y[a0];
    for (int i = a0; i <= b0; ++i)
        if (y[i] < baseline_est)
            baseline_est = y[i];

    // 初步判断 T 是否存在
    double T_net_est = T_raw - baseline_est;

    bool hasT = true;
    bool hasC = true;

    const double T_THRESHOLD = 0.01;  // 阴性判定阈值（你机器噪声 < 0.01）

    if (T_net_est < T_THRESHOLD)
        hasT = false;  // 阴性卡！

    // ============================================================
    // 6) 最终 baseline
    // ============================================================

    double baseline = 0;

    if (hasT) {
        // -----------------------
        // 阳性卡：用 C~T 区间最小值
        // -----------------------
        baseline = y[a0];
        for (int i = a0; i <= b0; ++i)
            if (y[i] < baseline)
                baseline = y[i];
    } else {
        // -----------------------
        // 阴性卡：只用背景区 baseline（更稳定）
        // 因为 T 不存在，无需 C~T baseline
        // -----------------------
        int L = 800, R = 950;
        if (L < 0)
            L = 0;
        if (R >= n)
            R = n - 1;

        baseline = y[L];
        for (int i = L; i <= R; ++i)
            if (y[i] < baseline)
                baseline = y[i];
    }

    // ============================================================
    // 7) 最终净峰值
    // ============================================================
    double C_net = C_raw - baseline;
    double T_net = hasT ? (T_raw - baseline) : 0;  // 阴性卡 T_net=0

    if (C_net < 0)
        C_net = 0;
    if (T_net < 0)
        T_net = 0;

    // ============================================================
    // 8) T/C 比值
    // ============================================================
    double ratio = 0;
    if (hasT && C_net > 0)
        ratio = T_net / C_net;
    else
        ratio = 0;  // 阴性卡 T/C=0

    // ============================================================
    // 9) 输出结果
    // ============================================================
    r["idxC"] = idxC;
    r["idxT"] = idxT;

    r["hasC"] = hasC;
    r["hasT"] = hasT;

    r["baseline"] = baseline;
    r["C_raw"] = C_raw;
    r["T_raw"] = T_raw;

    r["C_net"] = C_net;
    r["T_net"] = T_net;
    r["ratioTC"] = ratio;

    // debug 输出
    qInfo() << "[calcTC]"
            << "idxC=" << idxC
            << "idxT=" << idxT
            << "hasT=" << hasT
            << "baseline=" << baseline
            << "C_raw=" << C_raw
            << "T_raw=" << T_raw
            << "C_net=" << C_net
            << "T_net=" << T_net
            << "ratio=" << ratio;

    return r;
}
QVariantMap MainViewModel::calcTC_FixedWindow(const QVariantList& adcList) {
    QVariantMap result;
    result["hasT"] = false;
    result["hasC"] = false;
    result["areaT"] = 0.0;
    result["areaC"] = 0.0;
    result["ratioTC"] = 0.0;

    const int n = adcList.size();
    if (n < 100) {
        // 点太少，直接返回
        return result;
    }

    // 1) 把 QVariantList 转成 double 数组，方便运算
    QVector<double> y(n);
    for (int i = 0; i < n; ++i) {
        y[i] = adcList[i].toDouble();
    }

    // 2) 固定窗口索引（❗你需要根据自己的卡，自己微调这四个数字）
    //   假设峰位置大概是：
    //   T 峰：索引在 600~900
    //   C 峰：索引在 1500~1800
    //   你可以先用 qDebug 把峰位置打印一下，然后改成更合适的值
    const int T_START = 600;
    const int T_END = 900;
    const int C_START = 1500;
    const int C_END = 1800;

    // 防止越界，做一下裁剪
    const int tStart = qBound(0, T_START, n - 1);
    const int tEnd = qBound(0, T_END, n - 1);
    const int cStart = qBound(0, C_START, n - 1);
    const int cEnd = qBound(0, C_END, n - 1);

    if (tEnd <= tStart || cEnd <= cStart) {
        return result;  // 窗口非法
    }

    // 3) 计算每个窗口内的“局部基线”（取窗口内最小值）
    double baseT = y[tStart];
    for (int i = tStart + 1; i <= tEnd; ++i) {
        if (y[i] < baseT)
            baseT = y[i];
    }

    double baseC = y[cStart];
    for (int i = cStart + 1; i <= cEnd; ++i) {
        if (y[i] < baseC)
            baseC = y[i];
    }

    // 4) 在各自窗口内做简单积分：Σ max(0, y[i] - base)
    double areaT = 0.0;
    for (int i = tStart; i <= tEnd; ++i) {
        double v = y[i] - baseT;  // 去掉局部基线
        if (v > 0.0)
            areaT += v;  // 直接累加
    }

    double areaC = 0.0;
    for (int i = cStart; i <= cEnd; ++i) {
        double v = y[i] - baseC;
        if (v > 0.0)
            areaC += v;
    }

    // 5) 判断有没有峰（面积大于一个门限就认为存在）
    const double MIN_AREA = 0.3;  // 这个阈值你可以根据实际噪声调整

    bool hasT = (areaT > MIN_AREA);
    bool hasC = (areaC > MIN_AREA);

    double ratioTC = 0.0;
    if (hasT && hasC && areaC > 0.0) {
        ratioTC = areaT / areaC;  // 与别的仪器一样，用面积比值
    }

    // 6) 写回结果，给 QML 用
    result["hasT"] = hasT;
    result["hasC"] = hasC;
    result["areaT"] = areaT;
    result["areaC"] = areaC;
    result["ratioTC"] = ratioTC;

    return result;
}
