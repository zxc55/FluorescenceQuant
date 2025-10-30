#include "IIOReaderThread.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

static inline double median5(const double* buf, int n) {
    double tmp[5];
    for (int i = 0; i < n; ++i) tmp[i] = buf[i];
    std::sort(tmp, tmp + n);
    return tmp[n / 2];
}

bool IIOReaderThread::isLikelyMilliVoltScale(double s) {
    static const double tbl[] = {0.1875, 0.125, 0.0625, 0.03125, 0.015625, 0.0078125};
    for (double t : tbl)
        if (std::fabs(s - t) < 1e-6)
            return true;
    return false;
}

IIOReaderThread::IIOReaderThread(QObject* parent) : QObject(parent) {}
IIOReaderThread::~IIOReaderThread() { stop(); }

void IIOReaderThread::start() {
    if (running.load())
        return;
    if (!ensureSysfsReady()) {
        qWarning() << "IIO sysfs 未就绪，启动失败";
        return;
    }
    // 初始化滑动平均缓冲
    maBuf.assign(std::max(1, movWindow), 0.0);
    maIdx = 0;
    maCount = 0;
    maSum = 0.0;

    running = true;
    worker = std::thread(&IIOReaderThread::run, this);
    qInfo() << "IIOReaderThread 启动: 每" << batchPeriodMs
            << "ms 采样一次批次，每批" << samplesPerBatch << "点（≈"
            << (1000 * samplesPerBatch / batchPeriodMs) << "Hz）";
}

void IIOReaderThread::stop() {
    if (!running.load())
        return;
    running = false;
    if (worker.joinable())
        worker.join();
    qInfo() << "IIOReaderThread 已停止";
}

bool IIOReaderThread::ensureSysfsReady() {
    QDir base("/sys/bus/iio/devices");
    const auto dirs = base.entryList(QStringList() << "iio:device*", QDir::Dirs);
    if (dirs.isEmpty()) {
        qWarning() << "没有找到 iio:deviceX";
        return false;
    }
    int idx = deviceIndex;
    if (idx < 0 || idx >= dirs.size())
        idx = 0;
    sysfsDevDir = base.absoluteFilePath(dirs.at(idx));

    rawPath = sysfsDevDir + "/in_voltage0_raw";
    altRawPath = sysfsDevDir + "/in_voltage0_input";
    scalePath = sysfsDevDir + "/in_voltage0_scale";

    if (!QFileInfo::exists(rawPath) && !QFileInfo::exists(altRawPath)) {
        qWarning() << "未找到 in_voltage0_raw/input";
        return false;
    }
    if (!QFileInfo::exists(scalePath)) {
        qWarning() << "未找到 in_voltage0_scale，使用 lastGoodScale =" << lastGoodScale;
    }
    return true;
}

bool IIOReaderThread::readRawValue(int& raw) {
    QFile f(QFileInfo::exists(rawPath) ? rawPath : altRawPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QByteArray bytes = f.readAll();
    f.close();
    bool ok = false;
    raw = bytes.trimmed().toInt(&ok);
    return ok;
}

bool IIOReaderThread::readScale(double& scale) {
    if (!QFileInfo::exists(scalePath)) {
        scale = lastGoodScale;
        return false;
    }
    QFile f(scalePath);
    if (!f.open(QIODevice::ReadOnly)) {
        scale = lastGoodScale;
        return false;
    }
    QByteArray bytes = f.readAll();
    f.close();
    bool ok = false;
    double s = bytes.trimmed().toDouble(&ok);
    if (ok && s > 0.0) {
        lastGoodScale = s;
        scale = s;
        return true;
    }
    scale = lastGoodScale;
    return false;
}

double IIOReaderThread::rawToVolt(int raw, double scale) {
    // ADS1115 常见为 mV/LSB（当 PGA=±2.048V 时 scale=0.0625）
    return isLikelyMilliVoltScale(scale) ? (raw * (scale / 1000.0)) : (raw * scale);
}

void IIOReaderThread::run() {
    // 滤波状态复位
    emaInit = false;
    medCount = 0;

    const int stepMs = std::max(1, batchPeriodMs / std::max(1, samplesPerBatch));  // ≈ 2ms
    std::vector<double> batch;
    batch.reserve(samplesPerBatch);

    int raw = 0;
    double scale = lastGoodScale;

    while (running.load()) {
        const auto tBatchStart = std::chrono::steady_clock::now();

        // 批内只读一次 scale
        readScale(scale);

        batch.clear();
        for (int i = 0; i < samplesPerBatch && running.load(); ++i) {
            if (!readRawValue(raw)) {
                qWarning() << "[ADC] 读取 raw 失败，跳过本批余下数据";
                break;
            }

            double v = rawToVolt(raw, scale);

            if (!bypassFiltering) {
                // 1) 中值
                if (medianEnabled) {
                    if (medCount < MED_WIN)
                        medBuf[medCount++] = v;
                    else {
                        for (int k = 1; k < MED_WIN; ++k) medBuf[k - 1] = medBuf[k];
                        medBuf[MED_WIN - 1] = v;
                    }
                    v = (medCount < MED_WIN) ? v : median5(medBuf, MED_WIN);
                }
                // 2) 滑动平均（O(1)）
                if (movEnabled) {
                    if (maCount < movWindow) {
                        maBuf[maIdx] = v;
                        maSum += v;
                        maIdx = (maIdx + 1) % movWindow;
                        ++maCount;
                    } else {
                        maSum -= maBuf[maIdx];
                        maBuf[maIdx] = v;
                        maSum += v;
                        maIdx = (maIdx + 1) % movWindow;
                    }
                    v = maSum / double(maCount);
                }
                // 3) EMA
                if (emaEnabled) {
                    if (!emaInit) {
                        emaState = v;
                        emaInit = true;
                    } else {
                        emaState = emaAlpha * v + (1.0 - emaAlpha) * emaState;
                    }
                    v = emaState;
                }
            }

            if (calibEnabled)
                v = calibA * v + calibB;

            batch.push_back(v);

            // 批内点的节拍（约 2ms）
            std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
        }

        if (!batch.empty()) {
            QVector<double> qv = QVector<double>::fromStdVector(batch);
            // ❌ 已关闭批次打印
            emit newDataBatch(qv);
        }

        // 补齐到 50ms 边界
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - tBatchStart)
                           .count();
        int rest = batchPeriodMs - int(elapsed);
        if (rest > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(rest));
    }
}

void IIOReaderThread::setTwoPointCalib(double x1, double y1, double x2, double y2) {
    if (std::fabs(x2 - x1) < 1e-9) {
        calibEnabled = false;
        return;
    }
    calibA = (y2 - y1) / (x2 - x1);
    calibB = y1 - calibA * x1;
    calibEnabled = true;
    qInfo() << "[ADC] 两点校准: A=" << calibA << " B=" << calibB;
}

void IIOReaderThread::setEma(bool enabled, double alpha) {
    emaEnabled = enabled;
    if (alpha > 0.0 && alpha < 1.0)
        emaAlpha = alpha;
    emaInit = false;
    qInfo() << "[ADC] EMA" << (emaEnabled ? "启用" : "禁用") << " alpha=" << emaAlpha;
}

void IIOReaderThread::setMovingAverage(bool enabled, int window) {
    movEnabled = enabled;
    movWindow = std::max(1, window);
    maBuf.assign(movWindow, 0.0);
    maIdx = 0;
    maCount = 0;
    maSum = 0.0;
    qInfo() << "[ADC] MovingAverage" << (movEnabled ? "启用" : "禁用")
            << " 窗口=" << movWindow;
}
