#include "IIOReaderThread.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <climits>

// 只读通道0（AIN0）的候选 raw 属性名
static const char* kRawCandidatesCH0[] = {
    "in_voltage0_raw",
    "in_voltage0_input",
    nullptr};

IIOReaderThread::IIOReaderThread(QObject* parent)
    : QObject(parent) {
    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &IIOReaderThread::readIIODevice);

    watchdog = new QTimer(this);
    watchdog->setTimerType(Qt::CoarseTimer);
    watchdog->setInterval(2000);  // 2秒检查一次
    connect(watchdog, &QTimer::timeout, this, &IIOReaderThread::watchdogTick);
}

IIOReaderThread::~IIOReaderThread() {
    stop();
}

void IIOReaderThread::start(int intervalMs, int /*samplesPerRead*/) {
    if (running)
        return;
    intervalMs_ = intervalMs;

    if (!ensureSysfsReady()) {
        qWarning() << "IIO sysfs 未就绪，启动失败";
        return;
    }

    running = true;
    sameCount = 0;
    lastRaw = INT_MIN;
    lastEmitTick.start();

    timer->start(intervalMs_);
    watchdog->start();
    qInfo() << "启动 IIO 轮询: 每" << intervalMs_ << "ms 读取一次（通道0）";
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
static bool parseFirstIntLoose(const QByteArray& bytes, int& out) {
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

// POSIX 读 sysfs：10次重试 + 指数退避（~50ms）
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
            QByteArray data(buf, int(n));
            int val = 0;
            if (parseFirstIntLoose(data, val)) {
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
    // scale < 0.1 → V/LSB（标准内核）；否则按 mV 处理（你的板子常见）
    if (scale < 0.1)
        return raw * scale;  // 伏
    else
        return (raw * scale) / 1000;  // 毫伏→伏
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
    sysfsDevDir = base.absoluteFilePath(dirs.first());
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

    // 4) 设置采样率（接近 500Hz → 475Hz）
    {
        QString sf0 = sysfsDevDir + "/in_voltage0_sampling_frequency";
        QFile f(sf0);
        if (f.exists() && f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("475\n");
            f.close();
            qInfo() << "设置采样率为 475Hz";
            QThread::msleep(15);  // 给驱动一点时间
        }
    }

    // 5) 探测 CH0 的 raw 属性（存在即缓存，不强求立刻可读）
    rawAttrPathCached.clear();
    for (const char** p = kRawCandidatesCH0; *p; ++p) {
        const QString path = sysfsDevDir + "/" + *p;
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

    // 6) 仅通道0的 scale
    scaleAttrPathCached.clear();
    const QString s0 = sysfsDevDir + "/in_voltage0_scale";
    if (QFileInfo::exists(s0)) {
        scaleAttrPathCached = s0;
        qInfo() << "使用 scale 属性:" << scaleAttrPathCached;
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
        QString sf0 = sysfsDevDir + "/in_voltage0_sampling_frequency";
        if (QFileInfo::exists(sf0))
            writeSysfsString(sf0, "475\n");
    }

    // 3) 清缓存，稍后重新 ensure
    rawAttrPathCached.clear();
    scaleAttrPathCached.clear();
    sysfsDevDir.clear();

    // 4) 等一会
    QThread::msleep(20);

    // 5) 尝试立即 ensure（也可等下次 read 再 ensure）
    ensureSysfsReady();

    // 6) 重置卡住检测
    sameCount = 0;
    lastRaw = INT_MIN;

    qInfo() << "恢复流程结束";
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

        static const char* kCandidates[] = {
            "in_voltage0_raw",
            "in_voltage0_input",
            nullptr};

        bool switched = false;
        for (const char** p = kCandidates; *p; ++p) {
            const QString alt = sysfsDevDir + "/" + *p;
            if (alt == rawAttrPathCached)
                continue;
            if (!QFileInfo::exists(alt))
                continue;

            int tmp = 0;
            if (readSysfsIntPosix(alt, tmp)) {
                rawAttrPathCached = alt;
                raw = tmp;
                qInfo() << "切换 raw 属性为:" << rawAttrPathCached
                        << "当前值=" << raw;
                switched = true;
                break;
            }
        }
        if (!switched) {
            qWarning() << "所有候选 raw 路径均失败，本次放弃；看门狗稍后会尝试恢复";
            return;
        }
    }

    // 2) 读取 scale（失败则用默认 3.3/32768）
    double scale = 3.3 / 32768.0;
    if (!scaleAttrPathCached.isEmpty()) {
        double s = 0.0;
        if (readSysfsDouble(scaleAttrPathCached, s))
            scale = s;
    }

    // 3) 电压换算并发出
    const double voltage = voltageFromRawAndScale(raw, scale);
    emit newData(voltage);

    // 4) 卡住检测计数
    if (lastRaw == raw)
        ++sameCount;
    else {
        sameCount = 0;
        lastRaw = raw;
    }

    // 5) 记录 emit 时间
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

    // 连续相同值过多 or 过久未 emit → 恢复
    if (sameCount >= stallThreshold) {
        recoverSysfs();
        return;
    }
    if (lastEmitTick.isValid() && lastEmitTick.elapsed() > 5000) {
        recoverSysfs();
        return;
    }
}