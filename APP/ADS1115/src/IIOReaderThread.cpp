#include "IIOReaderThread.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cerrno>
#include <cmath>
#include <cstring>

static inline bool isMilliVoltScale(double s) {
    static const double tbl[] = {0.1875, 0.125, 0.0625, 0.03125, 0.015625, 0.0078125};
    for (double t : tbl) {
        if (std::fabs(s - t) < 1e-6)  // ✅ 用 std::fabs
            return true;
    }
    return false;
}

IIOReaderThread::IIOReaderThread(QObject* parent) : QObject(parent) {}
IIOReaderThread::~IIOReaderThread() { stop(); }

bool IIOReaderThread::fileExists(const QString& p) { return QFileInfo::exists(p); }

bool IIOReaderThread::writeTextFile(const QString& path, const QByteArray& data) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    auto ok = f.write(data) >= 0;
    f.close();
    return ok;
}

bool IIOReaderThread::readTextFile(const QString& path, QByteArray& out) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    out = f.readAll();
    f.close();
    return true;
}

QString IIOReaderThread::findIioDevDir(int index) {
    QDir base("/sys/bus/iio/devices");
    const auto ent = base.entryList(QStringList() << "iio:device*", QDir::Dirs);
    if (ent.isEmpty())
        return {};
    int idx = (index >= 0 && index < ent.size()) ? index : 0;
    return base.absoluteFilePath(ent.at(idx));
}

QString IIOReaderThread::devNodeFromDir(const QString& devDir) {
    auto base = QFileInfo(devDir).fileName();  // iio:deviceX
    return "/dev/" + base;
}

bool IIOReaderThread::ensureSysfsReady() {
    devDir_ = findIioDevDir(deviceIndex_);
    if (devDir_.isEmpty()) {
        qWarning() << "[IIO] 没有找到 /sys/bus/iio/devices/iio:deviceX";
        return false;
    }
    devNode_ = devNodeFromDir(devDir_);
    scanRoot_ = devDir_ + "/scan_elements";
    bufRoot_ = devDir_ + "/buffer";
    trigRoot_ = devDir_ + "/trigger";
    scalePath_ = devDir_ + "/in_voltage0_scale";

    {
        QByteArray s;
        if (readTextFile(scalePath_, s)) {
            bool ok = false;
            double v = QString::fromLatin1(s).trimmed().toDouble(&ok);
            if (ok && v > 0)
                lastScale_ = v;
        }
        qInfo() << "[IIO] scale =" << lastScale_ << (isMilliVoltScale(lastScale_) ? "(mV/LSB)" : "(V/LSB)");
    }

    {
        QString sf = devDir_ + "/sampling_frequency";
        if (!fileExists(sf))
            sf = devDir_ + "/in_voltage0_sampling_frequency";
        if (fileExists(sf)) {
            if (!writeTextFile(sf, QByteArray::number(targetHz_) + "\n"))
                qWarning() << "[IIO] 设置 sampling_frequency 失败，path=" << sf;
        }
    }

    if (!writeTextFile(scanRoot_ + "/in_voltage0_en", "1\n")) {
        qWarning() << "[IIO] 写 in_voltage0_en 失败";
        return false;
    }
    writeTextFile(scanRoot_ + "/in_timestamp_en", "0\n");

    if (!writeTextFile(bufRoot_ + "/length", QByteArray::number(bufLen_) + "\n")) {
        qWarning() << "[IIO] 写 buffer/length 失败";
        return false;
    }
    if (!writeTextFile(bufRoot_ + "/watermark", QByteArray::number(watermark_) + "\n")) {
        qWarning() << "[IIO] 写 buffer/watermark 失败";
        return false;
    }

    QString trigName = triggerName_;
    if (trigName.isEmpty()) {
        QByteArray s;
        QString t0 = "/sys/bus/iio/devices/trigger0/name";
        if (readTextFile(t0, s))
            trigName = QString::fromLatin1(s).trimmed();
        if (trigName.isEmpty()) {
            qWarning() << "[IIO] 找不到 trigger0/name；如需 hrtimer 请加载 iio-trig-hrtimer";
            return false;
        }
    }
    if (!writeTextFile(trigRoot_ + "/current_trigger", trigName.toUtf8() + "\n")) {
        qWarning() << "[IIO] 绑定 trigger 失败，name=" << trigName;
        return false;
    }
    qInfo() << "[IIO] 使用 trigger =" << trigName;

    configured_ = true;
    return true;
}

void IIOReaderThread::teardownSysfs() {
    if (configured_) {
        writeTextFile(bufRoot_ + "/enable", "0\n");
        writeTextFile(trigRoot_ + "/current_trigger", "\n");
        writeTextFile(scanRoot_ + "/in_voltage0_en", "0\n");
    }
    configured_ = false;
}

bool IIOReaderThread::start() {
    if (running_.load())
        return true;
    if (!ensureSysfsReady())
        return false;

    fd_ = ::open(devNode_.toLocal8Bit().constData(), O_RDONLY);
    if (fd_ < 0) {
        qWarning() << "[IIO] 打开" << devNode_ << "失败:" << strerror(errno);
        teardownSysfs();
        return false;
    }
    if (!writeTextFile(bufRoot_ + "/enable", "1\n")) {
        qWarning() << "[IIO] 启用 buffer 失败";
        ::close(fd_);
        fd_ = -1;
        teardownSysfs();
        return false;
    }

    running_ = true;
    worker_ = std::thread(&IIOReaderThread::threadMain, this);
    qInfo() << "[IIO] buffer/trigger 采集启动: dev=" << devNode_
            << " Hz=" << targetHz_ << " watermark=" << watermark_
            << " length=" << bufLen_;
    return true;
}

void IIOReaderThread::stop() {
    if (!running_.load())
        return;
    running_ = false;
    if (worker_.joinable())
        worker_.join();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    teardownSysfs();
    qInfo() << "[IIO] 采集已停止";
}

void IIOReaderThread::threadMain() {
    const int bytesPerSample = 2;  // int16
    std::vector<char> buf(watermark_ * bytesPerSample * 4);
    std::vector<double> batch;
    batch.reserve(watermark_);

    struct pollfd pfd{fd_, POLLIN, 0};

    while (running_.load()) {
        int pr = ::poll(&pfd, 1, 1000);
        if (pr <= 0) {
            if (pr < 0 && errno == EINTR)
                continue;
            else
                continue;
        }

        ssize_t n = ::read(fd_, buf.data(), (int)buf.size());
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            else
                break;
        }

        const int samples = n / bytesPerSample;
        batch.clear();
        batch.reserve(samples);

        // 更新一次 scale
        QByteArray s;
        if (readTextFile(scalePath_, s)) {
            bool ok = false;
            double v = QString::fromLatin1(s).trimmed().toDouble(&ok);
            if (ok && v > 0)
                lastScale_ = v;
        }
        const double k = isMilliVoltScale(lastScale_) ? (lastScale_ / 1000.0) : lastScale_;

        for (int i = 0; i < samples; ++i) {
            const unsigned char* p = reinterpret_cast<unsigned char*>(buf.data()) + i * bytesPerSample;
            int16_t raw = (int16_t)(p[0] | (p[1] << 8));
            batch.push_back(double(raw) * k);
        }

        if (!batch.empty()) {
            emit newDataBatch(QVector<double>::fromStdVector(batch));
        }
    }
}
