#include "SettingsViewModel.h"  // 自己的头文件

#include <QDebug>  // qDebug 等日志

// —— 构造函数：初始化默认值与定时器 ——
SettingsViewModel::SettingsViewModel(QObject* parent)
    : QObject(parent)  // 调用父类构造
{
    // 设置去抖定时器为“单次触发”，到点只触发一次
    saveTimer_.setSingleShot(true);  // 单次触发
    saveTimer_.setInterval(300);     // 去抖间隔 300ms（可按需调整）

    // 定时器到点后执行真正保存逻辑
    connect(&saveTimer_, &QTimer::timeout, this, [this]() {
        doSaveNow();  // 触发保存
    });

    // 给模型填一些初始显示值（防止 QML 看到空白；真正值以数据库加载为准）
    m_.manufacturer = QStringLiteral("Pribolab");  // 初始厂家名
    m_.engineerMode = false;                       // 初始为非工程师模式
    m_.autoPrint = true;                           // 初始开启自动打印
    m_.backlight = 80;                             // 初始背光 80
    m_.lang = QStringLiteral("zh_CN");             // 初始语言 zh_CN
    m_.updatedAt.clear();                          // 更新时间留空，等数据库读回
}

// —— 注入 DBWorker，并把它的信号连接到本 VM ——
void SettingsViewModel::bindWorker(DBWorker* worker) {
    worker_ = worker;  // 保存指针（不转移所有权）

    // 连接：数据库线程读到设置后，回调到 UI 线程，调用 applyFromDb 刷新属性
    connect(worker_, &DBWorker::settingsLoaded,  // 信号：加载成功
            this, [this](const AppSettingsRow& r) {
                applyFromDb(r);  // 用数据库值刷新属性
            });

    // 连接：保存完成（成功/失败）后，这里可做提示或重读（我们在 DBWorker 内已做重读）
    connect(worker_, &DBWorker::settingsUpdated,  // 信号：保存完成
            this, [this](bool ok) {
                if (!ok)  // 如果失败，简单打个日志
                    qWarning() << "[SettingsVM] update failed";
            });

    // 绑定完成后，主动向数据库读一次（把真实数据刷到界面）
    if (worker_)
        worker_->postLoadSettings();  // 请求后台线程加载 id=1
}

// —— 内部：用数据库读取到的值，刷新本地模型与通知 QML ——
void SettingsViewModel::applyFromDb(const AppSettingsRow& r) {
    loading_ = true;  // 标记“正在加载”，Setter 不触发保存

    // 判断差异再发信号（避免无意义刷新）
    if (m_.manufacturer != r.manufacturer) {  // 若厂家不同
        m_.manufacturer = r.manufacturer;     // 更新本地值
        emit manufacturerChanged();           // 通知 QML
    }
    if (m_.engineerMode != r.engineerMode) {  // 若工程师模式不同
        m_.engineerMode = r.engineerMode;     // 更新本地值
        emit engineerModeChanged();           // 通知 QML
    }
    if (m_.autoPrint != r.autoPrint) {  // 若自动打印不同
        m_.autoPrint = r.autoPrint;     // 更新本地值
        emit autoPrintChanged();        // 通知 QML
    }
    if (m_.backlight != r.backlight) {  // 若背光不同
        m_.backlight = r.backlight;     // 更新本地值
        emit backlightChanged();        // 通知 QML
    }
    if (m_.lang != r.lang) {  // 若语言不同
        m_.lang = r.lang;     // 更新本地值
        emit langChanged();   // 通知 QML
    }
    if (m_.updatedAt != r.updatedAt) {  // 若更新时间不同
        m_.updatedAt = r.updatedAt;     // 更新本地值
        emit updatedAtChanged();        // 通知 QML
    }

    loading_ = false;  // 结束“加载中”标记
    dirty_ = false;    // 刷库后清除脏标记
}

