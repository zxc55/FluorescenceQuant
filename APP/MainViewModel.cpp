#include "MainViewModel.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVector>    // QVector 注释
#include <QtGlobal>   // qBound/qSwap/qAbs 注释
#include <algorithm>  // std::sort
#include <algorithm>  // std::min/std::max/std::sort 注释
#include <chrono>
#include <cmath>  // std::abs
#include <thread>

#include "IIODeviceController.h"
#include "curve/cubicSpline_rida.h"
#include "curve/logit_rida.h"
#include "curve/piecewiseLinear_rida.h"

// =========================
// 峰候选结构体
// =========================
struct PeakCand {  // 峰候选结构体注释
    int idx;       // 峰索引注释
    double y;      // 峰值注释
    double prom;   // 峰显著性 prominence 注释
};  // 结束注释
#define PEAK_NEIGHBOR 3
MainViewModel::FourPLParams stdCurve;
void MainViewModel::setMethodConfigVm(QrMethodConfigViewModel* vm) {
    m_methodVm = vm;
    qInfo() << "[MainVM] methodVm set =" << vm;
}
static double fourPL_inverse(double y, const MainViewModel::FourPLParams& p) {
    const double eps = 1e-9;

    // 防御式检查（双保险）
    if (p.C <= 0.0 || std::fabs(p.B) < eps)
        return 0.0;

    const double A = p.A;
    const double B = p.B;
    const double C = p.C;
    const double D = p.D;
    qDebug() << "FourPL: A=" << A << " B=" << B << " C=" << C << " D=" << D;
    // clamp y 到曲线有效区间
    double y_min = std::min(A, D) + eps;
    double y_max = std::max(A, D) - eps;

    if (y < y_min)
        y = y_min;
    if (y > y_max)
        y = y_max;

    double denom = (y - D);
    if (std::fabs(denom) < eps)
        return 1e9;  // 接近下平台，浓度趋于无穷

    double ratio = (A - D) / denom - 1.0;
    if (ratio <= 0.0)
        return 0.0;

    double t = std::pow(ratio, 1.0 / B);
    double x = C * t;

    return (x < 0.0) ? 0.0 : x;
}

static QString formatLimitValue(double value) {
    if (std::fabs(value - std::round(value)) < 1e-9)
        return QString::number(static_cast<qint64>(std::llround(value)));
    return QString::number(value, 'f', 3);
}

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
// QString MainViewModel::generateSampleNo() {
//     QString result;

//     // 1) 获取今天日期
//     QString today = QDate::currentDate().toString("yyyyMMdd");

//     // 2) 打开数据库（按照你给的 getAdcData 的方式）
//     QSqlDatabase db = QSqlDatabase::database(readerConnName_);
//     if (!db.isOpen()) {
//         qWarning() << "[MainViewModel] generateSampleNo: DB not open";
//         return today + "0001";
//     }

//     // 3) 读取 last_sample_date 和 last_sample_index
//     QSqlQuery q(db);
//     q.prepare("SELECT last_sample_date, last_sample_index FROM app_settings WHERE id=1");

//     if (!q.exec() || !q.next()) {
//         qWarning() << "[MainViewModel] generateSampleNo: SELECT fail";
//         return today + "0001";
//     }

//     QString lastDate = q.value(0).toString();
//     int lastIndex = q.value(1).toInt();

//     // 4) 判断是否跨天
//     int newIndex = 0;
//     if (lastDate == today) {
//         newIndex = lastIndex + 1;
//     } else {
//         newIndex = 1;
//     }

//     // 5) 写回数据库
//     QSqlQuery q2(db);
//     q2.prepare("UPDATE app_settings SET last_sample_date=?, last_sample_index=? WHERE id=1");
//     q2.addBindValue(today);
//     q2.addBindValue(newIndex);
//     if (!q2.exec()) {
//         qWarning() << "[MainViewModel] generateSampleNo: UPDATE fail";
//     }

//     // 6) 生成最终编号 YYYYMMDD + 四位序号
//     result = today + QString("%1").arg(newIndex, 4, 10, QChar('0'));

