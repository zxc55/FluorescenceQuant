

#include "V4L2MjpegGrabber.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <QBuffer>
#include <QByteArray>
#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <cerrno>
#include <cmath>
#include <cstring>

#ifndef CLEAR
#define CLEAR(x) memset(&(x), 0, sizeof(x))
#endif

static int xioctl(int fd, int request, void* arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

V4L2MjpegGrabber::V4L2MjpegGrabber() {}
V4L2MjpegGrabber::~V4L2MjpegGrabber() { stop(); }

void V4L2MjpegGrabber::setFrameCallback(FrameCallback cb) { m_callback = std::move(cb); }

bool V4L2MjpegGrabber::openDevice(const QString& device) {
    m_fd = ::open(device.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK, 0);
    if (m_fd < 0) {
        qWarning() << "open device failed:" << device << "errno=" << errno;
        return false;
    }
    v4l2_capability cap;
    CLEAR(cap);
    if (xioctl(m_fd, VIDIOC_QUERYCAP, &cap) == -1) {
        qWarning() << "VIDIOC_QUERYCAP failed";
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
        qWarning() << "Device doesn't support capture+streaming";
        return false;
    }
    return true;
}

bool V4L2MjpegGrabber::setFormat(int width, int height, int fps) {
    // --- 1) 设置像素格式/分辨率/逐行 ---
    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = m_pixfmt;   // 改为 YUYV 4:2:2
    fmt.fmt.pix.field = V4L2_FIELD_NONE;  // 逐行，避免 ANY 触发交错

    // YUYV 为 16bpp，给出期望值（不少驱动会自行修正，但给出有助于稳定）
    fmt.fmt.pix.bytesperline = fmt.fmt.pix.width * 2;
    fmt.fmt.pix.sizeimage = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;

    if (xioctl(m_fd, VIDIOC_S_FMT, &fmt) == -1) {
        qWarning() << "VIDIOC_S_FMT (YUYV) failed, errno=" << errno;
        return false;
    }

    // 读回看驱动是否调整了参数（有些摄像头会改分辨率或field）
    if (xioctl(m_fd, VIDIOC_G_FMT, &fmt) == -1) {
        qWarning() << "VIDIOC_G_FMT failed, errno=" << errno;
    }

    // --- 2) 设置帧率 10fps（1/10s） ---
    struct v4l2_streamparm sp;
    CLEAR(sp);
    sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sp.parm.capture.timeperframe.numerator = 1;      // ← 关键：用 1/denom 表示帧间隔
    sp.parm.capture.timeperframe.denominator = fps;  // 10 => 10fps
    if (xioctl(m_fd, VIDIOC_S_PARM, &sp) == -1) {
        qWarning() << "VIDIOC_S_PARM failed, errno=" << errno;
        // 不返回，让后续尽量继续
    }

    // 再读回实际帧率（驱动可能做了四舍五入或限制）
    if (xioctl(m_fd, VIDIOC_G_PARM, &sp) == 0 &&
        sp.parm.capture.timeperframe.numerator > 0) {
        double fps_actual =
            (double)sp.parm.capture.timeperframe.denominator /
            (double)sp.parm.capture.timeperframe.numerator;
        m_fps = (int)llround(fps_actual);
    } else {
        m_fps = fps;  // 读回失败就用期望值
    }

    // --- 3) 更新成员为驱动最终采用的参数 ---
    m_width = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;

    return true;
}

bool V4L2MjpegGrabber::initMmap() {
    v4l2_requestbuffers req;
    CLEAR(req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (req.count < 2) {
        qCritical() << "REQBUFS returned too few buffers:" << req.count;
        return false;
    }
    if (xioctl(m_fd, VIDIOC_REQBUFS, &req) == -1) {
        qWarning() << "VIDIOC_REQBUFS failed";

        return false;
    }
    m_buffers.resize(req.count);

    for (unsigned i = 0; i < req.count; ++i) {
        v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(m_fd, VIDIOC_QUERYBUF, &buf) == -1) {
            qWarning() << "VIDIOC_QUERYBUF failed";
            return false;
        }
        m_buffers[i].length = buf.length;
        m_buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
        if (m_buffers[i].start == MAP_FAILED) {
            qWarning() << "mmap failed";
            return false;
        }
        if (xioctl(m_fd, VIDIOC_QBUF, &buf) == -1) {
            qWarning() << "VIDIOC_QBUF failed";
            return false;
        }
    }
    return true;
}

bool V4L2MjpegGrabber::startStreaming() {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_fd, VIDIOC_STREAMON, &type) == -1) {
        qWarning() << "VIDIOC_STREAMON failed";
        return false;
    }
    return true;
}

