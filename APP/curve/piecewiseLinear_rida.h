#ifndef _PIECEWISELINEAR_RIDA_H_
#define _PIECEWISELINEAR_RIDA_H_

#include <QString>
#include <vector>

#include "algorithm.h"
#include "json.hpp"

namespace RIDA_ALGORITHM {
class PiecewiseLinear : public Algorithm {
public:
    struct Param {
        int n;                      // 区间数量
        double scale;               // 原代码固定使用 100000.0
        std::vector<double> k;      // K[0..n]
        std::vector<double> y;      // Y[0..n]
        std::vector<double> slope;  // slope[0..n-1]
        double xMin;                // 原始浓度最小有效值
        double xMax;                // 原始浓度最大有效值
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Param, n, scale, k, y, slope, xMin, xMax);
    };

private:
    bool m_isSetPL;

    int m_plN;         // 区间数量
    double m_plScale;  // 默认 100000.0
    double m_plKmin;
    double m_plKmax;
    double m_plXmin;
    double m_plXmax;

    std::vector<double> m_plK;      // K[0..n]
    std::vector<double> m_plY;      // Y[0..n]
    std::vector<double> m_plSlope;  // slope[0..n-1]

    double ostd;

public:
    PiecewiseLinear();
    ~PiecewiseLinear();

    double getY(double x) override;                                                      // 输入内部横坐标 K，返回曲线 Y
    int getX(double y, double& x) override;                                              // 输入 Y，反算原始浓度 x
    int getSamplesData(double m, double& data) override;                                 // 获取标准数据
    int calculate(const std::vector<double>& x, const std::vector<double>& y) override;  // 建模
    int getCorrCoeff(std::vector<double>& x, std::vector<double>& y) override;           // 获取拟合/反算对照数据
    int getPlotPoint(std::vector<double>& x, std::vector<double>& y, std::vector<double>& x_origin) override;
    int getPlotLine(std::vector<double>& x, std::vector<double>& y) override;
    QString getParameterStr() const override;

    int exportCurveParam(Param& out) const;
    int importCurveParam(const Param& in);

private:
    int buildFromXY(const std::vector<double>& x, const std::vector<double>& y);
    int getSegmentFromK(double kin, int& seg) const;
    static bool isNearZero(double v);
};

}  // namespace RIDA_ALGORITHM

#endif  // _PIECEWISELINEAR_RIDA_H_
