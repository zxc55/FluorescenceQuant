#include "IIOReaderThread.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <cmath>

// 仅通道0候选属性名
static const char* kRawCandidatesCH0[] = {
    "in_voltage0_raw",
    "in_voltage0_input",
    nullptr};

IIOReaderThread::IIOReaderThread(QObject* parent)
    : QObject(parent) {
    // 高精度定时器：主轮询
    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &IIOReaderThread::readIIODevice);

    // 看门狗：2s 检查一次
    watchdog = new QTimer(this);
    watchdog->setTimerType(Qt::CoarseTimer);
    watchdog->setInterval(2000);
    connect(watchdog, &QTimer::timeout, this, &IIOReaderThread::watchdogTick);
}

IIOReaderThread::~IIOReaderThread() {
    stop();
}

void IIOReaderThread::start(int intervalMs, int samplesPerRead) {
    if (running)
        return;

    intervalMs_ = intervalMs;
    samplesPerRead_ = samplesPerRead;

    if (!ensureSysfsReady()) {
        qWarning() << "IIO sysfs 未就绪，启动失败";
        return;
    }

    running = true;
    sameCount = 0;
    lastRaw = INT_MIN;
    lastEmitTick.start();
    clearFilter();

    timer->start(intervalMs_);
    watchdog->start();

    qInfo() << "启动 IIO 轮询: 每" << intervalMs_ << "ms 读取一次（通道0），目标ODR="
            << desiredRateHz << "Hz, 设备索引=" << deviceIndex;
}

void IIOReaderThread::stop() {
    if (!running)
        return;
    timer->stop();
    watchdog->stop();
    running = false;
    qInfo() << "IIO 轮询已停止";
}

bool IIOReaderThread::writeSysfsString(const QString& path, const QByteArray& s) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool ok = (f.write(s) >= 0);
    f.close();
    return ok;
}

// 宽松提取第一个整数（容忍 NUL/空白/奇怪分隔）
bool IIOReaderThread::parseFirstIntLoose(const QByteArray& bytes, int& out) {
    QByteArray s = bytes;
    s.replace('\0', ' ');
    s = s.trimmed();
    if (s.isEmpty())
        return false;

    int i = 0;
    while (i < s.size() && !(s[i] == '+' || s[i] == '-' || (s[i] >= '0' && s[i] <= '9'))) ++i;
    if (i >= s.size())
        return false;
    int j = i + 1;
    while (j < s.size() && (s[j] >= '0' && s[j] <= '9')) ++j;

    bool ok = false;
    out = QString::fromLatin1(s.constData() + i, j - i).toInt(&ok);
    return ok;
}

// POSIX 读 sysfs：10 次重试 + 指数退避（~50ms）
bool IIOReaderThread::readSysfsIntPosix(const QString& path, int& out) {
    const QByteArray pa = path.toLocal8Bit();
    int backoffMs = 1;

    for (int attempt = 0; attempt < 10; ++attempt) {
        int fd = ::open(pa.constData(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            QThread::msleep(backoffMs);
            backoffMs = qMin(backoffMs * 2, 8);
            continue;
        }

        char buf[128] = {0};
        ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
        int savedErrno = errno;
        (void)savedErrno;
        ::close(fd);

        if (n > 0) {
            int val = 0;
            if (parseFirstIntLoose(QByteArray(buf, int(n)), val)) {
                out = val;
                return true;
            }
        }

        QThread::msleep(backoffMs);
        backoffMs = qMin(backoffMs * 2, 8);
    }
    return false;
}

bool IIOReaderThread::readSysfsDouble(const QString& path, double& out) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QByteArray bytes = f.readAll();
    f.close();
    bool ok = false;
    out = QString::fromLatin1(bytes).trimmed().toDouble(&ok);
    return ok;
}

void IIOReaderThread::warmupReads(const QString& rawPath, int tries, int sleepMs) {
    int dummy = 0;
    for (int i = 0; i < tries; ++i) {
        readSysfsIntPosix(rawPath, dummy);  // 读到/读不到都无所谓
        QThread::msleep(sleepMs);
    }
}

double IIOReaderThread::voltageFromRawAndScale(int raw, double scale) {
    // scale < 0.1 → 单位为 “V/LSB”（标准内核）；否则按 mV 处理（常见私有分支）
    if (scale < 0.1)
        return (raw * 0.0625) / 1000;  // 伏
    else
        return (raw * 0.0625) / 1000;  // 毫伏 → 伏
}

