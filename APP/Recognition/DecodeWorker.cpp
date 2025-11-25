#include "DecodeWorker.h"

#include <QDebug>
#include <chrono>

#include "../QUIRC/quirc.h"
DecodeWorker::DecodeWorker(ResultCallback cb) : m_onResult(std::move(cb)) {}
DecodeWorker::~DecodeWorker() { stop(); }

void DecodeWorker::start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true))
        return;
    m_thread = std::thread(&DecodeWorker::loop, this);
}

void DecodeWorker::stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false))
        return;
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_queue = std::queue<QImage>();
    }
}

void DecodeWorker::enqueueFrame(const QImage& img) {
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_queue.push(img);
        while (static_cast<int>(m_queue.size()) > m_queueSize) {
            m_queue.pop();
        }
        // qDebug() << "Enqueued frame, queue size:" << m_queue.size();
    }
    m_cv.notify_one();
}

void DecodeWorker::loop() {
    while (m_running.load(std::memory_order_relaxed)) {
        QImage img;
        {
            std::unique_lock<std::mutex> lk(m_mtx);
            m_cv.wait(lk, [&] { return !m_running.load() || !m_queue.empty(); });
            if (!m_running.load())
                break;
            if (m_queue.empty())
                continue;
            img = m_queue.front();
            m_queue.pop();
        }
        QString outText;
        if (processFrameQuirc(img, outText) && m_onResult) {
            m_onResult(outText);
            continue;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool DecodeWorker::processFrameQuirc(const QImage& img, QString& outText) {
    if (img.isNull())
        return false;

    // 灰度输入（若已是灰度不再转换，避免额外内存）
    const QImage gray = (img.format() == QImage::Format_Grayscale8)
                            ? img
                            : img.convertToFormat(QImage::Format_Grayscale8);
    const int W = gray.width(), H = gray.height();
    const int STRIDE = gray.bytesPerLine();
    const uint8_t* S = gray.constBits();

    // quirc 实例（持久化）
    static quirc* q = []() { return quirc_new(); }();
    if (!q)
        return false;

    // 将 ROI（可选反色）逐行写入 quirc 缓冲（零缩放、低拷贝）
    auto feedROI = [&](int x0, int y0, int w, int h, bool invert) -> bool {
        if (w <= 0 || h <= 0)
            return false;
        static int qw = 0, qh = 0;
        if (qw != w || qh != h) {
            if (quirc_resize(q, w, h) < 0)
                return false;
            qw = w;
            qh = h;
        }
        int ow = 0, oh = 0;
        uint8_t* dst = quirc_begin(q, &ow, &oh);
        if (!dst || ow != w || oh != h)
            return false;

        if (!invert) {
            for (int y = 0; y < h; ++y) {
                const uint8_t* src = S + (y0 + y) * STRIDE + x0;
                memcpy(dst + y * w, src, w);
            }
        } else {
            for (int y = 0; y < h; ++y) {
                const uint8_t* src = S + (y0 + y) * STRIDE + x0;
                uint8_t* d = dst + y * w;
                for (int x = 0; x < w; ++x) d[x] = 255 - src[x];
            }
        }
        quirc_end(q);
        return true;
    };

    auto decodeNow = [&](QString& out) -> bool {
        const int n = quirc_count(q);
        for (int i = 0; i < n; ++i) {
            quirc_code code;
            quirc_data data;
            quirc_extract(q, i, &code);
            if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
                out = QString::fromUtf8((const char*)data.payload, data.payload_len);
                return true;
            }
            quirc_flip(&code);
            if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
                out = QString::fromUtf8((const char*)data.payload, data.payload_len);
                return true;
            }
        }
        return false;
    };

    // —— ROI 轮询表：全图 / 中心 / 左 / 右 / 上 / 下 / 全图反色 / 中心60% / 四角 —— //
    struct ROI {
        int x, y, w, h;
        bool invert;
    };
    std::vector<ROI> sched;
    int PW = 0, PH = 0;

    PW = W;
    PH = H;
    sched.clear();
    auto push = [&](int x, int y, int w, int h, bool inv = false) { sched.push_back({x, y, w, h, inv}); };

    // 全图（最快命中）
    push(0, 0, W, H, false);

    // 中心 75%
    push(W / 8, H / 8, (W * 6) / 8, (H * 6) / 8, false);

    // 左/右 70% 宽（覆盖两侧边缘）
    int w70 = (W * 7) / 10;
    push(0, 0, w70, H, false);        // 左
    push(W - w70, 0, w70, H, false);  // 右

    // 上/下 70% 高（覆盖上下边缘）
    int h70 = (H * 7) / 10;
    push(0, 0, W, h70, false);        // 上
    push(0, H - h70, W, h70, false);  // 下

    // 全图反色兜底（黑底白码）
    push(0, 0, W, H, true);

    // // 中心 60%（再收紧一次，抗畸变）
    // push((W * 2) / 10, (H * 2) / 10, (W * 6) / 10, (H * 6) / 10, false);

    // 四角 60%（码正好在角落）
    // int cw = (W * 6) / 10, ch = (H * 6) / 10;
    // push(0, 0, cw, ch, false);            // 左上
    // push(W - cw, 0, cw, ch, false);       // 右上
    // push(0, H - ch, cw, ch, false);       // 左下
    // push(W - cw, H - ch, cw, ch, false);  // 右下

    // static uint32_t fno = 0;
    // const ROI& r = sched[fno++ % sched.size()];

    for (auto&& i : sched) {
        if (feedROI(i.x, i.y, i.w, i.h, i.invert) && decodeNow(outText))
            return true;
    }

    return false;
}
