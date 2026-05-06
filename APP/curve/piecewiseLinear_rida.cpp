#include "piecewiseLinear_rida.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace RIDA_ALGORITHM {

namespace {

static double round3(double v) {
    return std::round(v * 1000.0) / 1000.0;
}

static bool isStrictIncreasing(const std::vector<double>& v) {
    if (v.size() < 2) {
        return false;
    }

    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i] <= v[i - 1]) {
            return false;
        }
    }

    return true;
}

static bool valueInClosedRange(double v, double a, double b) {
    const double minV = (a < b) ? a : b;
    const double maxV = (a > b) ? a : b;
    const double eps = 1e-12;
    return v >= minV - eps && v <= maxV + eps;
}

}  // namespace

PiecewiseLinear::PiecewiseLinear()
    : Algorithm(Algorithm::Type::PiecewiseLinear),
      m_isSetPL(false),
      m_plN(0),
      m_plScale(100000.0),
      m_plKmin(0.0),
      m_plKmax(0.0),
      m_plXmin(0.0),
      m_plXmax(0.0),
      ostd(0.0) {}

PiecewiseLinear::~PiecewiseLinear() {}

bool PiecewiseLinear::isNearZero(double v) {
    return v <= 0.00000006 && v >= -0.00000006;
}

int PiecewiseLinear::calculate(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) {
        return 61;
    }

    this->_x.clear();
    this->_y.clear();

    for (size_t i = 0; i < x.size(); ++i) {
        this->_x.push_back(round3(x[i]));
        this->_y.push_back(round3(y[i]));
    }

    return buildFromXY(this->_x, this->_y);
}

int PiecewiseLinear::buildFromXY(const std::vector<double>& x, const std::vector<double>& y) {
    m_isSetPL = false;
    m_plN = 0;
    m_plK.clear();
    m_plY.clear();
    m_plSlope.clear();

    if (x.size() != y.size() || x.size() < 2) {
        return 61;
    }

    bool hasValidX = false;
    m_plXmin = 0.0;
    m_plXmax = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        double xv = x[i];
        double yv = y[i];

        // 与 CubicSpline 中 0 标准点的处理思路保持接近：
        // log 坐标不能接受 0，因此首点为 0 时转成 0.001。
        if (i == 0 && isNearZero(xv)) {
            xv = 0.001;
        }

        if (xv <= 0.0) {
            return 77;
        }

        const double k = std::log10(xv * m_plScale);
        m_plK.push_back(k);
        m_plY.push_back(yv);

        if (!hasValidX) {
            m_plXmin = xv;
            m_plXmax = xv;
            hasValidX = true;
        } else {
            if (xv < m_plXmin) {
                m_plXmin = xv;
            }
            if (xv > m_plXmax) {
                m_plXmax = xv;
            }
        }
    }

    if (!isStrictIncreasing(m_plK)) {
        return 74;
    }

    m_plN = static_cast<int>(m_plK.size()) - 1;
    if (m_plN < 1) {
        return 61;
    }

    m_plSlope.resize(m_plN);
    for (int i = 0; i < m_plN; ++i) {
        const double dk = m_plK[i + 1] - m_plK[i];
        if (dk <= 0.0) {
            return 77;
        }
        m_plSlope[i] = (m_plY[i + 1] - m_plY[i]) / dk;
    }

    m_plKmin = m_plK.front();
    m_plKmax = m_plK.back();
    ostd = m_plY.empty() ? 0.0 : m_plY.front();
    m_isSetPL = true;

    return 0;
}

int PiecewiseLinear::getSegmentFromK(double kin, int& seg) const {
    seg = 0;

    if (!m_isSetPL || m_plN <= 0) {
        return 79;
    }

    if (kin < m_plKmin) {
        return 81;
    }

    if (kin > m_plKmax) {
        seg = m_plN - 1;
        return 82;
    }

    if (kin >= m_plKmax) {
        seg = m_plN - 1;
        return 0;
    }

    std::vector<double>::const_iterator it = std::upper_bound(m_plK.begin(), m_plK.end(), kin);
    if (it == m_plK.begin()) {
        seg = 0;
    } else {
        seg = static_cast<int>((it - m_plK.begin()) - 1);
        if (seg >= m_plN) {
            seg = m_plN - 1;
        }
    }

    return 0;
}

// 注意：为了和 CubicSpline 保持一致，getY() 输入的是内部横坐标 K，
// 不是原始浓度 x。原始浓度应先转 K：K = log10(x * 100000.0)。
double PiecewiseLinear::getY(double Kin) {
    if (!m_isSetPL) {
        return 79.0;
    }

    int seg = 0;
    const int ret = getSegmentFromK(Kin, seg);
    if (ret == 79 || ret == 81) {
        return static_cast<double>(ret);
    }

    const double t = Kin - m_plK[seg];
    return m_plY[seg] + m_plSlope[seg] * t;
}

