#include "algorithm.h"

#include <QCoreApplication>
#include <QDebug>

Algorithm::Algorithm(Type type) : type(type) {
    switch (type) {
    case FourParameter:
        AlgorithmName = QCoreApplication::translate("Algorithm", "4-Parameter");
        break;
    case CubicSpline:
        AlgorithmName = QCoreApplication::translate("Algorithm", "Cubic Spline");
        break;
    case Logit:
        AlgorithmName = QCoreApplication::translate("Algorithm", "Logit/Log");
        break;
    case PiecewiseLinear:
        AlgorithmName = QCoreApplication::translate("Algorithm", "Piecewise Linear");
        break;
    default:
        break;
    }
}

Algorithm::~Algorithm() {
}

QString Algorithm::getAlgorithmName(Type type) {
    switch (type) {
    case FourParameter:
        return tr("4-Parameter");
    case CubicSpline:
        return tr("Cubic Spline");
    case Logit:
        return tr("Logit/Log");
    case PiecewiseLinear:
        return tr("Piecewise Linear");
    default:
        return "";
    }
}