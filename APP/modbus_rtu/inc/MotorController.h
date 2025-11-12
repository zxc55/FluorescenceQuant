#ifndef MOTORCONTROLLER_H_
#define MOTORCONTROLLER_H_

#include <QObject>
#include <QString>

#include "ModbusWorkerThread.h"

/**
 * @brief MotorController 电机控制接口层（供 QML 调用）
 */
class MotorController : public QObject {
    Q_OBJECT
public:
    explicit MotorController(QObject* parent = nullptr);
    ~MotorController();
    Q_INVOKABLE int readRegister(int addr);
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void enable();
    Q_INVOKABLE void disable();
    Q_INVOKABLE void clearAlarm();
    Q_INVOKABLE void stopMotor();
    Q_INVOKABLE void runSpeed(int dir, int acc, int rpm);
    Q_INVOKABLE void runPosition(int dir, int acc, int rpm, int pulses);
    Q_INVOKABLE void back();
signals:
    void logMessage(const QString& msg);

private:
    ModbusWorkerThread* worker = nullptr;
};

#endif  // MOTORCONTROLLER_H_
