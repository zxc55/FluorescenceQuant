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

// 你的工程头文件
#include "CardWatcherStd.h"
#include "IIODeviceController.h"  // 如不需要可删
#include "KeysProxy.h"
#include "MainViewModel.h"
#include "MotorController.h"
#include "PrinterManager.h"

// —— SQLite / ViewModels / DB 线程 —— //
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

    // —— 输入法/触摸相关 —— //
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_QPA_PLATFORM", QByteArray("linuxfb:tty=/dev/fb0:size=1024x600:mmsize=271x159,tslib=1"));
    qputenv("QT_QPA_FB_TSLIB", QByteArray("1"));
    qputenv("TSLIB_TSDEVICE", QByteArray("/dev/input/event3"));
    qunsetenv("QT_QPA_GENERIC_PLUGINS");  // 避免两套输入栈同时启用

    // —— Qt 插件路径（确保能找到 libqsqlite.so）—— //
    // 你设备上路径是 /usr/lib/arm-qt/plugins/sqldrivers/libqsqlite.so
    // 这里设置到 /usr/lib/arm-qt/plugins 即可
    qputenv("QT_PLUGIN_PATH", QByteArray("/usr/lib/arm-qt/plugins"));

    // —— 跨线程信号需要注册的类型 —— //
    qRegisterMetaType<AppSettingsRow>("AppSettingsRow");
    qRegisterMetaType<UserRow>("UserRow");
    qRegisterMetaType<QVector<UserRow>>("QVector<UserRow>");
    qRegisterMetaType<ProjectRow>("ProjectRow");
    qRegisterMetaType<QVector<ProjectRow>>("QVector<ProjectRow>");
    qRegisterMetaType<QVector<uint16_t>>("QVector<uint16_t>");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QVariantList>("QVariantList");

    // QML 可用的类型
    qmlRegisterType<MotorController>("Motor", 1, 0, "MotorController");
    qmlRegisterType<ProjectsViewModel>("App", 1, 0, "ProjectsViewModel");
    QApplication app(argc, argv);
    QApplication::setOverrideCursor(Qt::BlankCursor);
    qInfo() << "UI thread id =" << QThread::currentThread();

    // ================ 路径 ================
    const QString dbDir = "/mnt/SDCARD/app/db";
    const QString dbPath = dbDir + "/app.db";
    ensureDir(dbDir);

    // ================ 可选：卡在位检测 ================
    KeysProxy keysProxy;
    const QString devArg = (argc >= 2) ? QString::fromLocal8Bit(argv[1]) : QString();
    CardWatcherStd cardWatcher(&keysProxy, devArg.toStdString());
    cardWatcher.setWatchedCode(KEY_PROG1);  // 148
    cardWatcher.setDebounceMs(20);
    if (!cardWatcher.start()) {
        qWarning() << "[CardWatcherStd] start failed";
    }

    // ======================= DB 线程与 Worker =======================
    DBWorker* db = new DBWorker(dbPath);
    QThread dbThread;
    db->moveToThread(&dbThread);

    // 线程启动后，进入 DBWorker::initialize()（仅在 DB 线程里打开连接）
    QObject::connect(&dbThread, &QThread::started, db, &DBWorker::initialize, Qt::QueuedConnection);

    QObject::connect(db, &DBWorker::ready, []() {
        qInfo() << "[DB] schema ready";
    });
    QObject::connect(db, &DBWorker::errorOccurred, [](const QString& m) {
        qWarning() << "[DB] error:" << m;
    });

    // ======================= ViewModels =======================
    MainViewModel mainVm;
    SettingsViewModel settingsVm;
    UserViewModel userVm;
    ProjectsViewModel projectsVm(db);  // 你的构造是 ProjectsViewModel(DBWorker*,...)

    // 绑定 DBWorker（连接信号/槽，不直接访问数据库）
    settingsVm.bindWorker(db);
    userVm.bindWorker(db);
    // ProjectsViewModel 已通过构造注入了 db，无需再次 bind，如你类里有 bind 可再调一次也行

    // 只有在 DB ready 后，再触发加载（避免“db not open”）
    QObject::connect(db, &DBWorker::ready, [db]() {
        db->postLoadSettings();
        db->postLoadUsers();
        db->postLoadProjects();
    });

    // =================== 打印机初始化（可选） ===================
    PrinterManager& printer = PrinterManager::instance();
    printer.initPrinter();

    // =================== QSS 样式 ===================
    QFile qss(":/styles/main.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
        qInfo() << "Load stylesheet success.";
    } else {
        qWarning() << "Load stylesheet failed.";
    }

    // =================== 注入到 QML ===================
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("mainViewModel", &mainVm);
    engine.rootContext()->setContextProperty("settingsVm", &settingsVm);
    engine.rootContext()->setContextProperty("userVm", &userVm);
    engine.rootContext()->setContextProperty("projectsVm", &projectsVm);
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

    // 启动 DB 线程（initialize -> ready）
    dbThread.start();

    // 退出时收尾
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        dbThread.quit();
        dbThread.wait();
        db->deleteLater();
    });

    return app.exec();
}
