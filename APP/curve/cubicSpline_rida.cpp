#include "cubicSpline_rida.h"
#include <qlist.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
namespace RIDA_ALGORITHM {

namespace {

static int calcEffectiveXRange(const std::vector<double>& x, double& xMin, double& xMax) {
    bool inited = false;

    for (size_t i = 0; i < x.size(); ++i) {
        const double v = x[i];

        // 与原 getX() 中逻辑保持一致：忽略接近 0 的点。
        if (v <= 0.00000006 && v >= -0.00000006) {
            continue;
        }
        if (v <= 0.0) {
            continue;
        }

        if (!inited) {
            xMin = v;
            xMax = v;
            inited = true;
        } else {
            if (v < xMin) {
                xMin = v;
            }
            if (v > xMax) {
                xMax = v;
            }
        }
    }

    return inited ? 0 : 81;
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

}  // namespace

CubicSpline::CubicSpline(/* args */) : Algorithm(Algorithm::Type::CubicSpline) {
    this->m_isSetCS = false;
    this->m_csWeight = 10000;
    this->m_isSetWeight = true;
    this->m_isSetCSID = true;
    this->m_csDiffL = 0.0;
    this->m_csDiffR = 0.0;
    this->m_isSetDiff = true;
    double* array = new double[20];
    this->m_csVrz = array;
    double* array2 = new double[20];
    this->m_csVrr = array2;
    double* array3 = new double[20];
    this->m_csMxRb = array3;
    double* array4 = new double[20];
    this->m_csMxRa = array4;
    double* array5 = new double[20];
    this->m_csMxD = array5;
    double* array6 = new double[20];
    this->m_csKf3 = array6;
    double* array7 = new double[20];
    this->m_csKf2 = array7;
    double* array8 = new double[20];
    this->m_csKf1 = array8;
    double* array9 = new double[20];
    this->m_csKf0 = array9;
    double* array10 = new double[20];
    this->m_csY = array10;
    double* array11 = new double[20];
    this->m_csND2 = array11;
    double* array12 = new double[20];
    this->m_csND1 = array12;
    double* array13 = new double[20];
    this->m_csHD = array13;
    this->m_csH = new double[20];
    double* array14 = new double[20];
    this->m_csh_1 = array14;
    double* array15 = new double[20];
    this->m_csh = array15;
    double* array16 = new double[20];
    this->m_csE = array16;
    double* array17 = new double[20];
    this->m_csK = array17;
    int num = 0;
    do {
        array16[num] = -999.0;
        array15[num] = -999.0;
        array14[num] = -999.0;
        array13[num] = -999.0;
        array17[num] = -999.0;
        array9[num] = -999.0;
        array8[num] = -999.0;
        array7[num] = -999.0;
        array6[num] = -999.0;
        array5[num] = -999.0;
        array4[num] = -999.0;
        array3[num] = -999.0;
        array12[num] = -999.0;
        array11[num] = -999.0;
        array2[num] = -999.0;
        array[num] = -999.0;
        array10[num] = -999.0;
        num++;
    } while (num < 20);
}

CubicSpline::~CubicSpline() {
    delete[] this->m_csVrz;
    delete[] this->m_csVrr;
    delete[] this->m_csMxRb;
    delete[] this->m_csMxRa;
    delete[] this->m_csMxD;
    delete[] this->m_csKf3;
    delete[] this->m_csKf2;
    delete[] this->m_csKf1;
    delete[] this->m_csKf0;
    delete[] this->m_csY;
    delete[] this->m_csND2;
    delete[] this->m_csND1;
    delete[] this->m_csHD;
    delete[] this->m_csH;
    delete[] this->m_csh_1;
    delete[] this->m_csh;
    delete[] this->m_csE;
    delete[] this->m_csK;
}

int CubicSpline::getSamplesData(double m, double& data) {
    return getX(m, data);
}
int CubicSpline::getCorrCoeff(std::vector<double>& x, std::vector<double>& y) {
    x = _x;
    for (size_t i = 0; i < _x.size(); i++) {
        double xTmp;
        getX(_y.at(i), xTmp);
        y.push_back(xTmp);
    }
    return 0;
}
int CubicSpline::CSGetNfrK(double Kin, int& NRes) {
    int num = 0;
    double num2 = Kin;
    int num3;
    if (num2 < this->m_csKmin) {
        num3 = 1;
    } else {
        num3 = 0;
    }
    num = (((((num2 <= this->m_csKmax) ? 0 : 1) | num3) != 0) ? 82 : num);
    double* csK = this->m_csK;
    int num4;
    if (num2 < csK[0]) {
        num4 = 1;
    } else {
        num4 = 0;
    }
    int csN = this->m_csN;
    if ((((num2 > csK[csN]) ? 1 : 0) | num4) != 0) {
        return 81;
    }
    int num5 = 0;
    if (0 <= csN) {
        do {
            if (Kin >= this->m_csK[num5]) {
                NRes = num5;
            }
            num5++;
        } while (num5 <= this->m_csN);
    }
    return num;
}

int CubicSpline::CSClcKff013() {
    double num = this->m_csh[0];
    double num2 = num;
    double num3 = num2 * num2;
    double num4 = num3 * 2.0;
    double* csh_ = this->m_csh_1;
    double csW_ = this->m_csW_1;
    double num5 = csh_[0];
    double num6 = csW_ * 2.0 * num5;
    double num7 = num6 + num4;
    if (num7 == 0.0) {
        return 80;
    }
    double* csKf = this->m_csKf2;
    double* csE = this->m_csE;
    csKf[0] = ((this->m_csH[0] * csW_ + num5 * csW_ - num3) * csKf[1] + (csE[1] - csE[0] - this->m_csDiffL * num) * 3.0 - csh_[1] * csW_ * csKf[2]) / num7;
    int csN = this->m_csN;
    double num8 = this->m_csh[csN - 1];
    double num9 = num8;
    double num10 = num9 * num9;
    num4 = num10 * 2.0;
    csh_ = this->m_csh_1;
    csW_ = this->m_csW_1;
    double num11 = csh_[csN - 1];
    num6 = csW_ * 2.0 * num11;
    num7 = num6 + num4;
    if (num7 == 0.0) {
        return 80;
    }
    int num12 = csN - 2;
    csKf = this->m_csKf2;
    csE = this->m_csE;
    csKf[csN] = ((this->m_csH[num12] * csW_ + num11 * csW_ - num10) * csKf[csN - 1] + (this->m_csDiffR * num8 - (csE[csN] - csE[csN - 1])) * 3.0 - csh_[num12] * csW_ * csKf[num12]) / num7;
    double csWeight = this->m_csWeight;
    if (csWeight == 0.0) {
        return 80;
    }
    csKf = this->m_csKf2;
    this->m_csKf0[0] = this->m_csh_1[0] * 2.0 / csWeight * (csKf[0] - csKf[1]) + this->m_csE[0];
    int num13 = 1;
    if (1 < csN) {
        do {
            csKf = this->m_csKf2;
            csh_ = this->m_csh_1;
            int num14 = num13 - 1;
            this->m_csKf0[num13] = this->m_csE[num13] - (csKf[num14] * csh_[num14] - this->m_csH[num14] * csKf[num13] + csKf[num14 + 2] * csh_[num13]) * (2.0 / this->m_csWeight);
            num13++;
        } while (num13 < csN);
    }
    csKf = this->m_csKf2;
    this->m_csKf0[csN] = this->m_csE[csN] - this->m_csh_1[csN - 1] * 2.0 / this->m_csWeight * (csKf[csN - 1] - csKf[csN]);
    int num15 = 0;
    if (0 < csN) {
        do {
            double* csKf2 = this->m_csKf0;
            int num16 = num15 + 1;
            csKf = this->m_csKf2;
            this->m_csKf1[num15] = this->m_csh_1[num15] * (csKf2[num16] - csKf2[num15]) - this->m_csh[num15] / 3.0 * (csKf[num15] * 2.0 + csKf[num16]);
            num15 = num16;
        } while (num15 < csN);
    }
    int num17 = 0;
    if (0 < csN) {
        do {
            csKf = this->m_csKf2;
            int num18 = num17 + 1;
            this->m_csKf3[num17] = this->m_csh_1[num17] / 3.0 * (csKf[num18] - csKf[num17]);
            num17 = num18;
        } while (num17 < csN);
    }
    return 0;
}

int CubicSpline::CSClcMxRTDR() {
    this->m_csMxD[1] = this->m_csHD[1];
    this->m_csMxRa[1] = this->m_csND1[1] / this->m_csMxD[1];
    this->m_csMxRb[1] = this->m_csND2[1] / this->m_csMxD[1];
    this->m_csMxD[2] = this->m_csHD[2] - this->m_csND1[1] * this->m_csMxRa[1];
    double* csMxRa = this->m_csMxRa;
    csMxRa[2] = (this->m_csND1[2] - this->m_csND2[1] * csMxRa[1]) / this->m_csMxD[2];
    this->m_csMxRb[2] = this->m_csND2[2] / this->m_csMxD[2];
    int num = 3;
    if (3 < this->m_csN) {
        do {
            int num2 = num - 1;
            int num3 = num2 - 1;
            double* csMxD = this->m_csMxD;
            double num4 = this->m_csMxRa[num2];
            csMxD[num] = this->m_csHD[num] - this->m_csND2[num3] * this->m_csMxRb[num3] - csMxD[num2] * num4 * num4;
            if (num < this->m_csN - 1) {
                csMxRa = this->m_csMxRa;
                csMxRa[num] = (this->m_csND1[num] - this->m_csND2[num2] * csMxRa[num2]) / this->m_csMxD[num];
            }
            if (num < this->m_csN - 2) {
                this->m_csMxRb[num] = this->m_csND2[num] / this->m_csMxD[num];
            }
            num++;
        } while (num < this->m_csN);
    }
    this->m_csVrz[1] = this->m_csY[1];
    double* csVrz = this->m_csVrz;
    csVrz[2] = this->m_csY[2] - this->m_csMxRa[1] * csVrz[1];
    if (3 < this->m_csN) {
        int num5 = 1;
        do {
            csVrz = this->m_csVrz;
            int num6 = num5 + 1;
            int num7 = num5 + 2;
            csVrz[num7] = this->m_csY[num7] - this->m_csMxRa[num6] * csVrz[num6] - this->m_csMxRb[num5] * csVrz[num5];
            num5 = num6;
        } while (num5 + 2 < this->m_csN);
    }
    int num8 = 1;
    if (1 < this->m_csN) {
        do {
            this->m_csVrr[num8] = this->m_csVrz[num8] / this->m_csMxD[num8];
            num8++;
        } while (num8 < this->m_csN);
    }
    int csN = this->m_csN;
    int num9 = csN - 1;
    int num10 = csN - 3;
    this->m_csKf2[num9] = this->m_csVrr[num9];
    int num11 = num9 - 1;
    double* csKf = this->m_csKf2;
    csKf[num11] = this->m_csVrr[num11] - this->m_csMxRa[num11] * csKf[num9];
    int num12 = num10;
    if (num10 > 0) {
        do {
            csKf = this->m_csKf2;
            int num13 = num12 + 2;
            csKf[num12] = this->m_csVrr[num12] - this->m_csMxRa[num12] * csKf[num13 - 1] - this->m_csMxRb[num12] * csKf[num13];
            num12--;
        } while (num12 > 0);
    }
    return 0;
}

int CubicSpline::CSClcME() {
    double* csE = this->m_csE;
    double num = this->m_csh_1[0] * (csE[1] - csE[0]);
    int num2 = 1;
    if (1 < this->m_csN) {
        do {
            csE = this->m_csE;
            int num3 = num2 + 1;
            double num4 = this->m_csh_1[num2] * (csE[num3] - csE[num2]);
            this->m_csY[num2] = (num4 - num) * 3.0;
            num = num4;
            num2 = num3;
        } while (num2 < this->m_csN);
    }
    double* csY = this->m_csY;
    csY[1] = csY[1] - this->m_csZLL / this->m_csNL * this->m_csLF3;
    double* csY2 = this->m_csY;
    csY2[2] = csY2[2] - this->m_csZL / this->m_csNL * this->m_csLF3;
    int num5 = this->m_csN - 2;
    double* csY3 = this->m_csY;
    csY3[num5] -= this->m_csZR / this->m_csNR * this->m_csRF3;
    int num6 = this->m_csN - 1;
    double* csY4 = this->m_csY;
    csY4[num6] -= this->m_csZRR / this->m_csNR * this->m_csRF3;
    return 0;
}

int CubicSpline::CSClcRE() {
    double* csHD = this->m_csHD;
    csHD[1] = this->m_csZLL / this->m_csNL * this->m_csLF1 + csHD[1];
    double* csND = this->m_csND1;
    csND[1] = csND[1] - this->m_csZLL / this->m_csNL * this->m_csLF2;
    double* csHD2 = this->m_csHD;
    csHD2[2] = csHD2[2] - this->m_csZL / this->m_csNL * this->m_csLF2;
    int num = this->m_csN - 2;
    double* csHD3 = this->m_csHD;
    csHD3[num] -= this->m_csZR / this->m_csNR * this->m_csRF1;
    int num2 = this->m_csN - 2;
    double* csND2 = this->m_csND1;
    csND2[num2] = this->m_csZR / this->m_csNR * this->m_csRF2 + csND2[num2];
    int num3 = this->m_csN - 1;
    double* csHD4 = this->m_csHD;
    csHD4[num3] = this->m_csZRR / this->m_csNR * this->m_csRF2 + csHD4[num3];
    return 0;
}

int CubicSpline::CSClcKF() {
    double* csh = this->m_csh;
    double* csh_ = this->m_csh_1;
    double num = csh_[0];
    double csW_ = this->m_csW_1;
    double num2 = csW_ * num;
    double* csH = this->m_csH;
    this->m_csZLL = csh[0] - num2 * num - csH[0] * num2;
    this->m_csZL = csh_[0] * csW_ * csh_[1];
    double num3 = csh[0];
    this->m_csNL = csh_[0] * 2.0 * csW_ + num3 * 2.0 * num3;
    int csN = this->m_csN;
    this->m_csZR = csh_[csN - 2] * csW_ * csh_[csN - 1];
    double num4 = csh_[csN - 1];
    double num5 = num4 * csW_;
    this->m_csZRR = csh[csN - 1] - csH[csN - 2] * num5 - num5 * num4;
    double num6 = csh[csN - 1];
    this->m_csNR = csh_[csN - 1] * 2.0 * csW_ + num6 * 2.0 * num6;
    num3 = csh[0];
    double num7 = csH[0] * csW_ + csh_[0] * csW_;
    double num8 = num3;
    this->m_csLF1 = num7 - num8 * num8;
    this->m_csLF2 = csh_[1] * csW_;
    double* csE = this->m_csE;
    this->m_csLF3 = (csE[1] - csE[0] - csh[0] * this->m_csDiffL) * 3.0;
    this->m_csRF1 = csh_[csN - 2] * csW_;
    num6 = csh[csN - 1];
    double num9 = csH[csN - 2] * csW_ + csh_[csN - 1] * csW_;
    double num10 = num6;
    this->m_csRF2 = num9 - num10 * num10;
    this->m_csRF3 = (csh[csN - 1] * this->m_csDiffR - (csE[csN] - csE[csN - 1])) * 3.0;
    return 0;
}

int CubicSpline::CSClcDiagM() {
    if (!this->m_isSetWeight) {
        return 71;
    }
    this->m_csW_1 = 6.0 / this->m_csWeight;
    if (!this->m_isSetCSID) {
        return 75;
    }
    int num = 1;
    if (1 < this->m_csN) {
        do {
            double* csh_ = this->m_csh_1;
            int num2 = num - 1;
            double num3 = csh_[num2];
            double csW_ = this->m_csW_1;
            double* csh = this->m_csh;
            double num4 = this->m_csH[num2];
            double num5 = csh_[num];
            this->m_csHD[num] = (csh[num] + csh[num2]) * 2.0 + csW_ * num3 * num3 + num4 * csW_ * num4 + num5 * csW_ * num5;
            num++;
        } while (num < this->m_csN);
    }
    num = 1;
    if (1 < this->m_csN - 1) {
        do {
            double num6 = this->m_csh_1[num] * this->m_csW_1;
            double* csH = this->m_csH;
            this->m_csND1[num] = this->m_csh[num] - (csH[num - 1] * num6 + csH[num] * num6);
            num++;
        } while (num < this->m_csN - 1);
    }
    num = 1;
    if (1 < this->m_csN - 2) {
        do {
            double* csh_2 = this->m_csh_1;
            int num7 = num + 1;
            this->m_csND2[num] = csh_2[num] * this->m_csW_1 * csh_2[num7];
            num = num7;
        } while (num < this->m_csN - 2);
    }
    this->m_isSetCSDiagM = true;
    return 0;
}

int CubicSpline::CSClcIvSS() {
    if (!this->m_isSetCSID) {
        return 75;
    }
    int csN = this->m_csN;
    int num = (csN < 4) ? 1 : 0;
    if ((((csN > 20) ? 1 : 0) | num) != 0) {
        return 76;
    }
    int num2 = 0;
    if (0 < csN) {
        do {
            double* csK = this->m_csK;
            int num3 = num2 + 1;
            this->m_csh[num2] = csK[num3] - csK[num2];
            double num4 = this->m_csh[num2];
            if (num4 <= 0.0) {
                return 77;
            }
            this->m_csh_1[num2] = 1.0 / num4;
            num2 = num3;
        } while (num2 < this->m_csN);
    }
    int num5 = 0;
    if (0 < this->m_csN - 1) {
        do {
            double* csh_ = this->m_csh_1;
            int num6 = num5 + 1;
            this->m_csH[num5] = csh_[num6] + csh_[num5];
            num5 = num6;
        } while (num5 < this->m_csN - 1);
    }
    return 0;
}

int CubicSpline::CSCheckMono() {
    int csN = this->m_csN;
    if (csN < 5) {
        return 73;
    }
    double* csK = this->m_csK;
    double num = csK[0];
    double num2 = num;
    int num3 = 1;
    if (1 <= csN) {
        do {
            double num4 = csK[num3];
            if (num2 >= num4) {
                return 74;
            }
            num2 = num4;
            num3++;
        } while (num3 <= csN);
    }
    this->m_csKmin = num;
    this->m_csKmax = csK[csN];
    return 0;
}

int CubicSpline::CSLoadSS() {
    int num = 0;
    for (size_t i = 0; i < this->m_x.size(); i++) {
        double num3 = this->m_x[i];
        if (num3 > 0.0) {
            num3 *= 100000.0;
            num3 = std::log10(num3);
        }
        this->m_csE[num] = this->m_y[i];
        this->m_csK[num] = num3;
        num++;
    }
    this->m_csN = num - 1;
    return 0;
}

// int CubicSpline::getX(double Ein, double& Kout) {
//     double num = this->m_csKmin;
//     double num2 = this->m_csKmax;
//     double num3;
//     num3 = this->getY(num);
//     double num4;
//     num4 = this->getY(num2);
//     bool flag;
//     if (num3 < num4) {
//         flag = true;
//         double num5 = Ein;
//         if (num3 > num5) {
//             return 81;
//         }
//         if (num5 > num4) {
//             return 82;
//         }
//     } else {
//         flag = false;
//         double num5 = Ein;
//         if (num3 < num5) {
//             return 81;
//         }
//         if (num5 < num4) {
//             return 82;
//         }
//     }
//     int num6 = 0;
//     double num7 = m_x[0];

//     double* csK;
//     if (num7 == 0.001) {
//         csK = this->m_csK;
//         num = csK[1];
//     } else {
//         csK = this->m_csK;
//         num = csK[0];
//     }
//     num2 = csK[this->m_csN - 1];
//     num3 = this->getY(num);
//     num4 = this->getY(num2);
//     if (num3 < num4) {
//         double num5 = Ein;
//         int num8;
//         if (num3 > num5) {
//             num8 = 1;
//         } else {
//             num8 = 0;
//         }
//         if ((((num5 > num4) ? 1 : 0) | num8) != 0) {
//             num6 = 80;
//         }
//     } else {
//         double num5 = Ein;
//         int num9;
//         if (num3 < num5) {
//             num9 = 1;
//         } else {
//             num9 = 0;
//         }
//         num6 = (((((num5 >= num4) ? 0 : 1) | num9) != 0) ? 80 : num6);
//     }
//     num = this->m_csKmin;
//     num2 = this->m_csKmax;
//     uint16_t num10 = 33U;
//     do {
//         num7 = (num2 - num) * 0.5 + num;
//         double num11;
//         num11 = this->getY(num7);
//         if (flag) {
//             if (Ein < num11) {
//                 num2 = num7;
//             } else {
//                 num = num7;
//             }
//         } else if (Ein < num11) {
//             num = num7;
//         } else {
//             num2 = num7;
//         }
//         num10 -= 1U;
//     } while (num10 > 0U);
//     Kout = std::pow(10.0, num7) / 100000.0;
//     return num6;
// }
int CubicSpline::getX(double Ein, double& Kout) {
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
    double num = this->m_csKmin;
    double num2 = this->m_csKmax;
    double num3;
    num3 = this->getY(num);
    double num4;
    num4 = this->getY(num2);
    bool flag;
    if (num3 < num4) {
        flag = true;
        double num5 = Ein;
        if (num3 > num5) {
            return 81;
        }
        if (num5 > num4) {
            return 82;
        }
    } else {
        flag = false;
        double num5 = Ein;
        if (num3 < num5) {
            return 81;
        }
        if (num5 < num4) {
            return 82;
        }
    }
    int num6 = 0;
    double num7 = m_x[0];

    double* csK;
    if (num7 == 0.001) {
        csK = this->m_csK;
        num = csK[1];
    } else {
        csK = this->m_csK;
        num = csK[0];
    }
    num2 = csK[this->m_csN - 1];
    num3 = this->getY(num);
    num4 = this->getY(num2);
    if (num3 < num4) {
        double num5 = Ein;
        int num8;
        if (num3 > num5) {
            num8 = 1;
        } else {
            num8 = 0;
        }
        if ((((num5 > num4) ? 1 : 0) | num8) != 0) {
            num6 = 80;
        }
    } else {
        double num5 = Ein;
        int num9;
        if (num3 < num5) {
            num9 = 1;
        } else {
            num9 = 0;
        }
        num6 = (((((num5 >= num4) ? 0 : 1) | num9) != 0) ? 80 : num6);
    }
    num = this->m_csKmin;
    num2 = this->m_csKmax;
    uint16_t num10 = 33U;
    do {
        num7 = (num2 - num) * 0.5 + num;
        double num11;
        num11 = this->getY(num7);
        if (flag) {
            if (Ein < num11) {
                num2 = num7;
            } else {
                num = num7;
            }
        } else if (Ein < num11) {
            num = num7;
        } else {
            num2 = num7;
        }
        num10 -= 1U;
    } while (num10 > 0U);
    Kout = std::pow(10.0, num7) / 100000.0;

    if (Kout < mincalppd) {  // TODO
        return 81;           // <{0.00}
    } else if (Kout > maxcalppd) {
        return 82;  // >{0.00}
    }
    return num6;
}

int CubicSpline::RC_GetCSKoeff(double Kin, double& a, double& b, double& c, double& d) {
    if (!this->m_isSetCS) {
        return 79;
    }
    int num2;
    int num = this->CSGetNfrK(Kin, num2);
    if (num == 0) {
        a = this->m_csKf0[num2];
        b = this->m_csKf1[num2];
        c = this->m_csKf2[num2];
        d = this->m_csKf3[num2];
    }
    return num;
}

double CubicSpline::getY(double Kin) {
    double Eout = 0.0;
    if (!this->m_isSetCS) {
        return 79;
    }
    int num2;
    int num = this->CSGetNfrK(Kin, num2);
    int num3;
    int num4;
    if (num == 0) {
        num3 = 1;
    } else {
        num3 = 0;
        if (num == 82) {
            num4 = 1;
            goto IL_2B;
        }
    }
    num4 = 0;
IL_2B:
    if ((num4 | num3) != 0) {
        double num5 = Kin - this->m_csK[num2];
        Eout = this->m_csKf1[num2] * num5 + this->m_csKf0[num2] + this->m_csKf2[num2] * num5 * num5 + this->m_csKf3[num2] * num5 * num5 * num5;
    }
    return Eout;
}

int CubicSpline::RC_SetCS() {
    if (!this->m_isSetDiff) {
        return 72;
    }
    if (!this->m_isSetWeight) {
        return 71;
    }
    if (!this->m_isSetCSID) {
        return 75;
    }
    if (this->m_isSetCS) {
        return 78;
    }
    int num = this->CSClcDiagM();
    if (num != 0) {
        return num;
    }
    num = this->CSClcKF();
    if (num != 0) {
        return num;
    }
    num = this->CSClcRE();
    if (num != 0) {
        return num;
    }
    num = this->CSClcME();
    if (num != 0) {
        return num;
    }
    num = this->CSClcMxRTDR();
    if (num != 0) {
        return num;
    }

    num = this->CSClcKff013();

    if (num != 0) {
        return num;
    }
    this->m_isSetCS = true;
    return 0;
}

int CubicSpline::AddVirtualStd() {
    if (m_x[0] == 0.0) {
        m_x[0] = 0.001;
    }
    RRClcSs(this->m_x, this->m_y, 0);

    double resA;
    double resB;
    if (RC_GetKoeffRegression(resA, resB) == 61) {
        return 61;
    }
    double num = this->m_x[0];
    double num2 = this->m_x[1];
    double num3 = num2 / num;
    num2 = this->m_x.back();
    double num4 = num3 * num2 * 100000.0;
    num4 = std::log10(num4);
    double mw = (num4 - resA) / resB;
    this->m_x.push_back(num3 * num2);
    this->m_y.push_back(mw);
    for (auto&& i : this->m_y) {
        i = std::round(i * 1000.0) / 1000.0;
    }

    return 0;
}

int CubicSpline::calculate(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 5) {
        return 61;
    }
    _x.clear();
    _y.clear();
    for (size_t i = 0; i < x.size(); ++i) {
        double x_rounded = std::round(x[i] * 1000.0) / 1000.0;
        double y_rounded = std::round(y[i] * 1000.0) / 1000.0;
        _x.push_back(x_rounded);
        _y.push_back(y_rounded);
    }
    this->_x = _x;
    this->_y = _y;
    this->m_x = _x;
    this->m_y = _y;
#define EXECS(x)      \
    {                 \
        int f = x;    \
        if (f != 0)   \
            return f; \
    }
    EXECS(this->AddVirtualStd());
    EXECS(this->CSLoadSS());
    EXECS(this->CSCheckMono());
    EXECS(this->CSClcIvSS());
    EXECS(this->RC_SetCS());
    return 0;
}
int CubicSpline::RRClcSs(const std::vector<double>& x, const std::vector<double>& y, int algorithm) {
    double num2 = 0.0;
    double num3 = 0.0;
    double num4 = 0.0;
    double num5 = 0.0;
    double num6 = 0.0;
    double num7 = 0.0;
    double num8 = 0.0;
    int num9 = 0;
    int num10 = -1;
    double num12;
    double num13;
    double num14;
    double num15;
    double num18 = y.at(0);
    this->ostd = num18;
    for (size_t i = 0; i < x.size(); i++) {
        num10++;
        num2 = y[i];
        num3 = x[i];
        if (algorithm == 0) {
            if (num2 > 0.0) {
                if (num3 > 0.0) {
                    num3 *= 100000.0;
                    num3 = std::log10(num3);
                }
                num9++;
                if (num9 == 1) {
                    num12 = num2;
                    num13 = num2;
                    num14 = num3;
                    num15 = num3;
                } else {
                    if (num12 > num2) {
                        num12 = num2;
                    }
                    if (num13 < num2) {
                        num13 = num2;
                    }
                    if (num14 > num3) {
                        num14 = num3;
                    }
                    if (num15 < num3) {
                        num15 = num3;
                    }
                }
                num4 += num2;
                num5 += num3;
                double num16 = num2;
                num6 = num16 * num16 + num6;
                double num17 = num3;
                num7 = num17 * num17 + num7;
                num8 = num3 * num2 + num8;
            }
        } else {
            if (algorithm == 8) {
                if (num10 == 0) {
                    num18 = num2;
                }
                if (num18 > num2 && num2 > 0.0) {
                    double num19 = num2 / num18 * 100.0;
                    num2 = std::log(num19 / (100.0 - num19));
                }
            }
            if (num3 > 0.0 && algorithm == 8) {
                num9++;
                num3 *= 100000.0;
                num3 = std::log10(num3);
                if (num9 == 1) {
                    num12 = num2;
                    num13 = num2;
                    num14 = num3;
                    num15 = num3;
                } else {
                    if (num12 > num2) {
                        num12 = num2;
                    }
                    if (num13 < num2) {
                        num13 = num2;
                    }
                    if (num14 > num3) {
                        num14 = num3;
                    }
                    if (num15 < num3) {
                        num15 = num3;
                    }
                }
                num4 += num2;
                num5 += num3;
                double num20 = num2;
                num6 = num20 * num20 + num6;
                double num21 = num3;
                num7 = num21 * num21 + num7;
                num8 = num3 * num2 + num8;
                continue;
            } else if (algorithm == 1) {
                // if ((methodNo == "Elastase" || methodNo == "M2PK") && num3 > 0.0 && num2 > 0.0) {
                //     num3 *= 100000.0;
                //     num3 = std::log10(num3);
                //     num2 *= 100000.0;
                //     num2 = std::log10(num2);
                // }
                // num9++;
                //     if ((methodNo != "dhit" && num9 == 1) || (methodNo == "dhit" && num9 == 2)) {
                //         num12 = num2;
                //         num13 = num2;
                //         num14 = num3;
                //         num15 = num3;
                //     } else {
                //         if (num12 > num2) {
                //             num12 = num2;
                //         }
                //         if (num13 < num2) {
                //             num13 = num2;
                //         }
                //         if (num14 > num3) {
                //             num14 = num3;
                //         }
                //         if (num15 < num3) {
                //             num15 = num3;
                //         }
                //     num4 += num2;
                //     num5 += num3;
                //     double num22 = num2;
                //     num6 = num22 * num22 + num6;
                //     double num23 = num3;
                //     num7 = num23 * num23 + num7;
                //     num8 = num3 * num2 + num8;
                //     continue;
            }
        }
    }
    if (num9 < 2) {
        return 2;
    }
    this->m_rrExS = num4;
    this->m_rrEx2S = num6;
    this->m_rrKoS = num5;
    this->m_rrKo2S = num7;
    this->m_rrExKoS = num8;
    this->m_rrN = num9;
    this->m_rrExMin = num12;
    this->m_rrExMax = num13;
    this->m_rrKoMin = num14;
    this->m_rrKoMax = num15;
    this->m_rrIsSet = true;

    return 0;
}

int CubicSpline::RC_GetKoeffRegression(double& resA, double& resB) {
    if (!this->m_rrIsSet) {
        return 65;
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
    double rrKoS = this->m_rrKoS;
    double num4 = (this->m_rrExKoS - rrKoS * rrExS / num) / num3;
    double num5 = rrKoS / num - rrExS * num4 / num;
    this->m_rrKoeffA = num5;
    this->m_rrKoeffB = num4;
    resA = num5;
    resB = num4;
    return 0;
}
int CubicSpline::getPlotPoint(std::vector<double>& x, std::vector<double>& y, std::vector<double>& x_origin) {
    for (size_t i = 0; i < _x.size(); i++) {
        if (_x[i] <= 0.0)
            continue;
        double x_t = std::log10(_x[i] * 100000.0);
        double y_t = _y[i] / ostd * 100;
        x.push_back(x_t);
        y.push_back(y_t);
        x_origin.push_back(_x[i]);
    }
    return 0;
}
int CubicSpline::getPlotLine(std::vector<double>& x, std::vector<double>& y) {
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
        x_t = std::log10(x_t * 100000.0);
        double y_t = getY(x_t) / ostd * 100;
        x.push_back(x_t);
        y.push_back(y_t);
    }

    return 0;
}


QString CubicSpline::getParameterStr() const {
    CubicSplineCurveParam p;
    int f= exportCurveParam(p);
    if (f != 0) {
        return QString("Error exporting parameters: %1").arg(f);
    }
    std::string str = nlohmann::json(p).dump(4);
    return QString::fromStdString(str);
}

int CubicSpline::exportCurveParam(CubicSplineCurveParam& out) const {
    if (!this->m_isSetCS) {
        return 79;
    }
    if (this->m_csN <= 0) {
        return 79;
    }

    // 当前内部数组固定 new double[20]。
    // 由于 k/a/c 访问 [0..n]，所以 n 最大只能是 19。
    if (this->m_csN >= 20) {
        return 76;
    }

    CubicSplineCurveParam param;
    param.n = this->m_csN;
    param.scale = 100000.0;

    param.k.resize(param.n + 1);
    param.a.resize(param.n + 1);
    param.b.resize(param.n);
    param.c.resize(param.n + 1);
    param.d.resize(param.n);

    for (int i = 0; i <= param.n; ++i) {
        param.k[i] = this->m_csK[i];
        param.a[i] = this->m_csKf0[i];
        param.c[i] = this->m_csKf2[i];
    }

    for (int i = 0; i < param.n; ++i) {
        param.b[i] = this->m_csKf1[i];
        param.d[i] = this->m_csKf3[i];
    }

    // xMin/xMax 不在原类中单独保存，这里从原始 _x 中计算。
    const int rangeRet = calcEffectiveXRange(this->_x, param.xMin, param.xMax);
    if (rangeRet != 0) {
        // 如果不是通过 calculate() 正常生成，退化为从 k 反推。
        param.xMin = std::pow(10.0, param.k.front()) / param.scale;
        param.xMax = std::pow(10.0, param.k.back()) / param.scale;
    }

    out = param;
    return 0;
}

int CubicSpline::importCurveParam(const CubicSplineCurveParam& in) {
    if (in.n <= 0) {
        return 79;
    }

    // 当前内部数组固定 20 个元素，且需要访问 [0..n]。
    if (in.n >= 20) {
        return 76;
    }

    if (in.scale <= 0.0) {
        return 79;
    }

    if (static_cast<int>(in.k.size()) < in.n + 1 ||
        static_cast<int>(in.a.size()) < in.n + 1 ||
        static_cast<int>(in.b.size()) < in.n ||
        static_cast<int>(in.c.size()) < in.n + 1 ||
        static_cast<int>(in.d.size()) < in.n) {
        return 79;
    }

    if (!isStrictIncreasing(in.k)) {
        return 74;
    }

    if (in.xMin <= 0.0 || in.xMax <= 0.0 || in.xMin > in.xMax) {
        return 79;
    }

    // 清理旧数据，避免残留参数影响调试。
    for (int i = 0; i < 20; ++i) {
        this->m_csK[i] = -999.0;
        this->m_csE[i] = -999.0;

        this->m_csKf0[i] = -999.0;
        this->m_csKf1[i] = -999.0;
        this->m_csKf2[i] = -999.0;
        this->m_csKf3[i] = -999.0;

        this->m_csh[i] = -999.0;
        this->m_csh_1[i] = -999.0;
        this->m_csH[i] = -999.0;
        this->m_csHD[i] = -999.0;
        this->m_csND1[i] = -999.0;
        this->m_csND2[i] = -999.0;

        this->m_csMxD[i] = -999.0;
        this->m_csMxRa[i] = -999.0;
        this->m_csMxRb[i] = -999.0;
        this->m_csVrz[i] = -999.0;
        this->m_csVrr[i] = -999.0;
        this->m_csY[i] = -999.0;
    }

    this->m_csN = in.n;

    for (int i = 0; i <= in.n; ++i) {
        this->m_csK[i] = in.k[i];
        this->m_csKf0[i] = in.a[i];
        this->m_csKf2[i] = in.c[i];
    }

    for (int i = 0; i < in.n; ++i) {
        this->m_csKf1[i] = in.b[i];
        this->m_csKf3[i] = in.d[i];
    }

    this->m_csKmin = in.k.front();
    this->m_csKmax = in.k[in.n];

    // 标记对象已有完整曲线，可以直接 getY/getX。
    this->m_isSetCS = true;
    this->m_isSetCSID = true;
    this->m_isSetWeight = true;
    this->m_isSetDiff = true;

    // 原 getX() 会扫描 _x 得到有效 xMin/xMax，并会访问 m_x[0]。
    // 因此导入后补齐这两个容器，避免边界判断和 m_x[0] 访问异常。
    this->_x.clear();
    this->_x.push_back(in.xMin);
    if (in.xMax != in.xMin) {
        this->_x.push_back(in.xMax);
    }

    this->_y.clear();

    this->m_x.clear();
    const double firstModelX = std::pow(10.0, in.k[0]) / in.scale;
    this->m_x.push_back(firstModelX);
    if (in.xMax != firstModelX) {
        this->m_x.push_back(in.xMax);
    }

    this->m_y.clear();

    return 0;
}

}  // namespace RIDA_ALGORITHM