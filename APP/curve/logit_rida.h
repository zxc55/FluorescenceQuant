/*
 * @Author: joker
 * @Date: 2024-03-14 16:50:23
 * @LastEditors:
 * @LastEditTime: 2024-03-22 14:05:14
 * @Description: 请填写简介
 */
#ifndef _LOGIT_RIDA_H_
#define _LOGIT_RIDA_H_

#include <QVector>

#include "algorithm.h"
#include "json.hpp"
namespace RIDA_ALGORITHM {
class Logit : public Algorithm {
public:
    struct Parameter {
        double k;
        double b;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Parameter, k, b);
    };

private:
    double m_rrExS;
    double m_rrEx2S;
    double m_rrKoS;
    double m_rrExKoS;
    double m_rrN;
    double m_rrKoeffA;
    double m_rrKoeffB;
    int monotonie = 0;
    double ostd = 0.0;

public:
    Logit(/* args */);
    ~Logit();
    double getY(double x) override;                                                      // 曲线y
    int getX(double y, double& x) override;                                              // 曲线x
    int getSamplesData(double m, double& data) override;                                 // 获取标准数据
    double getSlope();                                                                   // 获取斜率
    int calculate(const std::vector<double>& x, const std::vector<double>& y) override;  // logit-log
    int getCorrCoeff(std::vector<double>& x, std::vector<double>& y) override;           // 获取最小二乘输入数据
    int getPlotPoint(std::vector<double>& x, std::vector<double>& y, std::vector<double>& x_origin) override;
    int getPlotLine(std::vector<double>& x, std::vector<double>& y) override;
    QString getParameterStr() const override;
    int importCurveParam(const Parameter& in);

private:
    int fitCurve(const std::vector<double>& x, const std::vector<double>& y);  // 最小二乘法
};
}  // namespace RIDA_ALGORITHM
#endif  // _LOGIT_RIDA_H_
