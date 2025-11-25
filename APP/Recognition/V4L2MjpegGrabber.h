#ifndef _V4L2MJPEGGRABBER_H_
#define _V4L2MJPEGGRABBER_H_
#include <linux/videodev2.h>

#include <QImage>
#include <QString>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

class V4L2MjpegGrabber final {
public:
    using FrameCallback = std::function<void(const QImage&)>;

    V4L2MjpegGrabber();
    ~V4L2MjpegGrabber();

    // Configure callback to receive decoded grayscale frames
    void setFrameCallback(FrameCallback cb);

    // Start/stop capture. Returns true on success.
    bool start(const QString& device = "/dev/video0", int width = 1280, int height = 720, int fps = 10);
    void stop();

private:
    bool openDevice(const QString& device);
    bool initMmap();
    bool setFormat(int width, int height, int fps);
    bool startStreaming();
    void stopStreaming();
    void closeDevice();
    void runLoop();

private:
    int m_fd = -1;
    struct Buffer {
        void* start = nullptr;
        size_t length = 0;
    };
    std::vector<Buffer> m_buffers;

    std::thread m_thread;
    std::atomic<bool> m_running{false};

    FrameCallback m_callback;
    int m_width = 1280, m_height = 720, m_fps = 10;
    __u32 m_pixfmt = V4L2_PIX_FMT_YUYV;
};
#endif  // _V4L2MJPEGGRABBER_H_
