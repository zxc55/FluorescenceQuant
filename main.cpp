// main.cpp
#include <linux/input-event-codes.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMetaType>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QThread>
#include <QVariantList>

#include "APP/MyQmlComponents/MyLineSeries/MyLineSeries.h"
// ====== 你的工程头文件 ======
#include "CardWatcherStd.h"
#include "HistoryViewModel.h"
#include "IIODeviceController.h"
#include "KeysProxy.h"
#include "MainViewModel.h"
#include "MotorController.h"
#include "PrinterManager.h"

// ====== SQLite / ViewModels / DB 线程 ======
#include "APP/MyQmlComponents/MyCurveLoader/CurveLoader.h"
#include "APP/MyQmlComponents/MyPlotView/PlotView.h"
#include "APP/MyQmlComponents/MySeriesFeeder/SeriesFeeder.h"
#include "DBWorker.h"
#include "DTO.h"
#include "ProjectsViewModel.h"
#include "SettingsViewModel.h"
#include "UserViewModel.h"
static bool ensureDir(const QString& path) {
    QDir d;
    return d.exists(path) ? true : d.mkpath(path);
}

static void setupQtDpiAndPlatform() {
#ifndef LOCAL_BUILD
    qputenv("QT_QPA_PLATFORM", QByteArray("linuxfb:tty=/dev/fb0:size=1024x600:mmsize=271x159"));
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qputenv("QT_SCALE_FACTOR", "1");
    qputenv("QT_FONT_DPI", "96");
    qunsetenv("QT_DEVICE_PIXEL_RATIO");
    qunsetenv("QT_SCREEN_SCALE_FACTORS");
#endif
    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, false);
}

int main(int argc, char* argv[]) {
    setupQtDpiAndPlatform();
#ifndef LOCAL_BUILD

    // —— 输入法/触摸相关 —— //
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_QPA_PLATFORM", QByteArray("linuxfb:tty=/dev/fb0:size=1024x600:mmsize=271x159,tslib=1"));
    qputenv("QT_QPA_FB_TSLIB", QByteArray("1"));
    qputenv("TSLIB_TSDEVICE", QByteArray("/dev/input/event3"));
    qunsetenv("QT_QPA_GENERIC_PLUGINS");  // 避免两套输入栈同时启用

    // —— Qt 插件路径（确保能找到 libqsqlite.so）—— //
    qputenv("QT_PLUGIN_PATH", QByteArray("/usr/lib/arm-qt/plugins"));
#endif

    // ========== 注册跨线程信号/类型 ==========
    qRegisterMetaType<AppSettingsRow>("AppSettingsRow");
    qRegisterMetaType<UserRow>("UserRow");
    qRegisterMetaType<QVector<UserRow>>("QVector<UserRow>");
    qRegisterMetaType<ProjectRow>("ProjectRow");
    qRegisterMetaType<QVector<ProjectRow>>("QVector<ProjectRow>");
    qRegisterMetaType<QVector<uint16_t>>("QVector<uint16_t>");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QVariantList>("QVariantList");

    // ✅ 新增：注册历史记录类型
    qRegisterMetaType<HistoryRow>("HistoryRow");
    qRegisterMetaType<QVector<HistoryRow>>("QVector<HistoryRow>");

    // ========== 注册 QML 类型 ==========
    qmlRegisterType<MotorController>("Motor", 1, 0, "MotorController");
    qmlRegisterType<ProjectsViewModel>("App", 1, 0, "ProjectsViewModel");
    qmlRegisterType<SeriesFeeder>("App", 1, 0, "SeriesFeeder");
    qmlRegisterType<MyLineSeries>(
        "App",          // QML 模块名
        1, 0,           // 版本号
        "MyLineSeries"  // QML 中的类名
    );
    qmlRegisterType<CurveLoader>("App", 1, 0, "CurveLoader");
    QApplication app(argc, argv);
    QApplication::setOverrideCursor(Qt::BlankCursor);
    qInfo() << "UI thread id =" << QThread::currentThread();
#ifndef LOCAL_BUILD
    // ================= 路径 =================
    const QString dbDir = "/mnt/SDCARD/app/db";
#else
    const QString dbDir = "/home/pribolab/Project/FluorescenceQuant/debugDir";
#endif
    const QString dbPath = dbDir + "/app.db";
    ensureDir(dbDir);

    // ================ 卡在位检测 ================
    KeysProxy keysProxy;
    const QString devArg = (argc >= 2) ? QString::fromLocal8Bit(argv[1]) : QString();
    CardWatcherStd cardWatcher(&keysProxy, devArg.toStdString());
    cardWatcher.setWatchedCode(KEY_PROG1);
    cardWatcher.setDebounceMs(20);
    if (!cardWatcher.start()) {
        qWarning() << "[CardWatcherStd] start failed";
    }

    // ================= DB线程与Worker =================
    DBWorker* db = new DBWorker(dbPath);
    QThread dbThread;
    db->moveToThread(&dbThread);

    QObject::connect(&dbThread, &QThread::started, db, &DBWorker::initialize, Qt::QueuedConnection);

    QObject::connect(db, &DBWorker::ready, []() {
        qInfo() << "[DB] schema ready";
    });
    QObject::connect(db, &DBWorker::errorOccurred, [](const QString& m) {
        qWarning() << "[DB] error:" << m;
    });

    // ================= ViewModels =================
    MainViewModel mainVm;
    SettingsViewModel settingsVm;
    UserViewModel userVm;
    ProjectsViewModel projectsVm(db);
    HistoryViewModel historyVm(db);

    // 绑定 Worker
    settingsVm.bindWorker(db);
    userVm.bindWorker(db);

    QObject::connect(db, &DBWorker::ready, [db]() {
        db->postLoadSettings();
        db->postLoadUsers();
        db->postLoadProjects();
        db->postLoadHistory();  // ✅ 启动时加载历史记录
    });

    // ================= 打印机初始化 =================
    PrinterManager& printer = PrinterManager::instance();
    printer.initPrinter();

    // ================= QSS 样式 =================
    QFile qss(":/styles/main.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
        qInfo() << "Load stylesheet success.";
    } else {
        qWarning() << "Load stylesheet failed.";
    }

    // ================= 注入 QML =================
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("mainViewModel", &mainVm);
    engine.rootContext()->setContextProperty("settingsVm", &settingsVm);
    engine.rootContext()->setContextProperty("userVm", &userVm);
    engine.rootContext()->setContextProperty("projectsVm", &projectsVm);
    engine.rootContext()->setContextProperty("historyVm", &historyVm);
    engine.rootContext()->setContextProperty("keys", &keysProxy);
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    // 启动 DB 线程
    dbThread.start();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        dbThread.quit();
        dbThread.wait();
        db->deleteLater();
    });

    return app.exec();
}