//     qInfo() << "[MainViewModel] AutoSampleNo =" << result;
//     return result;
// }
QString MainViewModel::generateSampleNo() {
    QString today = QDate::currentDate().toString("yyyyMMdd");
    QSqlDatabase db = QSqlDatabase::database(readerConnName_);
    if (!db.isOpen())
        return "";

    QSqlQuery q(db);
    if (!q.exec("BEGIN IMMEDIATE TRANSACTION"))
        return "";

    auto fail = [&db]() -> QString {
        QSqlQuery rb(db);
        rb.exec("ROLLBACK");
        return "";
    };

    // 保证 id=1 行存在
    if (!q.exec("INSERT OR IGNORE INTO app_settings(id,last_sample_date,last_sample_index) VALUES(1,'',0)"))
        return fail();

    QSqlQuery r(db);
    r.prepare("SELECT last_sample_date,last_sample_index FROM app_settings WHERE id=1");
    if (!r.exec() || !r.next())
        return fail();

    QString lastDate = r.value(0).toString();
    int lastIndex = r.value(1).toInt();
    int newIndex = (lastDate == today) ? (lastIndex + 1) : 1;

    QSqlQuery u(db);
    u.prepare("UPDATE app_settings SET last_sample_date=?, last_sample_index=? WHERE id=1");
    u.addBindValue(today);
    u.addBindValue(newIndex);
    if (!u.exec() || u.numRowsAffected() != 1)
        return fail();

    QSqlQuery c(db);
    if (!c.exec("COMMIT"))
        return fail();

    return today + QString("%1").arg(newIndex, 4, 10, QChar('0'));
}

static double avgPeak(const QVector<double>& y, int idx) {
    int n = y.size();
    double sum = 0;
    int cnt = 0;
    for (int k = -PEAK_NEIGHBOR; k <= PEAK_NEIGHBOR; k++) {
        int p = idx + k;
        if (p >= 0 && p < n) {
            sum += y[p];
            cnt++;
        }
    }
    return cnt ? sum / cnt : 0;
}
static bool parseFourPLFromJson(const QString& json,
                                MainViewModel::FourPLParams& out) {
    // 1. 空字符串保护
    if (json.trimmed().isEmpty()) {
        qWarning() << "[FourPL] empty methodData";
        return false;
    }

    // 2. JSON 解析
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[FourPL] json parse failed:" << err.errorString();
        return false;
    }

    QJsonObject o = doc.object();

    // 3. 取参数（缺失则为 0）
    out.A = o.value("A").toDouble(0.0);
    out.B = o.value("B").toDouble(0.0);
    out.C = o.value("C").toDouble(0.0);
    out.D = o.value("D").toDouble(0.0);
    out.C1 = o.value("C1").toInt(0);
    out.C2 = o.value("C2").toInt(0);
    out.T1 = o.value("T1").toInt(0);
    out.T2 = o.value("T2").toInt(0);
    // 4. 基本合法性校验（工程必须）
    if (out.C <= 0.0 || std::fabs(out.B) < 1e-9) {
        qDebug() << "[FourPL] invalid params:"
                 << "A=" << out.A
                 << "B=" << out.B
                 << "C=" << out.C
                 << "D=" << out.D;
        return false;
    }
    return true;
}

static std::vector<double> jsonArrayToVector(const QJsonValue& value) {
    std::vector<double> out;
    if (!value.isArray()) {
        return out;
    }

    const QJsonArray array = value.toArray();
    out.reserve(array.size());
    for (const QJsonValue& item : array) {
        out.push_back(item.toDouble());
    }
    return out;
}

static bool parseMethodJson(const QString& json, QJsonObject& out) {
    if (json.trimmed().isEmpty()) {
        qWarning() << "[Curve] empty methodData";
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[Curve] json parse failed:" << err.errorString();
        return false;
    }

    out = doc.object();
    return true;
}

static bool parseCubicSplineParam(const QJsonObject& o,
                                  RIDA_ALGORITHM::CubicSplineCurveParam& out) {
    out.n = o.value("n").toInt(0);
    out.scale = o.value("scale").toDouble(100000.0);
    out.k = jsonArrayToVector(o.value("k"));
    out.a = jsonArrayToVector(o.value("a"));
    out.b = jsonArrayToVector(o.value("b"));
    out.c = jsonArrayToVector(o.value("c"));
    out.d = jsonArrayToVector(o.value("d"));
    out.xMin = o.value("xMin").toDouble(0.0);
    out.xMax = o.value("xMax").toDouble(0.0);
    return true;
}