bool IIOReaderThread::ensureSysfsReady() {
    // 已经就绪
    if (!sysfsDevDir.isEmpty() && !rawAttrPathCached.isEmpty())
        return true;

    // 1) 定位 iio:deviceX
    QDir base("/sys/bus/iio/devices");
    const auto dirs = base.entryList(QStringList() << "iio:device*", QDir::Dirs);
    if (dirs.isEmpty()) {
        qWarning() << "没有找到 IIO 设备目录: /sys/bus/iio/devices/iio:deviceX";
        return false;
    }
    QString chosen = (deviceIndex >= 0 && deviceIndex < dirs.size())
                         ? dirs[deviceIndex]
                         : dirs.first();
    sysfsDevDir = base.absoluteFilePath(chosen);
    qInfo() << "使用 IIO 目录:" << sysfsDevDir;

    // 2) 禁用 autosuspend（若支持）
    powerControlPath = sysfsDevDir + "/power/control";
    if (QFileInfo::exists(powerControlPath)) {
        if (writeSysfsString(powerControlPath, "on\n"))
            qInfo() << "已将" << powerControlPath << "设置为 'on'（禁用 autosuspend）";
    }

    // 3) 关闭 buffer（避免干扰 raw）
    {
        QFile be(sysfsDevDir + "/buffer/enable");
        if (be.exists() && be.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            be.write("0\n");
            be.close();
            qInfo() << "已关闭 buffer/enable";
        }
    }

    // 4) 设置采样率（接近 500Hz → 475Hz，或使用用户设置）
    {
        const QString sf0 = sysfsDevDir + "/in_voltage0_sampling_frequency";
        QFile f(sf0);
        if (f.exists() && f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QByteArray::number(desiredRateHz) + "\n");
            f.close();
            qInfo() << "设置采样率为" << desiredRateHz << "Hz";
            QThread::msleep(15);  // 给驱动一点时间
        }
    }

    // 5) 探测 CH0 的 raw 属性（存在即缓存，不强求立刻可读）
    rawAttrPathCached.clear();
    for (auto p : kRawCandidatesCH0) {
        if (!p)
            break;
        const QString path = sysfsDevDir + "/" + p;
        if (QFileInfo::exists(path)) {
            rawAttrPathCached = path;
            qInfo() << "选择 raw 属性候选:" << rawAttrPathCached;
            break;
        }
    }
    if (rawAttrPathCached.isEmpty()) {
        qWarning() << "未找到通道0的 raw 属性（in_voltage0_raw/input）";
        return false;
    }

    // ★ 预热几次，稳定后再正常读
    warmupReads(rawAttrPathCached, /*tries=*/6, /*sleepMs=*/5);

    // 6) 仅通道0的 scale：首次读取成功则缓存为 lastGoodScale，失败保留默认
    scaleAttrPathCached.clear();
    const QString s0 = sysfsDevDir + "/in_voltage0_scale";
    if (QFileInfo::exists(s0)) {
        scaleAttrPathCached = s0;
        double s = 0.0;
        if (readSysfsDouble(s0, s) && std::isfinite(s) && s > 0.0) {
            lastGoodScale = s;
            qInfo() << "使用 scale 属性:" << scaleAttrPathCached << " 初始值=" << lastGoodScale;
        } else {
            qWarning() << "scale 初次读取失败，使用兜底 lastGoodScale =" << lastGoodScale;
        }
    } else {
        qWarning() << "找不到 scale 属性，使用兜底 lastGoodScale =" << lastGoodScale;
    }

    return true;
}

void IIOReaderThread::recoverSysfs() {
    qWarning() << "检测到可能卡住，尝试恢复…";

    // 1) 再次禁用 autosuspend
    if (!powerControlPath.isEmpty() && QFileInfo::exists(powerControlPath))
        writeSysfsString(powerControlPath, "on\n");

    // 2) 重新设置采样率
    {
        const QString sf0 = sysfsDevDir + "/in_voltage0_sampling_frequency";
        if (QFileInfo::exists(sf0))
            writeSysfsString(sf0, QByteArray::number(desiredRateHz) + "\n");
    }

    // 3) 清缓存，稍后重新 ensure
    rawAttrPathCached.clear();
    scaleAttrPathCached.clear();
    sysfsDevDir.clear();

    // 4) 稍等
    QThread::msleep(20);

    // 5) 立即 ensure（也可等下次 read 再 ensure）
    ensureSysfsReady();

    // 6) 重置卡住检测
    sameCount = 0;
    lastRaw = INT_MIN;

    qInfo() << "恢复流程结束";
}

