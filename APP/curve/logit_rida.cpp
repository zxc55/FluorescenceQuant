#include "logit_rida.h"

// #include <Eigen/Core>
// #include <Eigen/LU>
#include <QObject>
#include <QString>
#include <QVector>
// #include <QwtMath>
#include <iostream>
namespace RIDA_ALGORITHM {
Logit::Logit() : Algorithm(Algorithm::Type::Logit) {
}

Logit::~Logit() {
}
// y = k*x + b
int Logit::fitCurve(const std::vector<double>& x, const std::vector<double>& y) {
    {
        double num2 = 0.0;  // y
        double num3 = 0.0;  // x
        double num4 = 0.0;
        double num5 = 0.0;
        double num6 = 0.0;
        double num8 = 0.0;
        int num9 = 0;
        int num10 = -1;
        double num12;
        double num13;
        double num14;
        double num15;
        double num18 = y[0];
        this->ostd = num18;
        for (size_t i = 0; i < x.size(); i++) {
            num2 = y[i];
            num3 = x[i];
            if (monotonie == 0 && num18 > num2 && num2 > 0.0) {
                double num19 = num2 / num18 * 100.0;
                num2 = std::log(num19 / (100.0 - num19));
            }
            if (num3 > 0.0) {
                num9++;
                num3 *= 100000.0;
                num3 = std::log10(num3);
                num4 += num2;
                num5 += num3;
                double num20 = num2;
                num6 = num20 * num20 + num6;
                double num21 = num3;
                num8 = num3 * num2 + num8;
            }
        }
        this->m_rrExS = num4;
        this->m_rrEx2S = num6;
        this->m_rrKoS = num5;
        this->m_rrExKoS = num8;
        this->m_rrN = num9;
    }
    int rrN = this->m_rrN;
    if (rrN == 0) {
        return 61;
    }
    double rrExS = this->m_rrExS;
    double num = (double)rrN;
    double rrEx2S = this->m_rrEx2S;
    double num2 = rrExS;
    double num3 = rrEx2S - num2 * num2 / num;
    if (num3 == 0.0) {
        return 61;
    }
    double rrKoS = m_rrKoS;
    double num4 = (m_rrExKoS - rrKoS * rrExS / num) / num3;
    double num5 = rrKoS / num - rrExS * num4 / num;
    this->m_rrKoeffA = num5;
    this->m_rrKoeffB = num4;
    return 0;
}
double Logit::getY(double x) {
    double z = std::log10(x * 100000.0);
    double y = (z - this->m_rrKoeffA) / this->m_rrKoeffB;
    return y;
}
int Logit::getX(double y, double& result) {
    if (y == 0.0) {
        double z = std::pow(10.0, this->m_rrKoeffA) / 100000.0;
        result = z;
        return 0;
    }

    double num7 = y / this->ostd * 100.0;
    double x = std::log(num7 / (100.0 - num7));
    y = this->m_rrKoeffB * x + this->m_rrKoeffA;
    double z = std::pow(10.0, y) / 100000.0;
    result = z;

    double mincalppd;
    double maxcalppd;
    bool initFlag = false;
    for (auto&& x : _x) {
        if (x <= 0.00000006 && x >= -0.00000006) {
            continue;  // 忽略小于等于0.0的值
        }

        if (!initFlag) {
            initFlag = true;
            mincalppd = x;
            maxcalppd = x;
        }
        if (x > maxcalppd) {
            maxcalppd = x;
        } else if (x < mincalppd) {
            mincalppd = x;
        }
    }

    if (result < mincalppd) {  // TODO
        return 81;             // <{0.00}
    } else if (result > maxcalppd) {
        return 82;  // >{0.00}
    }
    return 0;
}
int Logit::getSamplesData(double m, double& data) {
    return getX(m, data);
}
int Logit::calculate(const std::vector<double>& x, const std::vector<double>& y) {
    _x.clear();
    _y.clear();
    for (size_t i = 0; i < x.size(); ++i) {
        double x_rounded = std::round(x[i] * 1000.0) / 1000.0;
        double y_rounded = std::round(y[i] * 1000.0) / 1000.0;
        _x.push_back(x_rounded);
        _y.push_back(y_rounded);
    }
    if (x.size() <= 2)
        return 0;
    this->_x = _x;
    this->_y = _y;
    _stdData = _y[0];
    fitCurve(_x, _y);
    return 0;
}
double Logit::getSlope() {
    return 1 / this->m_rrKoeffB;
}
int Logit::getCorrCoeff(std::vector<double>& x, std::vector<double>& y) {
    for (int i = 0; i < _x.size(); i++) {
        if (_x[i] <= 0.0) {
            continue;
        }
        double x_t = std::log10(_x[i] * 100000.0);
        double y_t = _y[i] / _stdData;
        y_t = std::log(y_t / (1 - y_t));
        x.push_back(x_t);
        y.push_back(y_t);
    }
    return 0;
}
int Logit::getPlotPoint(std::vector<double>& x, std::vector<double>& y, std::vector<double>& x_origin) {
    for (size_t i = 0; i < _x.size(); i++) {
        if (_x[i] > 0.0) {
            double x_t = std::log10(_x[i] * 100000.0);
            double y_t = _y[i];
            if (y_t == 0.0) {
                y_t = std::pow(10.0, this->m_rrKoeffA) / 100000.0;
            }
            double num7 = y_t / this->ostd * 100.0;
            y_t = std::log(num7 / (100.0 - num7));
            x.push_back(x_t);
            y.push_back(y_t);
            x_origin.push_back(_x[i]);
        }
    }
    return 0;
}
int Logit::getPlotLine(std::vector<double>& x, std::vector<double>& y) {
    int linePointNum = 100;
    double maxX = _x[0];
    double minX = _x[0];
    for (size_t i = 0; i < _x.size(); i++) {
        if (_x[i] > maxX) {
            maxX = _x[i];
        }
        if (minX <= 0.0) {
            minX = _x[i];
        }

        if (_x[i] < minX && _x[i] > 0.0) {
            minX = _x[i];
        }
    }

    double stepD = (maxX - minX) / linePointNum;
    for (size_t i = 0; i < linePointNum; i++) {
        double x_t = minX + stepD * i;
        double y_t = this->getY(x_t);
        x_t = std::log10(x_t * 100000.0);
        x.push_back(x_t);
        y.push_back(y_t);
    }
    return 0;
}
QString Logit::getParameterStr() const {
    Parameter p;
    p.k = this->m_rrKoeffB;
    p.b = this->m_rrKoeffA;
    std::string str = nlohmann::json(p).dump(4);
    return QString::fromStdString(str);
}

int Logit::importCurveParam(const Parameter& in) {
    Parameter p = in;
    this->m_rrKoeffB = p.k;
    this->m_rrKoeffA = p.b;
    this->ostd = 1.0;
    this->_x.clear();
    this->_x.push_back(0.000001);
    this->_x.push_back(1000000000.0);
    return 0;
}
}  // namespace RIDA_ALGORITHM
