#include <linux/input-event-codes.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariantList>

#include "APP/MainViewModel.h"
#include "CardWatcherStd.h"  // ← 用纯 C++ 线程版
// #include "DBWorker.h"        // 数据库后台线程
// #include "DTO.h"             // 数据结构
#include "KeysProxy.h"
#include "ModbusWorkerThread.h"
#include "MotorController.h"
#include "PrinterManager.h"
// #include "SettingsViewModel.h"              // 设置页 VM
// #include "UserViewModel.h"                  // 用户 VM（登录/管理）
static bool ensureDir(const QString& path)  // path：要创建的目录
{
    QDir d;                                  // QDir 用于目录操作
    return d.exists(path) ? true             // 如果目录已存在 → 直接返回 true
                          : d.mkpath(path);  // 否则递归创建目录
}

static void setupQtDpiAndPlatform() {
#ifndef LOCAL_BUILD
    qputenv("QT_QPA_PLATFORM",
            "linuxfb:tty=/dev/fb0:size=1024x600:mmsize=271x159");
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qputenv("QT_SCALE_FACTOR", "1");
    qputenv("QT_FONT_DPI", "96");
    qDebug() << "UI thread id =" << QThread::currentThreadId();
    qunsetenv("QT_DEVICE_PIXEL_RATIO");
    qunsetenv("QT_SCREEN_SCALE_FACTORS");
#endif
    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, false);
}

int main(int argc, char* argv[]) {
    setupQtDpiAndPlatform();
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_QPA_PLATFORM", "linuxfb:tty=/dev/fb0:size=1024x600:mmsize=271x159,tslib=1");
    qputenv("QT_QPA_FB_TSLIB", "1");
    qputenv("TSLIB_TSDEVICE", "/dev/input/event3");
    qunsetenv("QT_QPA_GENERIC_PLUGINS");  // 避免两套输入栈同时启用
    QApplication app(argc, argv);
    QApplication::setOverrideCursor(Qt::BlankCursor);
    qputenv("QT_PLUGIN_PATH", "/usr/lib/arm-qt/plugins");

    const QString dbDir = "/mnt/SDCARD/app/db";  // TF/SD 路径
    const QString dbPath = dbDir + "/app.db";    // 数据库文件
    QDir().mkpath(dbDir);

    // ✅ 元类型注册（QML 才能收 newDataBatch）
    qRegisterMetaType<QVector<uint16_t>>("QVector<uint16_t>");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QVariantList>("QVariantList");

    // ✅ 注册 MotorController 给 QML 使用
    qmlRegisterType<MotorController>("Motor", 1, 0, "MotorController");
    // ===============================
    // 1) 创建后台线程版的卡在位检测器
    // ===============================
    KeysProxy keysProxy;
    const QString devArg = (argc >= 2) ? QString::fromLocal8Bit(argv[1]) : QString();
    CardWatcherStd cardWatcher(&keysProxy, devArg.toStdString());
    cardWatcher.setWatchedCode(KEY_PROG1);  // 等价于数字 148

    cardWatcher.setDebounceMs(20);
    if (!cardWatcher.start()) {
        qWarning() << "[CardWatcherStd] start failed";
        // 视情况决定是否退出
        // return -2;
    }
    // ===============================
    // 2) 你的业务对象 & 打印机初始化
    // ===============================
    QQmlApplicationEngine engine;
    MainViewModel viewModel;

    PrinterManager& printer1 = PrinterManager::instance();
    printer1.initPrinter();

    QFile f(":/styles/main.qss");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(f.readAll()));
        printf("Load stylesheet success.\n");
    } else {
        printf("Load stylesheet failed.\n");
    }

    // 把对象注入到 QML
    engine.rootContext()->setContextProperty("mainViewModel", &viewModel);
    engine.rootContext()->setContextProperty("keys", &keysProxy);

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