double IIOReaderThread::applyMovingAverage(double x) {
    if (!filterEnabled || filterWindow <= 1)
        return x;
    if (!std::isfinite(x))
        return x;  // 不把 NaN/Inf 放入窗口

    fifo.push_back(x);
    fifoSum += x;
    if ((int)fifo.size() > filterWindow) {
        fifoSum -= fifo.front();
        fifo.pop_front();
    }
    const double avg = fifoSum / double(fifo.size());
    return std::isfinite(avg) ? avg : x;
}

void IIOReaderThread::readIIODevice() {
    if (!running)
        return;
    if (!ensureSysfsReady())
        return;

    // 1) 读取 raw（仅通道0；失败则在 raw/input 间切换）
    int raw = 0;
    if (!readSysfsIntPosix(rawAttrPathCached, raw)) {
        qWarning() << QFileInfo(rawAttrPathCached).fileName()
                   << "读取/解析失败，尝试切换候选路径";

        bool switched = false;
        for (auto p : kRawCandidatesCH0) {
            if (!p)
                break;
            const QString alt = sysfsDevDir + "/" + p;
            if (alt == rawAttrPathCached)
                continue;
            if (!QFileInfo::exists(alt))
                continue;

            int tmp = 0;
            if (readSysfsIntPosix(alt, tmp)) {
                raw = tmp;
                rawAttrPathCached = alt;
                qInfo() << "切换 raw 属性为:" << rawAttrPathCached << " 当前值=" << raw;
                switched = true;
                break;
            }
        }
        if (!switched) {
            qWarning() << "所有候选 raw 路径均失败，本次放弃；看门狗稍后会尝试恢复";
            return;
        }
    }

    // 2) 读取 scale（失败则用 lastGoodScale；lastGoodScale 再失败用兜底 0.0001875）
    double scale = lastGoodScale;
    if (!scaleAttrPathCached.isEmpty()) {
        double s = 0.0;
        if (readSysfsDouble(scaleAttrPathCached, s) && std::isfinite(s) && s > 0.0) {
            scale = s;
            lastGoodScale = s;  // 更新缓存
        }  // 否则继续用 lastGoodScale
    }
    // if (!std::isfinite(scale) || scale <= 0.0) {
    //     scale = 0.0001875;  // 最终兜底（0.1875mV/LSB）
    //     lastGoodScale = scale;
    // }

    // 3) 电压换算
    double v = voltageFromRawAndScale(raw, scale);

    // // 4) 有限性检查 + 限频告警 + 使用 lastGoodValue 兜底
    // if (!std::isfinite(v)) {
    //     if ((invalidLogCounter++ % 20) == 0) {
    //         qWarning() << "Ignored NaN/Inf value. raw=" << raw
    //                    << " scale=" << scale
    //                    << " 使用上次有效值 lastGoodValue=" << lastGoodValue;
    //     }
    //     v = lastGoodValue;
    // }

    // // 5) 滤波（滑动均值）
    // v = applyMovingAverage(v);

    // // 6) 再做一次有限性保护（极端情况下滤波也可能出 NaN）
    // if (!std::isfinite(v)) {
    //     if ((invalidLogCounter++ % 20) == 0) {
    //         qWarning() << "Filtered value NaN/Inf，继续沿用 lastGoodValue=" << lastGoodValue;
    //     }
    //     v = lastGoodValue;
    // }

    // 7) 发布并更新 lastGoodValue
    emit newData(v);
    lastGoodValue = v;

    // 8) 卡住检测计数
    if (lastRaw == raw)
        ++sameCount;
    else {
        sameCount = 0;
        lastRaw = raw;
    }

    // 9) 记录 emit 时间
    lastEmitTick.restart();
}

void IIOReaderThread::watchdogTick() {
    if (!running)
        return;

    // 主定时器健康检查
    if (!timer->isActive()) {
        qWarning() << "采样定时器不活跃，自动重启";
        timer->start(intervalMs_);
    }

    // 连续相同值过多或过久未 emit → 恢复
    if (sameCount >= stallThreshold) {
        recoverSysfs();
        return;
    }
    if (lastEmitTick.isValid() && lastEmitTick.elapsed() > 5000) {
        recoverSysfs();
        return;
    }
}
