#include "MainViewModel.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>

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
    db.setDatabaseName("/mnt/SDCARD/app/db/app.db");

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
    db.setDatabaseName("/mnt/SDCARD/app/db/app.db");
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