// —— Setter：设置厂家名称（触发去抖保存）——
void SettingsViewModel::setManufacturer(const QString& s) {
    if (m_.manufacturer == s)
        return;                  // 值未变时忽略
    m_.manufacturer = s;         // 更新本地值
    emit manufacturerChanged();  // 通知 QML 刷新
    if (loading_)
        return;      // 正在加载时不触发保存
    dirty_ = true;   // 标记为脏
    scheduleSave();  // 安排延时保存
}

// —— Setter：设置工程师模式（触发去抖保存）——
void SettingsViewModel::setEngineerMode(bool on) {
    if (m_.engineerMode == on)
        return;                  // 值未变时忽略
    m_.engineerMode = on;        // 更新本地值
    emit engineerModeChanged();  // 通知 QML
    if (loading_)
        return;      // 正在加载时不触发保存
    dirty_ = true;   // 标记为脏
    scheduleSave();  // 安排延时保存
}

// —— Setter：设置自动打印（触发去抖保存）——
void SettingsViewModel::setAutoPrint(bool on) {
    if (m_.autoPrint == on)
        return;               // 值未变时忽略
    m_.autoPrint = on;        // 更新本地值
    emit autoPrintChanged();  // 通知 QML
    if (loading_)
        return;      // 正在加载时不触发保存
    dirty_ = true;   // 标记为脏
    scheduleSave();  // 安排延时保存
}

// —— Setter：设置背光（触发去抖保存；夹紧到 0~100）——
void SettingsViewModel::setBacklight(int v) {
    if (v < 0)
        v = 0;  // 下限保护
    if (v > 100)
        v = 100;  // 上限保护
    if (m_.backlight == v)
        return;               // 值未变时忽略
    m_.backlight = v;         // 更新本地值
    emit backlightChanged();  // 通知 QML
    if (loading_)
        return;      // 正在加载时不触发保存
    dirty_ = true;   // 标记为脏
    scheduleSave();  // 安排延时保存
}

// —— Setter：设置语言（触发去抖保存）——
void SettingsViewModel::setLang(const QString& s) {
    if (m_.lang == s)
        return;          // 值未变时忽略
    m_.lang = s;         // 更新本地值
    emit langChanged();  // 通知 QML
    if (loading_)
        return;      // 正在加载时不触发保存
    dirty_ = true;   // 标记为脏
    scheduleSave();  // 安排延时保存
}

// —— QML 可调用：立即保存（不等去抖）——
void SettingsViewModel::save() {
    // 取消去抖定时器，直接保存
    if (saveTimer_.isActive())
        saveTimer_.stop();  // 停止计时器
    doSaveNow();            // 立刻落库
}

// —— QML 可调用：从数据库重读（覆盖本地修改）——
void SettingsViewModel::reload() {
    if (!worker_)
        return;                   // 未绑定 DBWorker 直接返回
    worker_->postLoadSettings();  // 请求后台线程读取 id=1
}

// —— 内部：启动/重置去抖定时器 ——
void SettingsViewModel::scheduleSave() {
    if (!worker_)
        return;  // 未绑定 DBWorker 不保存
    // 重启计时器：每次修改都会把“保存时间”往后推 300ms（防止频繁落库）
    saveTimer_.start();  // 300ms 后触发 timeout → doSaveNow()
}

// —— 内部：真正执行保存（调用 DBWorker 在线程里写数据库）——
void SettingsViewModel::doSaveNow() {
    if (!worker_)
        return;  // 未绑定 DBWorker 不保存
    if (!dirty_)
        return;  // 没有脏改动就不写
    // 把当前本地模型提交给后台线程，写 app_settings 的 id=1
    worker_->postUpdateSettings(m_);  // 线程安全：DBWorker 内部串行执行
    // 注：DBWorker 保存成功后会再次读取并通过 settingsLoaded 回写 UI
    dirty_ = false;  // 提交后先清脏标（最终以回读为准）
}