static bool parsePiecewiseLinearParam(const QJsonObject& o,
                                      RIDA_ALGORITHM::PiecewiseLinear::Param& out) {
    out.n = o.value("n").toInt(0);
    out.scale = o.value("scale").toDouble(100000.0);
    out.k = jsonArrayToVector(o.value("k"));
    out.y = jsonArrayToVector(o.value("y"));
    out.slope = jsonArrayToVector(o.value("slope"));
    out.xMin = o.value("xMin").toDouble(0.0);
    out.xMax = o.value("xMax").toDouble(0.0);
    return true;
}

static bool calculateConcentrationByCurveType(const QString& methodData,
                                              double ratio,
                                              double& concentration,
                                              int& curveType) {
    QJsonObject o;
    if (!parseMethodJson(methodData, o)) {
        return false;
    }

    curveType = o.value("type").toInt(0);

    switch (curveType) {
    case 0: {
        MainViewModel::FourPLParams curve;
        if (!parseFourPLFromJson(methodData, curve)) {
            qWarning() << "[Curve] invalid FourPL params";
            return false;
        }
        concentration = fourPL_inverse(ratio, curve);
        return true;
    }
    case 1: {
        RIDA_ALGORITHM::CubicSplineCurveParam param;
        parseCubicSplineParam(o, param);

        RIDA_ALGORITHM::CubicSpline curve;
        const int importRet = curve.importCurveParam(param);
        if (importRet != 0) {
            qWarning() << "[Curve] CubicSpline import failed, ret =" << importRet;
            return false;
        }

        const int ret = curve.getX(ratio, concentration);
        if (ret == 79) {
            qWarning() << "[Curve] CubicSpline getX failed, ret =" << ret;
            return false;
        }
        return true;
    }
    case 2: {
        RIDA_ALGORITHM::Logit::Parameter param;
        param.k = o.value("k").toDouble(0.0);
        param.b = o.value("b").toDouble(0.0);

        RIDA_ALGORITHM::Logit curve;
        const int importRet = curve.importCurveParam(param);
        if (importRet != 0) {
            qWarning() << "[Curve] Logit import failed, ret =" << importRet;
            return false;
        }

        const int ret = curve.getX(ratio, concentration);
        if (ret == 79) {
            qWarning() << "[Curve] Logit getX failed, ret =" << ret;
            return false;
        }
        return true;
    }
    case 3: {
        RIDA_ALGORITHM::PiecewiseLinear::Param param;
        parsePiecewiseLinearParam(o, param);

        RIDA_ALGORITHM::PiecewiseLinear curve;
        const int importRet = curve.importCurveParam(param);
        if (importRet != 0) {
            qWarning() << "[Curve] PiecewiseLinear import failed, ret =" << importRet;
            return false;
        }

        const int ret = curve.getX(ratio, concentration);
        if (ret == 79) {
            qWarning() << "[Curve] PiecewiseLinear getX failed, ret =" << ret;
            return false;
        }
        return true;
    }
    default:
        qWarning() << "[Curve] unsupported curve type =" << curveType;
        return false;
    }
}

// =========================
// 计算区间 [L,R] 的前缀最小和后缀最小，用于快速算 prominence
// leftMin[k]  = min(y[L..L+k])
// rightMin[k] = min(y[L+k..R])
// =========================
static void buildMinArrays(const QVector<double>& y, int L, int R,
                           QVector<double>& leftMin, QVector<double>& rightMin)  // 构建最小值数组注释
{
    const int m = R - L + 1;  // 区间长度注释
    leftMin.resize(m);        // 调整 leftMin 大小注释
    rightMin.resize(m);       // 调整 rightMin 大小注释

    leftMin[0] = y[L];                                    // 初始化前缀最小值注释
    for (int k = 1; k < m; ++k) {                         // 遍历区间注释
        leftMin[k] = std::min(leftMin[k - 1], y[L + k]);  // 前缀最小值递推注释
    }  // 结束循环注释

    rightMin[m - 1] = y[R];                                 // 初始化后缀最小值注释
    for (int k = m - 2; k >= 0; --k) {                      // 逆向遍历注释
        rightMin[k] = std::min(rightMin[k + 1], y[L + k]);  // 后缀最小值递推注释
    }  // 结束循环注释
}  // 结束注释

