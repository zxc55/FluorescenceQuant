#include "MotorController.h"

#include <QDebug>

MotorController::MotorController(QObject* parent)
    : QObject(parent) {
    worker = new ModbusWorkerThread("/dev/ttyS4", 115200, 1);
}

MotorController::~MotorController() {
    if (worker) {
        worker->stop();
        delete worker;
    }
}

void MotorController::start() {
    if (worker) {
        worker->start();
        emit logMessage("✅ Modbus worker started");
    }
}

void MotorController::stop() {
    if (worker) {
        worker->stop();
        emit logMessage("🛑 Modbus worker stopped");
    }
}

void MotorController::enable() {
    if (worker) {
        worker->enqueue({MotorCmdType::Enable});
        emit logMessage("⚙️ 驱动使能");
    }
}

void MotorController::disable() {
    if (worker) {
        worker->enqueue({MotorCmdType::Disable});
        emit logMessage("🛑 驱动关闭");
    }
}

void MotorController::clearAlarm() {
    if (worker) {
        worker->enqueue({MotorCmdType::ClearAlarm});
        emit logMessage("🚨 清除报警");
    }
}

void MotorController::stopMotor() {
    if (worker) {
        worker->enqueue({MotorCmdType::Stop});
        emit logMessage("⛔ 电机立停");
    }
}

void MotorController::runSpeed(int dir, int acc, int rpm) {
    if (worker) {
        worker->enqueue({MotorCmdType::RunSpeed, dir, acc, rpm});
        emit logMessage(QString("🚀 速度模式 dir=%1 rpm=%2").arg(dir).arg(rpm));
    }
}

void MotorController::runPosition(int dir, int acc, int rpm, int pulses) {
    if (worker) {
        worker->enqueue({MotorCmdType::RunPosition, dir, acc, rpm, pulses});
        // emit logMessage(QString("📍 位置模式 dir=%1 rpm=%2 pulses=%3")
        //                     .arg(dir)
        //                     .arg(rpm)
        //                     .arg(pulses));
    }
}
void MotorController::back() {
    if (worker) {
        worker->enqueue({MotorCmdType::Back});
        emit logMessage(QString("回原点ing"));
    }
}
int MotorController::readRegister(int addr) {
    if (!worker)
        return -1;
    return worker->readRegister(addr);
}
111