#include "ModbusWorkerThread.h"

#include <chrono>
#include <iostream>

ModbusWorkerThread::ModbusWorkerThread(const std::string& device, int baud, int addr)
    : devPath(device), baudrate(baud), slaveAddr(addr) {}

ModbusWorkerThread::~ModbusWorkerThread() {
    stop();
}

bool ModbusWorkerThread::connectModbus() {
    ctx = modbus_new_rtu(devPath.c_str(), baudrate, 'N', 8, 1);
    if (!ctx) {
        std::cerr << "❌ 无法创建 Modbus RTU 连接" << std::endl;
        return false;
    }
    if (modbus_set_slave(ctx, slaveAddr) == -1) {
        std::cerr << "❌ 设置从站地址失败" << std::endl;
        modbus_free(ctx);
        ctx = nullptr;
        return false;
    }
    if (modbus_connect(ctx) == -1) {
        std::cerr << "❌ 连接串口失败: " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        ctx = nullptr;
        return false;
    }
    std::cout << "✅ Modbus 连接成功: " << devPath << " 波特率 " << baudrate << std::endl;
    return true;
}

void ModbusWorkerThread::closeModbus() {
    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = nullptr;
    }
}

void ModbusWorkerThread::start() {
    if (running.load())
        return;
    running.store(true);
    worker = std::thread(&ModbusWorkerThread::threadFunc, this);
}

void ModbusWorkerThread::stop() {
    if (!running.load())
        return;
    enqueue({MotorCmdType::Exit});
    running.store(false);
    cv.notify_all();
    if (worker.joinable())
        worker.join();
    closeModbus();
}

void ModbusWorkerThread::enqueue(const MotorCommand& cmd) {
    std::lock_guard<std::mutex> lock(mtx);
    cmdQueue.push(cmd);
    cv.notify_one();
}

void ModbusWorkerThread::threadFunc() {
    if (!connectModbus())
        return;
    std::cout << "🧵 Modbus worker thread started." << std::endl;

    while (running.load()) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() { return !cmdQueue.empty() || !running.load(); });
        if (!running.load())
            break;

        MotorCommand cmd = cmdQueue.front();
        cmdQueue.pop();
        lock.unlock();

        if (cmd.type == MotorCmdType::Exit)
            break;

        handleCommand(cmd);
    }

    std::cout << "🧵 Modbus worker thread stopped." << std::endl;
    closeModbus();
}

void ModbusWorkerThread::handleCommand(const MotorCommand& cmd) {
    bool ok = false;

    switch (cmd.type) {
    case MotorCmdType::Enable:
        ok = modbus_write_register(ctx, 0x00F3, 1) != -1;
        std::cout << "⚙️ 驱动使能\n";
        break;
    case MotorCmdType::Disable:
        ok = modbus_write_register(ctx, 0x00F3, 0) != -1;
        std::cout << "🛑 驱动关闭\n";
        break;
    case MotorCmdType::ClearAlarm:
        ok = modbus_write_register(ctx, 0x00F3, 2) != -1;
        std::cout << "🚨 清除报警\n";
        break;
    case MotorCmdType::Stop:
        ok = modbus_write_register(ctx, 0x00F7, 1) != -1;
        std::cout << "⛔ 立停\n";
        break;
    case MotorCmdType::RunSpeed: {
        uint16_t regs[2];
        regs[0] = ((cmd.dir & 0xFF) << 8) | (cmd.acc & 0xFF);
        regs[1] = static_cast<uint16_t>(cmd.rpm);
        ok = modbus_write_registers(ctx, 0x00F6, 2, regs) != -1;
        std::cout << "🚀 速度模式 dir=" << cmd.dir << " rpm=" << cmd.rpm << "\n";
        break;
    }
    case MotorCmdType::RunPosition: {
        uint16_t regs[4];
        regs[0] = ((cmd.dir & 0xFF) << 8) | (cmd.acc & 0xFF);
        regs[1] = static_cast<uint16_t>(cmd.rpm);
        regs[2] = (cmd.pulses >> 16) & 0xFFFF;
        regs[3] = cmd.pulses & 0xFFFF;
        ok = modbus_write_registers(ctx, 0x00FD, 4, regs) != -1;
        std::cout << "📍 位置模式 dir=" << cmd.dir
                  << " rpm=" << cmd.rpm
                  << " pulses=" << cmd.pulses << "\n";
        break;
    }
    default:
        break;
    }

    if (!ok) {
        std::cerr << "❌ Modbus 命令执行失败: " << modbus_strerror(errno) << std::endl;
    }
}