// =========================
// 在 [L,R] 内枚举局部峰（含平台峰），并算 prominence
// prominence = y[peak] - max(左侧最小值, 右侧最小值)
// =========================
static void collectPeaksSoftware(const QVector<double>& y, int L, int R,
                                 double minProminence, QVector<PeakCand>& out)  // 收集峰候选注释
{
    out.clear();             // 清空输出注释
    const int n = y.size();  // 数据长度注释
    if (n <= 0)
        return;  // 空数据保护注释

    L = qBound(0, L, n - 1);  // 夹紧 L 注释
    R = qBound(0, R, n - 1);  // 夹紧 R 注释
    if (L > R)
        qSwap(L, R);  // 保证 L<=R 注释
    if (R - L < 2)
        return;  // 太短无法做局部峰注释

    QVector<double> leftMin;                     // 前缀最小数组注释
    QVector<double> rightMin;                    // 后缀最小数组注释
    buildMinArrays(y, L, R, leftMin, rightMin);  // 构建最小值数组注释

    int i = L + 1;                   // 从 L+1 开始扫描注释
    while (i <= R - 1) {             // 扫描到 R-1 注释
        const double yl = y[i - 1];  // 左邻点注释
        const double yc = y[i];      // 当前点注释
        const double yr = y[i + 1];  // 右邻点注释

        // ---- 平台峰处理：检测到“上升后进入平台” ----
        if (yc > yl && yc == yr) {         // 上升并进入平台注释
            int j = i + 1;                 // 平台右边界游标注释
            while (j < R && y[j] == yc) {  // 扩展平台直到变化或到 R-1 注释
                ++j;                       // 继续扩展注释
            }  // 结束 while 注释

            // j 现在是平台结束后的第一个点索引（可能等于 R）注释
            // 需要保证平台后面确实下降才算平台峰注释
            if (j <= R && y[j] < yc) {                                    // 平台后下降注释
                const int peakIdx = (i + (j - 1)) / 2;                    // 取平台中心点作为峰注释
                const int k = peakIdx - L;                                // 映射到最小值数组索引注释
                const double valley = std::max(leftMin[k], rightMin[k]);  // 两侧较高谷底注释
                const double prom = y[peakIdx] - valley;                  // 显著性注释
                if (prom >= minProminence) {                              // 显著性过滤注释
                    PeakCand c;                                           // 候选峰注释
                    c.idx = peakIdx;                                      // 记录索引注释
                    c.y = y[peakIdx];                                     // 记录峰值注释
                    c.prom = prom;                                        // 记录显著性注释
                    out.push_back(c);                                     // 加入列表注释
                }  // 结束注释
            }  // 结束注释

            i = j;     // 跳过整段平台，继续扫描注释
            continue;  // 继续下一轮注释
        }  // 结束平台峰分支注释

        // ---- 普通局部峰（包含“尖峰/单点峰”）----
        const bool isPeak = ((yc >= yl) && (yc > yr)) || ((yc > yl) && (yc >= yr));  // 局部峰条件注释
        if (isPeak) {                                                                // 如果是局部峰注释
            const int k = i - L;                                                     // 映射到最小值数组索引注释
            const double valley = std::max(leftMin[k], rightMin[k]);                 // 两侧较高谷底注释
            const double prom = yc - valley;                                         // 显著性注释
            if (prom >= minProminence) {                                             // 显著性过滤注释
                PeakCand c;                                                          // 候选峰注释
                c.idx = i;                                                           // 记录索引注释
                c.y = yc;                                                            // 记录峰值注释
                c.prom = prom;                                                       // 记录显著性注释
                out.push_back(c);                                                    // 加入列表注释
            }  // 结束注释
        }  // 结束注释

        ++i;  // i 前进注释
    }  // 结束 while 注释
}  // 结束注释

