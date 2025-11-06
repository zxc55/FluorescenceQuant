#ifndef SETTINGSVIEWMODEL_H_
#define SETTINGSVIEWMODEL_H_
#include <QObject>  // QObject 基类
#include <QString>  // QString 字符串
#include <QTimer>   // QTimer 定时器（做去抖）

#include "DBWorker.h"  // DBWorker 声明（后台线程执行数据库操作）
#include "DTO.h"       // AppSettingsRow 数据结构（与数据库字段对应）

// SettingsViewModel：供 QML 直接绑定的“系统设置”视图模型
// 作用：把 app_settings（单行 id=1）暴露为一组 Q_PROPERTY，支持双向绑定 + 自动保存
class SettingsViewModel : public QObject {
Q_OBJECT  // Qt 元对象宏（支持信号槽、属性）

// —— 可绑定到 QML 的属性（读函数 / 写函数 / 变更信号）——
Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer NOTIFY manufacturerChanged)
    Q_PROPERTY(bool engineerMode READ engineerMode WRITE setEngineerMode NOTIFY engineerModeChanged)
        Q_PROPERTY(bool autoPrint READ autoPrint WRITE setAutoPrint NOTIFY autoPrintChanged)
            Q_PROPERTY(int backlight READ backlight WRITE setBacklight NOTIFY backlightChanged)
                Q_PROPERTY(QString lang READ lang WRITE setLang NOTIFY langChanged)
                    Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY updatedAtChanged)

                        public : explicit SettingsViewModel(QObject* parent = nullptr);  // 构造：初始化定时器/默认值
    ~SettingsViewModel() override = default;                                             // 析构：默认即可

    // —— 把 DBWorker 注入进来（main.cpp 创建好后调用）——
    void bindWorker(DBWorker* worker);  // 绑定后台数据库线程对象

    // —— 只读 Getter（供 QML 读属性）——
    QString manufacturer() const { return m_.manufacturer; }  // 厂家名称
    bool engineerMode() const { return m_.engineerMode; }     // 工程师模式
    bool autoPrint() const { return m_.autoPrint; }           // 自动打印
    int backlight() const { return m_.backlight; }            // 背光 0~100
    QString lang() const { return m_.lang; }                  // 语言
    QString updatedAt() const { return m_.updatedAt; }        // 更新时间（数据库填）

    // —— 可写 Setter（供 QML 写属性；内部触发自动保存去抖）——
    void setManufacturer(const QString& s);  // 改厂家
    void setEngineerMode(bool on);           // 改工程师模式
    void setAutoPrint(bool on);              // 改自动打印
    void setBacklight(int v);                // 改背光
    void setLang(const QString& s);          // 改语言

    // —— 可被 QML 调用的便捷方法（按需手动触发）——
    Q_INVOKABLE void save();    // 立即保存（不等去抖）
    Q_INVOKABLE void reload();  // 从数据库重新读取（覆盖本地改动）

signals:
    // —— 属性变更信号（QML 绑定会自动监听）——
    void manufacturerChanged();  // 厂家变更
    void engineerModeChanged();  // 工程师模式变更
    void autoPrintChanged();     // 自动打印变更
    void backlightChanged();     // 背光变更
    void langChanged();          // 语言变更
    void updatedAtChanged();     // 更新时间变更

private:
    // —— 内部工具：收到数据库的最新值时统一刷新属性 ——
    void applyFromDb(const AppSettingsRow& r);  // 用数据库值刷新本地模型并发信号
    void scheduleSave();                        // 安排一次延时保存（去抖）
    void doSaveNow();                           // 立即落库（调用 DBWorker）

private:
    DBWorker* worker_ = nullptr;  // 后台线程对象指针（外部注入）
    AppSettingsRow m_;            // 本地模型（与数据库行一一对应）
    bool loading_ = false;        // 标记“正在从数据库加载”（避免回写风暴）
    bool dirty_ = false;          // 标记“本地有未保存更改”
    QTimer saveTimer_;            // 去抖定时器（触发时执行保存）
};
#endif  // SETTINGSVIEWMODEL_H_