#ifndef DEVICESTATUSOBJECT_H
#define DEVICESTATUSOBJECT_H

#include <QObject>

class DeviceStatusObject : public QObject {
    Q_OBJECT

    /* ===== 温度 ===== */
    Q_PROPERTY(float currentTemp READ currentTemp NOTIFY currentTempChanged)
    Q_PROPERTY(float targetTemp READ targetTemp NOTIFY targetTempChanged)

    /* ===== 孵育 ===== */
    Q_PROPERTY(int incubState READ incubState NOTIFY incubStateChanged)
    Q_PROPERTY(bool limitSwitch READ limitSwitch NOTIFY limitSwitchChanged)

    /* ===== 电机 ===== */
    Q_PROPERTY(int motorState READ motorState NOTIFY motorStateChanged)
    Q_PROPERTY(int motorSteps READ motorSteps NOTIFY motorStepsChanged)

public:
    explicit DeviceStatusObject(QObject* parent = nullptr)
        : QObject(parent) {}

    /* ===== getters ===== */
    float currentTemp() const { return m_currentTemp; }
    float targetTemp() const { return m_targetTemp; }

    int incubState() const { return m_incubState; }
    bool limitSwitch() const { return m_limitSwitch; }

    int motorState() const { return m_motorState; }
    int motorSteps() const { return m_motorSteps; }

    /* ===== setters（只给 C++ 用）===== */
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

    void setIncubState(int v) {
        if (m_incubState == v)
            return;
        m_incubState = v;
        emit incubStateChanged();
    }

    void setLimitSwitch(bool v) {
        if (m_limitSwitch == v)
            return;
        m_limitSwitch = v;
        emit limitSwitchChanged();
    }

    void setMotorState(int v) {
        if (m_motorState == v)
            return;
        m_motorState = v;
        emit motorStateChanged();
    }

    void setMotorSteps(int v) {
        if (m_motorSteps == v)
            return;
        m_motorSteps = v;
        emit motorStepsChanged();
    }

signals:
    void currentTempChanged();
    void targetTempChanged();
    void incubStateChanged();
    void limitSwitchChanged();
    void motorStateChanged();
    void motorStepsChanged();

private:
    float m_currentTemp = 0.0f;
    float m_targetTemp = 0.0f;

    int m_incubState = 0;
    bool m_limitSwitch = false;

    int m_motorState = 0;
    int m_motorSteps = 0;
};

#endif