// =========================
// 从候选峰里选出两个“主峰”
// 规则：prominence 从大到小选，且两峰间距 >= minSepSamples
// 若候选不足：回退到“全区间最大值 + 去掉附近再找一次最大值”
// =========================
static bool findTwoMainPeaksSoftware(const QVector<double>& y, int L, int R,
                                     double minProminence, int minSepSamples,
                                     int& outIdx1, int& outIdx2)  // 找两主峰注释
{
    outIdx1 = -1;  // 初始化输出注释
    outIdx2 = -1;  // 初始化输出注释

    const int n = y.size();  // 数据长度注释
    if (n <= 0)
        return false;  // 空数据保护注释

    L = qBound(0, L, n - 1);  // 夹紧 L 注释
    R = qBound(0, R, n - 1);  // 夹紧 R 注释
    if (L > R)
        qSwap(L, R);  // 保证 L<=R 注释
    if (R - L < 2)
        return false;  // 太短无法找两峰注释

    QVector<PeakCand> cands;                              // 候选峰列表注释
    collectPeaksSoftware(y, L, R, minProminence, cands);  // 收集候选峰注释

    // ---- 按 prominence 降序排序 ----
    std::sort(cands.begin(), cands.end(),                 // 排序注释
              [](const PeakCand& a, const PeakCand& b) {  // 比较函数注释
                  if (a.prom != b.prom)
                      return a.prom > b.prom;  // 先比显著性注释
                  return a.y > b.y;            // 再比峰高注释
              });                              // 结束注释

    // ---- 选两个相隔足够远的峰 ----
    for (int i = 0; i < cands.size(); ++i) {  // 遍历候选注释
        const int p1 = cands[i].idx;          // 第一个峰候选索引注释
        if (outIdx1 < 0) {                    // 还没选第一个峰注释
            outIdx1 = p1;                     // 选中第一个峰注释
            continue;                         // 继续选第二个峰注释
        }  // 结束注释
        if (qAbs(p1 - outIdx1) >= minSepSamples) {  // 与第一个峰距离足够注释
            outIdx2 = p1;                           // 选中第二个峰注释
            break;                                  // 退出注释
        }  // 结束注释
    }  // 结束循环注释

    if (outIdx1 >= 0 && outIdx2 >= 0) {  // 正常找到两峰注释
        return true;                     // 返回成功注释
    }  // 结束注释

    // =========================
    // 回退策略：最大值法找两峰（不依赖局部峰）
    // 先找全局最大值，再把它附近 minSepSamples 区域“屏蔽”，再找第二大
    // =========================
    int idxA = L;                   // 第一峰索引注释
    double maxA = y[L];             // 第一峰值注释
    for (int i = L; i <= R; ++i) {  // 扫描区间注释
        if (y[i] > maxA) {          // 找更大值注释
            maxA = y[i];            // 更新注释
            idxA = i;               // 更新注释
        }  // 结束注释
    }  // 结束循环注释

    const int banL = qBound(L, idxA - minSepSamples, R);  // 屏蔽左边界注释
    const int banR = qBound(L, idxA + minSepSamples, R);  // 屏蔽右边界注释

    int idxB = -1;         // 第二峰索引注释
    double maxB = -1e300;  // 第二峰值注释

    for (int i = L; i <= R; ++i) {  // 再扫一遍注释
        if (i >= banL && i <= banR)
            continue;       // 屏蔽区域跳过注释
        if (y[i] > maxB) {  // 找最大注释
            maxB = y[i];    // 更新注释
            idxB = i;       // 更新注释
        }  // 结束注释
    }  // 结束循环注释

    if (idxB < 0)
        return false;  // 仍找不到第二峰注释

    outIdx1 = idxA;  // 输出第一峰注释
    outIdx2 = idxB;  // 输出第二峰注释
    return true;     // 返回成功注释
}  // 结束注释