void V4L2MjpegGrabber::stopStreaming() {
    if (m_fd < 0)
        return;
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(m_fd, VIDIOC_STREAMOFF, &type);
    for (auto& b : m_buffers) {
        if (b.start && b.start != MAP_FAILED)
            munmap(b.start, b.length);
    }
    m_buffers.clear();
}

void V4L2MjpegGrabber::closeDevice() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool V4L2MjpegGrabber::start(const QString& device, int width, int height, int fps) {
    if (m_running.load())
        return true;
    if (!openDevice(device))
        return false;
    if (!setFormat(width, height, fps)) {
        closeDevice();
        return false;
    }
    if (!initMmap()) {
        closeDevice();
        return false;
    }
    if (!startStreaming()) {
        stopStreaming();
        closeDevice();
        return false;
    }

    m_running.store(true);
    m_thread = std::thread(&V4L2MjpegGrabber::runLoop, this);
    return true;
}

void V4L2MjpegGrabber::stop() {
    if (!m_running.exchange(false))
        return;
    if (m_thread.joinable())
        m_thread.join();
    stopStreaming();
    closeDevice();
}
void V4L2MjpegGrabber::runLoop() {
    // 改为阻塞式 DQBUF
    int flags = fcntl(m_fd, F_GETFL, 0);
    if (flags != -1 && (flags & O_NONBLOCK))
        (void)fcntl(m_fd, F_SETFL, flags & ~O_NONBLOCK);

    // 简单背压：如果上一帧还在处理，就丢当前帧，防止内存/CPU堆积
    static std::atomic<bool> busy{false};

    while (m_running.load(std::memory_order_relaxed)) {
        v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        // 阻塞直到有帧（或被 STREAMOFF 唤醒/出错）
        if (xioctl(m_fd, VIDIOC_DQBUF, &buf) == -1) {
            if (errno == EINTR)
                continue;
            qWarning() << "VIDIOC_DQBUF failed, errno=" << errno;
            break;
        }
        if (buf.flags & V4L2_BUF_FLAG_ERROR) {
            (void)xioctl(m_fd, VIDIOC_QBUF, &buf);
            continue;
        }

        const char* src = static_cast<const char*>(m_buffers[buf.index].start);
        const int used = buf.bytesused;

        QImage gray;

        if (m_pixfmt == V4L2_PIX_FMT_MJPEG) {
            // 下游忙就直接归还这帧，避免堆积
            if (busy.exchange(true)) {
                (void)xioctl(m_fd, VIDIOC_QBUF, &buf);
                continue;
            }

            // 直接在 mmap 数据上解码（无深拷贝）
            QByteArray raw = QByteArray::fromRawData(src, used);
            QBuffer qbuf;
            qbuf.setData(raw);
            qbuf.open(QIODevice::ReadOnly);
            QImageReader reader(&qbuf, "JPG");
            reader.setAutoTransform(false);

            // 关键：按需“解码时下采样”，显著降低内存与CPU（libjpeg 支持 1/2,1/4 等下采样）
            if (m_width > 800) {
                const int tw = 800;
                const int th = (int)((double)m_height * tw / m_width);
                reader.setScaledSize(QSize(tw, th));
            }

            QImage img = reader.read();

            // 解码完再归还缓冲
            if (xioctl(m_fd, VIDIOC_QBUF, &buf) == -1) {
                qWarning() << "VIDIOC_QBUF (requeue) failed";
                busy.store(false);
                break;
            }

            if (!img.isNull())
                gray = img.convertToFormat(QImage::Format_Grayscale8);

            busy.store(false);

        } else if (m_pixfmt == V4L2_PIX_FMT_YUYV) {
            // 直接提 Y 面，尽快 QBUF
            const int strideYUYV = used / m_height;
            QImage g(m_width, m_height, QImage::Format_Grayscale8);
            for (int y = 0; y < m_height; ++y) {
                const uint8_t* line = reinterpret_cast<const uint8_t*>(src) + y * strideYUYV;
                uchar* dst = g.scanLine(y);
                for (int x = 0; x < m_width; ++x) dst[x] = line[x * 2];
            }
            if (xioctl(m_fd, VIDIOC_QBUF, &buf) == -1) {
                qWarning() << "VIDIOC_QBUF (requeue) failed";
                break;
            }
            gray = std::move(g);

        } else {
            (void)xioctl(m_fd, VIDIOC_QBUF, &buf);
            continue;
        }

        if (!gray.isNull() && m_callback)
            m_callback(gray);
    }
}
