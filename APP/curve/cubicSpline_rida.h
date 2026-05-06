#ifndef _CUBICSPLINE_RIDA_H_
#define _CUBICSPLINE_RIDA_H_
#include <vector>

#include "algorithm.h"
#include "json.hpp"

namespace RIDA_ALGORITHM {

struct CubicSplineCurveParam {
    int n = 0;  // m_csN, 区间数量

    double scale = 100000.0;  // 原代码固定使用 100000.0

    std::vector<double> k;  // m_csK[0..n]
    std::vector<double> a;  // m_csKf0[0..n]
    std::vector<double> b;  // m_csKf1[0..n-1]
    std::vector<double> c;  // m_csKf2[0..n]
    std::vector<double> d;  // m_csKf3[0..n-1]

    double xMin = 0.0;  // 原始浓度最小有效值
    double xMax = 0.0;  // 原始浓度最大有效值
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CubicSplineCurveParam, n, scale, k, a, b, c, d, xMin, xMax);
};

class CubicSpline : public Algorithm {
private:
    double* m_csVrz;
    double* m_csVrr;
    double* m_csMxRb;
    double* m_csMxRa;
    double* m_csMxD;
    double* m_csKf3;
    double* m_csKf2;
    double* m_csKf1;
    double* m_csKf0;
    double* m_csY;
    double m_csLF3;
    double m_csLF2;
    double m_csLF1;
    double m_csRF3;
    double m_csRF2;
    double m_csRF1;
    double m_csZR;
    double m_csZLL;
    double m_csZL;
    double m_csZRR;
    double m_csNL;
    double m_csNR;
    double m_csW_1;
    double* m_csND2;
    double* m_csND1;
    double* m_csHD;
    bool m_isSetCSDiagM;
    double* m_csH;
    double* m_csh_1;
    double* m_csh;
    double m_csKmax;
    double m_csKmin;
    double* m_csE;
    double* m_csK;
    int m_csN;
    bool m_isSetDiff = false;
    bool m_isSetCSID = false;
    bool m_isSetWeight = false;
    bool m_isSetCS = false;
    double m_csDiffR;
    double m_csDiffL;
    double m_csWeight;
    std::vector<double> m_x;
    std::vector<double> m_y;

    double m_rrExS;
    double m_rrEx2S;
    double m_rrKoS;
    double m_rrKo2S;
    double m_rrExKoS;
    double m_rrN;
    double m_rrExMin;
    double m_rrExMax;
    double m_rrKoMin;
    double m_rrKoMax;
    double m_rrIsSet = true;
    double ostd = 0.0;

    double m_rrKoeffA;
    double m_rrKoeffB;

public:
    CubicSpline(/* args */);
    ~CubicSpline();
    double getY(double x) override;                                                      // 曲线y
    int getX(double y, double& x) override;                                              // 曲线x
    int getSamplesData(double m, double& data) override;                                 // 获取标准数据
    int calculate(const std::vector<double>& x, const std::vector<double>& y) override;  // logit-log
    int getCorrCoeff(std::vector<double>& x, std::vector<double>& y) override;           // 获取最小二乘输入数据
    int getPlotPoint(std::vector<double>& x, std::vector<double>& y, std::vector<double>& x_origin) override;
    int getPlotLine(std::vector<double>& x, std::vector<double>& y) override;
    QString getParameterStr() const override;

    int exportCurveParam(CubicSplineCurveParam& out) const;
    int importCurveParam(const CubicSplineCurveParam& in);

private:
    int CSGetNfrK(double Kin, int& NRes);
    int CSClcKff013();
    int CSClcMxRTDR();
    int CSClcME();
    int CSClcRE();
    int CSClcKF();
    int CSClcDiagM();
    int CSClcIvSS();
    int CSCheckMono();
    int CSLoadSS();
    int RC_GetCSKoeff(double Kin, double& a, double& b, double& c, double& d);
    int RC_SetCS();
    int AddVirtualStd();
    int RRClcSs(const std::vector<double>& x, const std::vector<double>& y, int algorithm);
    int RC_GetKoeffRegression(double& resA, double& resB);
};
}  // namespace RIDA_ALGORITHM
#endif  // _CUBICSPLINE_RIDA_H_