QVariantMap MainViewModel::calcTC(const QVariantList& adcList, int id) {
    QVariantMap r;

    int n = adcList.size();
    if (n < 600) {
        qWarning() << "[calcTC] curve too short, n=" << n;
        return r;
    }
    // =========================
    // 0) 根据 id 取方法配置
    // =========================
    if (!m_methodVm) {
        qWarning() << "[calcTC] methodVm not set";
        return r;
    }

    QrMethodConfigViewModel::Item method =
        m_methodVm->findItemById(id);
    qInfo() << "[calcTC] methodId=" << id
            << "methodData.len=" << method.methodData.size()
            << "methodData=" << method.methodData;
    if (method.rid == 0) {
        qWarning() << "[calcTC] invalid method config id =" << id;
        return QVariantMap();
    }
    // methodData 中的 type 决定 T/C 后使用哪条曲线反算浓度。
    QJsonObject methodCurveJson;
    if (!parseMethodJson(method.methodData, methodCurveJson)) {
        qWarning() << "[calcTC] invalid curve params, methodId =" << id;
        return QVariantMap();
    }
    const int curveType = methodCurveJson.value("type").toInt(0);
    // -------------------------
    // 1) 转数组
    // -------------------------
    QVector<double> y(n);
    for (int i = 0; i < n; ++i)
        y[i] = adcList[i].toDouble();

    // =========================
    // 2) 软件判峰：全曲线找两个主峰（左=C，右=T）
    // =========================
    const double MIN_PROM = 0.0;  // 显著性阈值注释（稳定数据先用 0）
    const int MIN_SEP = 300;      // 两峰最小间隔(样点)注释（你指定 300）

    int p1 = -1;  // 第一个峰索引注释
    int p2 = -1;  // 第二个峰索引注释

    bool ok = findTwoMainPeaksSoftware(y, 0, n - 1, MIN_PROM, MIN_SEP, p1, p2);  // 找两主峰注释
    if (!ok) {                                                                   // 失败兜底注释
        qWarning() << "[calcTC] findTwoMainPeaksSoftware failed";                // 打印注释
        return QVariantMap();                                                    // 返回空注释
    }  // 结束注释

    int idxC = std::min(p1, p2);  // 左边峰当 C 注释
    int idxT = std::max(p1, p2);  // 右边峰当 T 注释

    // ---- 峰值 7 点平均 ----
    double C_raw = avgPeak(y, idxC);
    double T_raw = avgPeak(y, idxT);
    // 打印：C/T 两峰中更大的那个峰位置
    // =========================
    int idxMaxCT = idxC;  // 默认先认为 C 更高注释
    if (y[idxT] > y[idxC])
        idxMaxCT = idxT;  // 如果 T 更高则取 T 注释

    qDebug() << "[calcTC] maxPeakCT idx=" << idxMaxCT             // 打印最高峰索引注释
             << " val=" << y[idxMaxCT]                            // 打印最高峰原始值注释
             << " (C idx=" << idxC << " val=" << y[idxC]          // 顺带打印 C 峰注释
             << ", T idx=" << idxT << " val=" << y[idxT] << ")";  // 顺带打印 T 峰注释

    // ======================================================================
    // 3) 初步 baseline 判断 T 是否存在
    // ======================================================================
    int a0 = qMin(idxC, idxT);
    int b0 = qMax(idxC, idxT);

    double baseline_est = y[a0];
    for (int i = a0; i <= b0; i++)
        baseline_est = std::min(baseline_est, y[i]);

    double T_net_est = T_raw - baseline_est;

    // ---- 阴性卡自动识别 ----
    bool hasT = (T_net_est >= 0.01);  // 阈值可调

    // ======================================================================
    // 4) 最终 baseline
    // ======================================================================
    double baseline = 0;

    if (hasT) {
        baseline = y[a0];
        for (int i = a0; i <= b0; i++)
            baseline = std::min(baseline, y[i]);
    } else {
        int L = 800, R = 950;
        if (R >= n)
            R = n - 1;

        baseline = y[L];
        for (int i = L; i <= R; i++)
            baseline = std::min(baseline, y[i]);
    }

    // ======================================================================
    // 5) 扣除本底
    // ======================================================================
    double C_net = C_raw - baseline;
    double T_net = hasT ? (T_raw - baseline) : 0;

    if (C_net < 0)
        C_net = 0;
    if (T_net < 0)
        T_net = 0;

    // ======================================================================
    // 6) 计算 T/C（竞争法）
    // ======================================================================
    double ratio = 0;
    if (C_net > 0)
        ratio = T_net / C_net;

    // ======================================================================
    // 7) 根据曲线类型计算浓度
    // ======================================================================
    double rawConcentration = 0.0;
    int calcCurveType = 0;
    if (!calculateConcentrationByCurveType(method.methodData, ratio, rawConcentration, calcCurveType)) {
        qWarning() << "[calcTC] calculate concentration failed, methodId =" << id
                   << " curveType=" << curveType;
        return QVariantMap();
    }
    double concentration = rawConcentration;
    QString concentrationDisplay;
    qDebug() << "--------concentration = " << concentration;
    // ======================================================================
    // 8) ★★★ 只有两个结果：阳性 / 阴性 ★★★
    // ======================================================================
    const double CUTOFF = 0.20;  // ← 竞争法典型阈值
    if (method.C2 > 0 && concentration > method.C2) {
        concentration = method.C2;
        concentrationDisplay = ">" + formatLimitValue(method.C2);
    } else if (method.T1 > 0 && concentration < method.T1) {
        concentration = method.T1;
        concentrationDisplay = "<" + formatLimitValue(method.T1);
    } else {
        concentrationDisplay = QString::number(concentration, 'f', 3);
    }
    QString resultStr;
    if (concentration > method.C1)
        resultStr = "阳性";
    else
        resultStr = "阴性";

    // ======================================================================
    // 9) 输出到 QML
    // ======================================================================
    r["idxC"] = idxC;
    r["idxT"] = idxT;
    r["hasT"] = hasT;

    r["C_raw"] = C_raw;
    r["T_raw"] = T_raw;
    r["baseline"] = baseline;

    r["C_net"] = C_net;
    r["T_net"] = T_net;

    r["ratioTC"] = ratio;
    r["curveType"] = calcCurveType;
    r["concentration"] = concentration;
    r["concentrationDisplay"] = concentrationDisplay;
    r["resultStr"] = resultStr;

    // // 日志
    qDebug() << "[calcTC]"
             << " idxC=" << idxC
             << " idxT=" << idxT
             << " hasT=" << hasT
             << " baseline=" << baseline
             << " C_net=" << C_net
             << " T_net=" << T_net
             << " ratio=" << ratio
             << " rawConc=" << rawConcentration
             << " conc=" << concentration
             << " concDisplay=" << concentrationDisplay
             << " resultStr=" << resultStr;
    qDebug() << "[calcTC][DBG]"
             << "range=0.." << (n - 1)                // 本次判峰范围注释
             << "MIN_SEP=" << MIN_SEP                 // 两峰最小间隔注释
             << "p1=" << p1 << "y1=" << y[p1]         // 第一个峰注释
             << "p2=" << p2 << "y2=" << y[p2]         // 第二个峰注释
             << "idxC=" << idxC << "yC=" << y[idxC]   // C 峰注释
             << "idxT=" << idxT << "yT=" << y[idxT];  // T 峰注释

    return r;
}
QVariantMap MainViewModel::testCalcTCByRatio(double ratio, int id) {
    QVariantMap r;

    if (!m_methodVm) {
        qWarning() << "[testCalcTCByRatio] methodVm not set";
        return r;
    }

    QrMethodConfigViewModel::Item method =
        m_methodVm->findItemById(id);

    qInfo() << "[testCalcTCByRatio] methodId=" << id
            << "ratio=" << ratio
            << "methodData.len=" << method.methodData.size()
            << "methodData=" << method.methodData;

    if (method.rid == 0) {
        qWarning() << "[testCalcTCByRatio] invalid method config id =" << id;
        return QVariantMap();
    }

    QJsonObject methodCurveJson;
    if (!parseMethodJson(method.methodData, methodCurveJson)) {
        qWarning() << "[testCalcTCByRatio] invalid curve params, methodId =" << id;
        return QVariantMap();
    }

    const int curveType = methodCurveJson.value("type").toInt(0);
    qInfo() << "[testCalcTCByRatio] curveType=" << curveType
            << "ratio=" << ratio;

    double rawConcentration = 0.0;
    int calcCurveType = 0;
    if (!calculateConcentrationByCurveType(method.methodData, ratio, rawConcentration, calcCurveType)) {
        qWarning() << "[testCalcTCByRatio] calculate concentration failed, methodId =" << id
                   << " curveType=" << curveType
                   << " ratio=" << ratio;
        return QVariantMap();
    }
    qInfo() << "[testCalcTCByRatio] rawConcentration=" << rawConcentration
            << "calcCurveType=" << calcCurveType;

    double concentration = rawConcentration;
    QString concentrationDisplay;

    if (method.C2 > 0 && concentration > method.C2) {
        concentration = method.C2;
        concentrationDisplay = ">" + formatLimitValue(method.C2);
    } else if (method.T1 > 0 && concentration < method.T1) {
        concentration = method.T1;
        concentrationDisplay = "<" + formatLimitValue(method.T1);
    } else {
        concentrationDisplay = QString::number(concentration, 'f', 3);
    }

    QString resultStr;
    if (concentration > method.C1)
        resultStr = "阳性";
    else
        resultStr = "阴性";

    r["methodId"] = id;
    r["ratioTC"] = ratio;
    r["curveType"] = calcCurveType;
    r["rawConcentration"] = rawConcentration;
    r["concentration"] = concentration;
    r["concentrationDisplay"] = concentrationDisplay;
    r["resultStr"] = resultStr;

    qInfo() << "[testCalcTCByRatio] done"
            << "methodId=" << id
            << "ratio=" << ratio
            << "curveType=" << calcCurveType
            << "rawConcentration=" << rawConcentration
            << "concentration=" << concentration
            << "concentrationDisplay=" << concentrationDisplay
            << "resultStr=" << resultStr;

    return r;
}
