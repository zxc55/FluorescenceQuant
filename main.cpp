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

// 自定义组件
#include "APP/MyQmlComponents/MyCurveLoader/CurveLoader.h"
#include "APP/MyQmlComponents/MyLineSeries/MyLineSeries.h"
#include "APP/MyQmlComponents/MyPlotView/PlotView.h"
#include "APP/MyQmlComponents/MySeriesFeeder/SeriesFeeder.h"

// 工程组件
#include "CardWatcherStd.h"
#include "DecodeWorker.h"
#include "HistoryViewModel.h"
#include "IIODeviceController.h"
#include "KeysProxy.h"
#include "MainViewModel.h"
#include "MotorController.h"
#include "PrinterManager.h"
#include "V4L2MjpegGrabber.h"

// SQLite 组件
#include "DBWorker.h"
#include "DTO.h"
#include "ProjectsViewModel.h"
#include "SettingsViewModel.h"
#include "UserViewModel.h"

// 扫码
#include "APP/Scanner/QrImageProvider.h"
#include "APP/Scanner/QrScanner.h"

static bool ensureDir(const QString& path) {
    QDir d;
    return d.exists(path) ? true : d.mkpath(path);
}

// ===============================
// DPI & platform 设置修复
// ===============================
static void setupQtDpi() {
#ifndef LOCAL_BUILD
    qputenv("QT_QPA_PLATFORM",
            "linuxfb:tty=/dev/fb0:size=1024x600:mmsize=271x159");

    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qputenv("QT_SCALE_FACTOR", "1");
    qputenv("QT_FONT_DPI", "96");

    qunsetenv("QT_DEVICE_PIXEL_RATIO");
    qunsetenv("QT_SCREEN_SCALE_FACTORS");
#endif

    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
}

int main(int argc, char* argv[]) {
    setupQtDpi();
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

#ifndef LOCAL_BUILD
    // 输入法 / 触摸驱动
    qputenv("QT_QPA_PLATFORM",
            QByteArray("linuxfb:tty=/dev/fb0:size=1024x600:mmsize=271x159,tslib=1"));
    qputenv("QT_QPA_FB_TSLIB", "1");
    qputenv("TSLIB_TSDEVICE", "/dev/input/event4");
    qunsetenv("QT_QPA_GENERIC_PLUGINS");

    qputenv("QT_PLUGIN_PATH", "/usr/lib/arm-qt/plugins");
#endif

    QApplication app(argc, argv);
    QApplication::setOverrideCursor(Qt::BlankCursor);

    // ======================
    // 注册跨线程的元类型
    // ======================
    qRegisterMetaType<AppSettingsRow>("AppSettingsRow");
    qRegisterMetaType<UserRow>("UserRow");
    qRegisterMetaType<QVector<UserRow>>("QVector<UserRow>");
    qRegisterMetaType<ProjectRow>("ProjectRow");
    qRegisterMetaType<QVector<ProjectRow>>("QVector<ProjectRow>");
    qRegisterMetaType<QVector<uint16_t>>("QVector<uint16_t>");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QVariantList>("QVariantList");
    qRegisterMetaType<HistoryRow>("HistoryRow");
    qRegisterMetaType<QVector<HistoryRow>>("QVector<HistoryRow>");

    // ======================
    // 注册 QML 类型
    // ======================
    qmlRegisterType<MotorController>("Motor", 1, 0, "MotorController");
    qmlRegisterType<ProjectsViewModel>("App", 1, 0, "ProjectsViewModel");
    qmlRegisterType<SeriesFeeder>("App", 1, 0, "SeriesFeeder");
    qmlRegisterType<QrScanner>("App", 1, 0, "QrScanner");
    qmlRegisterType<MyLineSeries>("App", 1, 0, "MyLineSeries");
    qmlRegisterType<CurveLoader>("App", 1, 0, "CurveLoader");

    // ======================
    // DB 路径
    // ======================
#ifndef LOCAL_BUILD
    const QString dbDir = "/mnt/SDCARD/app/db";
#else
    const QString dbDir = "/home/pribolab/Project/FluorescenceQuant/debugDir";
#endif

    ensureDir(dbDir);
    QString dbPath = dbDir + "/app.db";

    // ======================
    // 卡检测 KeysProxy
    // ======================
    KeysProxy keysProxy;
    QString dev = (argc >= 2) ? QString::fromLocal8Bit(argv[1]) : "";
    CardWatcherStd cardWatcher(&keysProxy, dev.toStdString());
    cardWatcher.setWatchedCode(KEY_PROG1);
    cardWatcher.setDebounceMs(20);
    cardWatcher.start();

    // ======================
    // DBWorker + QThread
    // ======================
    QThread* dbThread = new QThread();
    DBWorker* db = new DBWorker(dbPath);

    db->moveToThread(dbThread);
    QObject::connect(dbThread, &QThread::started, db, &DBWorker::initialize);
    QObject::connect(dbThread, &QThread::finished, db, &DBWorker::deleteLater);
    dbThread->start();

    // ======================
    // ViewModel 初始化
    // ======================
    MainViewModel mainVm;
    SettingsViewModel settingsVm;  // 允许延迟绑定
    UserViewModel userVm;
    ProjectsViewModel projectsVm(db);
    HistoryViewModel historyVm(db);

    // ==== 绑定 DB ====
    settingsVm.bindWorker(db);
    userVm.bindWorker(db);

    // DB 初始化后加载数据
    QObject::connect(db, &DBWorker::ready, [&]() {
        db->postLoadSettings();
        db->postLoadUsers();
        db->postLoadProjects();
        db->postLoadHistory();
    });

    // ======================
    // 打印机
    // ======================
    PrinterManager::instance().initPrinter();

    // ======================
    // QrScanner
    // ======================
    QrScanner qrScanner;
    QObject::connect(db, &DBWorker::settingsLoaded,
                     &settingsVm, &SettingsViewModel::onSettingsLoaded,
                     Qt::QueuedConnection);
    // ======================
    // 加载 QSS
    // ======================
    QFile qss(":/styles/main.qss");
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(qss.readAll());

    // ======================
    // QML 引擎
    // ======================
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("mainViewModel", &mainVm);
    engine.rootContext()->setContextProperty("settingsVm", &settingsVm);
    engine.rootContext()->setContextProperty("userVm", &userVm);
    engine.rootContext()->setContextProperty("projectsVm", &projectsVm);
    engine.rootContext()->setContextProperty("historyVm", &historyVm);
    engine.rootContext()->setContextProperty("keys", &keysProxy);
    engine.rootContext()->setContextProperty("qrScanner", &qrScanner);

    engine.addImageProvider("qr", new QrImageProvider(&qrScanner));

    QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
