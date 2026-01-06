#ifndef DEVICESTATUSOBJECT_H
#define DEVICESTATUSOBJECT_H

#include <QObject>
#include <QtGlobal>
#include <cstdint>

class DeviceStatusObject : public QObject {
    Q_OBJECT

    /* ===== 温度 ===== */
    Q_PROPERTY(float currentTemp READ currentTemp NOTIFY currentTempChanged)
    Q_PROPERTY(float targetTemp READ targetTemp NOTIFY targetTempChanged)

    /* ===== 孵育状态 ===== */
    Q_PROPERTY(bool incubPos1 READ incubPos1 NOTIFY limitSwitchChanged)
    Q_PROPERTY(bool incubPos2 READ incubPos2 NOTIFY limitSwitchChanged)
    Q_PROPERTY(bool incubPos3 READ incubPos3 NOTIFY limitSwitchChanged)
    Q_PROPERTY(bool incubPos4 READ incubPos4 NOTIFY limitSwitchChanged)
    Q_PROPERTY(bool incubPos5 READ incubPos5 NOTIFY limitSwitchChanged)
    Q_PROPERTY(bool incubPos6 READ incubPos6 NOTIFY limitSwitchChanged)

    /* ===== 孵育剩余时间（秒） ===== */
    Q_PROPERTY(int incubRemain1 READ incubRemain1 NOTIFY incubRemainChanged)
    Q_PROPERTY(int incubRemain2 READ incubRemain2 NOTIFY incubRemainChanged)
    Q_PROPERTY(int incubRemain3 READ incubRemain3 NOTIFY incubRemainChanged)
    Q_PROPERTY(int incubRemain4 READ incubRemain4 NOTIFY incubRemainChanged)
    Q_PROPERTY(int incubRemain5 READ incubRemain5 NOTIFY incubRemainChanged)
    Q_PROPERTY(int incubRemain6 READ incubRemain6 NOTIFY incubRemainChanged)
    /* ===== 孵育完成状态 ===== */
    Q_PROPERTY(bool incubDone1 READ incubDone1 NOTIFY incubRemainChanged)
    Q_PROPERTY(bool incubDone2 READ incubDone2 NOTIFY incubRemainChanged)
    Q_PROPERTY(bool incubDone3 READ incubDone3 NOTIFY incubRemainChanged)
    Q_PROPERTY(bool incubDone4 READ incubDone4 NOTIFY incubRemainChanged)
    Q_PROPERTY(bool incubDone5 READ incubDone5 NOTIFY incubRemainChanged)
    Q_PROPERTY(bool incubDone6 READ incubDone6 NOTIFY incubRemainChanged)

public:
    explicit DeviceStatusObject(QObject* parent = nullptr)
        : QObject(parent) {}

    /* ===== getters ===== */
    float currentTemp() const { return m_currentTemp; }
    float targetTemp() const { return m_targetTemp; }

    bool incubPos1() const { return m_incubPos1; }
    bool incubPos2() const { return m_incubPos2; }
    bool incubPos3() const { return m_incubPos3; }
    bool incubPos4() const { return m_incubPos4; }
    bool incubPos5() const { return m_incubPos5; }
    bool incubPos6() const { return m_incubPos6; }

    int incubRemain1() const { return m_incubRemain1; }
    int incubRemain2() const { return m_incubRemain2; }
    int incubRemain3() const { return m_incubRemain3; }
    int incubRemain4() const { return m_incubRemain4; }
    int incubRemain5() const { return m_incubRemain5; }
    int incubRemain6() const { return m_incubRemain6; }

    bool incubDone1() const { return m_incubPos1 && m_incubRemain1 == 0; }
    bool incubDone2() const { return m_incubPos2 && m_incubRemain2 == 0; }
    bool incubDone3() const { return m_incubPos3 && m_incubRemain3 == 0; }
    bool incubDone4() const { return m_incubPos4 && m_incubRemain4 == 0; }
    bool incubDone5() const { return m_incubPos5 && m_incubRemain5 == 0; }
    bool incubDone6() const { return m_incubPos6 && m_incubRemain6 == 0; }

    /* ===== setters（C++ 内部调用）===== */
    void setCurrentTemp(float v) {
        if (qFuzzyCompare(m_currentTemp, v))
            return;
        m_currentTemp = v;
        emit currentTempChanged();
    }

    void setTargetTemp(float v) {
        if (qFuzzyCompare(m_targetTemp, v))
            return;
        m_targetTemp = v;
        emit targetTempChanged();
    }

    void setIncubPos(int index, bool v) {
        bool* arr[6] = {
            &m_incubPos1, &m_incubPos2, &m_incubPos3,
            &m_incubPos4, &m_incubPos5, &m_incubPos6};

        if (index < 0 || index >= 6)
            return;

        if (*arr[index] == v)
            return;

        *arr[index] = v;
        emit limitSwitchChanged();
    }

    void setIncubRemain(int index, int sec) {
        if (sec < 0)
            sec = 0;

        int* arr[6] = {
            &m_incubRemain1, &m_incubRemain2, &m_incubRemain3,
            &m_incubRemain4, &m_incubRemain5, &m_incubRemain6};

        if (index < 0 || index >= 6)
            return;

        if (*arr[index] == sec)
            return;

        *arr[index] = sec;
        emit incubRemainChanged();
    }

    int incubRemain(int index) const {
        switch (index) {
        case 0:
            return m_incubRemain1;
        case 1:
            return m_incubRemain2;
        case 2:
            return m_incubRemain3;
        case 3:
            return m_incubRemain4;
        case 4:
            return m_incubRemain5;
        case 5:
            return m_incubRemain6;
        default:
            return 0;
        }
    }

signals:
    void currentTempChanged();
    void targetTempChanged();
    void limitSwitchChanged();
    void incubRemainChanged();

private:
    float m_currentTemp = 0.0f;
    float m_targetTemp = 0.0f;

    bool m_incubPos1 = false;
    bool m_incubPos2 = false;
    bool m_incubPos3 = false;
    bool m_incubPos4 = false;
    bool m_incubPos5 = false;
    bool m_incubPos6 = false;

    int m_incubRemain1 = 0;
    int m_incubRemain2 = 0;
    int m_incubRemain3 = 0;
    int m_incubRemain4 = 0;
    int m_incubRemain5 = 0;
    int m_incubRemain6 = 0;
};

#endif
