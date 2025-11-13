#ifndef MODBUSWORKERTHREAD_H_
#define MODBUSWORKERTHREAD_H_

#include <modbus/modbus.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

/**
 * @brief 电机命令类型
 */
enum class MotorCmdType {
    Enable,       // 使能驱动
    Disable,      // 关闭驱动
    ClearAlarm,   // 清除报警
    RunSpeed,     // 速度控制模式运行
    RunPosition,  // 位置控制模式运行
    Stop,         // 立停
    Exit,         // 退出线程
    Back          // 回原点
};

/**
 * @brief 电机命令结构体
 */
struct MotorCommand {
    MotorCmdType type;
    int dir = 0;
    int acc = 0;
    int rpm = 0;
    int pulses = 0;
};

/**
 * @brief ModbusWorkerThread
 * 使用C++标准线程实现的后台命令处理线程
 */
class ModbusWorkerThread {
public:
    ModbusWorkerThread(const std::string& device, int baud, int addr);
    ~ModbusWorkerThread();

    void start();
    void stop();
    void enqueue(const MotorCommand& cmd);
    int readRegister(int addr);

private:
    void threadFunc();
    void handleCommand(const MotorCommand& cmd);
    bool connectModbus();
    void closeModbus();

private:
    modbus_t* ctx = nullptr;
    std::string devPath;
    int baudrate;
    int slaveAddr;

    std::thread worker;
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<MotorCommand> cmdQueue;
    std::atomic<bool> running{false};
    std::mutex ioMtx;
};

#endif  // MODBUSWORKERTHREAD_H_
