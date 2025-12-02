#include "ModbusWorkerThread.h"

#include <termios.h>

#include <chrono>
#include <iostream>
ModbusWorkerThread::ModbusWorkerThread(const std::string& device, int baud, int addr)
    : devPath(device), baudrate(baud), slaveAddr(addr) {}

ModbusWorkerThread::~ModbusWorkerThread() {
    stop();
}

bool ModbusWorkerThread::connectModbus() {
#ifndef LOCAL_BUILD
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
    int fd = modbus_get_socket(ctx);
    tcflush(fd, TCIOFLUSH);  // ⭐ 清空输入输出缓冲区
#endif
    return true;
}

void ModbusWorkerThread::closeModbus() {
#ifndef LOCAL_BUILD
    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = nullptr;
    }
#endif
}

void ModbusWorkerThread::start() {
#ifndef LOCAL_BUILD
    if (running.load())
        return;
    running.store(true);
    worker = std::thread(&ModbusWorkerThread::threadFunc, this);
#endif
}

void ModbusWorkerThread::stop() {
#ifndef LOCAL_BUILD
    if (!running.load())
        return;
    enqueue({MotorCmdType::Exit});
    running.store(false);
    cv.notify_all();
    if (worker.joinable())
        worker.join();
    closeModbus();
#endif
}

void ModbusWorkerThread::enqueue(const MotorCommand& cmd) {
#ifndef LOCAL_BUILD
    std::lock_guard<std::mutex> lock(mtx);
    cmdQueue.push(cmd);
    cv.notify_one();  // 唤醒当前在 cv 上等待（cv.wait(...)）的一个线程
#endif
}

void ModbusWorkerThread::threadFunc() {
#ifndef LOCAL_BUILD
    if (!connectModbus())
        return;
    int fd = modbus_get_socket(ctx);

    modbus_set_debug(ctx, TRUE);
    std::cout << "🧵 Modbus worker thread started." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    tcflush(fd, TCIFLUSH);
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
#endif
}

void ModbusWorkerThread::handleCommand(const MotorCommand& cmd) {
#ifndef LOCAL_BUILD
    bool ok = false;

    switch (cmd.type) {
    case MotorCmdType::Enable: {
        std::lock_guard<std::mutex> lk(ioMtx);  // ✅ 加锁
        ok = (modbus_write_register(ctx, 0x00F3, 1) != -1);
        std::cout << "⚙️ 驱动使能\n";
        break;
    }
    case MotorCmdType::Disable: {
        std::lock_guard<std::mutex> lk(ioMtx);  // ✅
        ok = (modbus_write_register(ctx, 0x00F3, 0) != -1);
        std::cout << "🛑 驱动关闭\n";
        break;
    }
    case MotorCmdType::ClearAlarm: {
        std::lock_guard<std::mutex> lk(ioMtx);  // ✅
        ok = (modbus_write_register(ctx, 0x00F3, 2) != -1);
        std::cout << "🚨 清除报警\n";
        break;
    }
    case MotorCmdType::Stop: {
        std::lock_guard<std::mutex> lk(ioMtx);  // ✅
        ok = (modbus_write_register(ctx, 0x00F7, 1) != -1);
        std::cout << "⛔ 立停\n";
        break;
    }
    case MotorCmdType::RunSpeed: {
        uint16_t regs[2];
        regs[0] = ((cmd.dir & 0xFF) << 8) | (cmd.acc & 0xFF);
        regs[1] = static_cast<uint16_t>(cmd.rpm);
        {
            std::lock_guard<std::mutex> lk(ioMtx);  // ✅
            ok = (modbus_write_registers(ctx, 0x00F6, 2, regs) != -1);
        }
        std::cout << "🚀 速度模式 dir=" << cmd.dir << " rpm=" << cmd.rpm << "\n";
        break;
    }
    case MotorCmdType::RunPosition: {
        // std::this_thread::sleep_for(std::chrono::milliseconds(2000));  // 延时2秒
        uint16_t regs[4];

        regs[0] = ((cmd.dir & 0xFF) << 8) | (cmd.acc & 0xFF);
        regs[1] = static_cast<uint16_t>(cmd.rpm);
        regs[2] = (cmd.pulses >> 16) & 0xFFFF;
        regs[3] = (cmd.pulses) & 0xFFFF;
        {
            std::lock_guard<std::mutex> lk(ioMtx);  // ✅
            ok = (modbus_write_registers(ctx, 0x00FD, 4, regs) != -1);
        }
        std::cout << "📍 位置模式 dir=" << cmd.dir
                  << " rpm=" << cmd.rpm
                  << " pulses=" << cmd.pulses << "\n";
        break;
    }
    case MotorCmdType::Back: {
        std::lock_guard<std::mutex> lk(ioMtx);  // ✅
        ok = (modbus_write_register(ctx, 0x0091, 1) != -1);
        std::cout << "🔙 回原点\n";
        break;
    }
    default:
        break;
    }

    if (!ok) {
        std::cerr << "❌ Modbus 命令执行失败: " << modbus_strerror(errno) << std::endl;
    }
#endif
}

int ModbusWorkerThread::readRegister(int addr) {
#ifndef LOCAL_BUILD
    if (!ctx) {
        std::cerr << "❌ Modbus 未连接，无法读取寄存器" << std::endl;
        return -1;
    }

    uint16_t val = 0;
    // ✅ 使用输入寄存器读取函数 (功能码 04)
    std::lock_guard<std::mutex> lk(ioMtx);
    int rc = modbus_read_input_registers(ctx, addr, 1, &val);
    if (rc == -1) {
        std::cerr << "❌ 读取输入寄存器失败: " << modbus_strerror(errno)
                  << "  地址: 0x" << std::hex << addr << std::endl;

        return -1;
    }

    std::cout << "📖 输入寄存器 0x" << std::hex << addr
              << " 值=" << std::dec << val << std::endl;
    return static_cast<int>(val);
#else
    return 0;
#endif
}
