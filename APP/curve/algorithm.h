#ifndef _ALGORITHM_H_
#define _ALGORITHM_H_
#include <QObject>
#include <QString>
class Algorithm : public QObject {
    Q_OBJECT
public:
    enum Type {
        FourParameter,
        CubicSpline,
        Logit,
        PiecewiseLinear,
    };
    QString AlgorithmName = "";
    Type type;

public:
    Algorithm(Type type);
    virtual ~Algorithm();
    static QString getAlgorithmName(Type type);
    virtual double getY(double x) = 0;  // 曲线y
    // virtual double getX(double y) = 0;  // 曲线x
    virtual int getX(double y, double &x) = 0;  // 曲线x
    /*
    return:
        0：正常输出
        80：{0.00}*
        81：<{0.00}
        82: >{0.00}
    */
    virtual int getSamplesData(double m, double &data) = 0;                                 // 获取标准数据
    virtual int calculate(const std::vector<double> &x, const std::vector<double> &y) = 0;  // 预处理，形成曲线的数据
    virtual int getCorrCoeff(std::vector<double> &x, std::vector<double> &y) = 0;           // 获取曲线拟合的输入数据
    virtual int getPlotPoint(std::vector<double> &x, std::vector<double> &y, std::vector<double> &x_origin) = 0;
    virtual int getPlotLine(std::vector<double> &x, std::vector<double> &y) = 0;
    virtual QString getParameterStr() const  = 0; // 获取参数字符串

protected:
    double _stdData;
    std::vector<double> _x;
    std::vector<double> _y;
};

#endif  // _ALGORITHM_H_