int PiecewiseLinear::getX(double Ein, double& Kout) {
    Kout = 0.0;

    if (!m_isSetPL) {
        return 79;
    }

    // 逐段查找响应值所在区间。
    // 这样比只根据首尾判断更稳，可以兼容局部非单调数据；
    // 但如果有多个区间都满足，只返回第一个。
    for (int i = 0; i < m_plN; ++i) {
        const double y0 = m_plY[i];
        const double y1 = m_plY[i + 1];

        if (!valueInClosedRange(Ein, y0, y1)) {
            continue;
        }

        const double dy = y1 - y0;
        double k = m_plK[i];

        if (std::fabs(dy) <= 1e-12) {
            // 水平段无法唯一反算；如果 Ein 正好在水平段上，返回左端点。
            k = m_plK[i];
        } else {
            const double ratio = (Ein - y0) / dy;
            k = m_plK[i] + ratio * (m_plK[i + 1] - m_plK[i]);
        }

        const double x = std::pow(10.0, k) / m_plScale;
        Kout = x;

        if (x < m_plXmin) {
            return 81;
        }
        if (x > m_plXmax) {
            return 82;
        }

        return 0;
    }

    // 没有任何区间包含该响应值，根据首尾趋势给出近似错误码。
    if (m_plY.size() >= 2) {
        const double firstY = m_plY.front();
        const double lastY = m_plY.back();

        if (firstY < lastY) {
            return (Ein < firstY) ? 81 : 82;
        } else if (firstY > lastY) {
            return (Ein > firstY) ? 81 : 82;
        }
    }

    return 80;
}

int PiecewiseLinear::getSamplesData(double m, double& data) {
    return getX(m, data);
}

int PiecewiseLinear::getCorrCoeff(std::vector<double>& x, std::vector<double>& y) {
    x = this->_x;
    y.clear();

    for (size_t i = 0; i < this->_y.size(); ++i) {
        double xTmp = 0.0;
        getX(this->_y[i], xTmp);
        y.push_back(xTmp);
    }

    return 0;
}

int PiecewiseLinear::getPlotPoint(std::vector<double>& x, std::vector<double>& y, std::vector<double>& x_origin) {
    x.clear();
    y.clear();
    x_origin.clear();

    if (ostd == 0.0) {
        return 80;
    }

    for (size_t i = 0; i < this->_x.size(); ++i) {
        if (this->_x[i] <= 0.0) {
            continue;
        }

        const double x_t = std::log10(this->_x[i] * m_plScale);
        const double y_t = this->_y[i] / ostd * 100.0;

        x.push_back(x_t);
        y.push_back(y_t);
        x_origin.push_back(this->_x[i]);
    }

    return 0;
}

int PiecewiseLinear::getPlotLine(std::vector<double>& x, std::vector<double>& y) {
    x.clear();
    y.clear();

    if (!m_isSetPL) {
        return 79;
    }
    if (ostd == 0.0) {
        return 80;
    }

    const int linePointNum = 100;
    const double minX = m_plXmin;
    const double maxX = m_plXmax;

    if (minX <= 0.0 || maxX <= minX) {
        return 80;
    }

    const double stepD = (maxX - minX) / static_cast<double>(linePointNum);
    for (int i = 0; i < linePointNum; ++i) {
        const double xRaw = minX + stepD * static_cast<double>(i);
        const double k = std::log10(xRaw * m_plScale);
        const double yRaw = getY(k);

        x.push_back(k);
        y.push_back(yRaw / ostd * 100.0);
    }

    return 0;
}

QString PiecewiseLinear::getParameterStr() const {
    QString p;
    Param param;
    const int ret = exportCurveParam(param);
    if (ret != 0) {
        p = QString("Error exporting parameters: %1").arg(ret);
    } else {
        std::string str = nlohmann::json(param).dump(4);
        p = QString::fromStdString(str);    
    }

    return p;
}

int PiecewiseLinear::exportCurveParam(PiecewiseLinear::Param& out) const {
    if (!m_isSetPL) {
        return 79;
    }

    PiecewiseLinear::Param param;
    param.n = m_plN;
    param.scale = m_plScale;
    param.k = m_plK;
    param.y = m_plY;
    param.slope = m_plSlope;
    param.xMin = m_plXmin;
    param.xMax = m_plXmax;

    out = param;
    return 0;
}

int PiecewiseLinear::importCurveParam(const PiecewiseLinear::Param& in) {
    if (in.n < 1) {
        return 79;
    }
    if (in.scale <= 0.0) {
        return 79;
    }
    if (static_cast<int>(in.k.size()) < in.n + 1 ||
        static_cast<int>(in.y.size()) < in.n + 1 ||
        static_cast<int>(in.slope.size()) < in.n) {
        return 79;
    }
    if (!isStrictIncreasing(in.k)) {
        return 74;
    }
    if (in.xMin <= 0.0 || in.xMax <= 0.0 || in.xMin > in.xMax) {
        return 79;
    }

    m_plN = in.n;
    m_plScale = in.scale;
    m_plK.assign(in.k.begin(), in.k.begin() + in.n + 1);
    m_plY.assign(in.y.begin(), in.y.begin() + in.n + 1);
    m_plSlope.assign(in.slope.begin(), in.slope.begin() + in.n);

    m_plKmin = m_plK.front();
    m_plKmax = m_plK.back();
    m_plXmin = in.xMin;
    m_plXmax = in.xMax;
    ostd = m_plY.empty() ? 0.0 : m_plY.front();

    this->_x.clear();
    this->_y.clear();

    for (size_t i = 0; i < m_plK.size(); ++i) {
        this->_x.push_back(std::pow(10.0, m_plK[i]) / m_plScale);
        this->_y.push_back(m_plY[i]);
    }

    m_isSetPL = true;
    return 0;
}

}  // namespace RIDA_ALGORITHM
