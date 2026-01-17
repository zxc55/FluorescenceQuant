// main.cpp
#include <linux/input-event-codes.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMetaType>
#include <QProcess>
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
#include "printerDeviceController.h"
// SQLite 组件
#include <QQmlContext>

#include "DBWorker.h"
#include "DTO.h"
#include "ProjectsViewModel.h"
#include "SettingsViewModel.h"
#include "UserViewModel.h"
// 扫码
#include "APP/Scanner/QrImageProvider.h"
#include "APP/Scanner/QrScanner.h"
#include "DeviceManager.h"
// 状态对象
#include <QJsonDocument>

#include "DeviceService.h"
#include "DeviceStatusObject.h"
#include "LabKeyClient.h"
#include "LabKeyService.h"
#include "QrRepoModel.h"
#include "TaskQueueWorker.h"
static bool ensureDir(const QString& path) {
    QDir d;
    return d.exists(path) ? true : d.mkpath(path);
}
static QString getTouchFromScript() {
    QProcess p;
    p.start("/usr/bin/find_touch.sh");
    if (!p.waitForFinished(2000))
        return QString();

    QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    // out = "Touch device: /dev/input/event3\n"

    int idx = out.indexOf("/dev/input/");
    if (idx < 0)
        return QString();

    QString dev = out.mid(idx).trimmed();
    return dev;
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
    QString touchDev = getTouchFromScript();

    if (!touchDev.isEmpty()) {
        qputenv("TSLIB_TSDEVICE", touchDev.toUtf8());
        qDebug() << "Use touch:" << touchDev;
    } else {
        qDebug() << "Touch not found, fallback";
    }

    qputenv("QT_QPA_PLATFORM", "linuxfb");
    qputenv("QT_QPA_FB_TSLIB", "1");

    // qputenv("TSLIB_TSDEVICE", "/dev/input/event4");
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
    qRegisterMetaType<PrintData>("PrintData");

    // ======================
    // 注册 QML 类型
    // ======================
    qmlRegisterType<MotorController>("Motor", 1, 0, "MotorController");
    qmlRegisterType<ProjectsViewModel>("App", 1, 0, "ProjectsViewModel");
    qmlRegisterType<SeriesFeeder>("App", 1, 0, "SeriesFeeder");
    qmlRegisterType<QrScanner>("App", 1, 0, "QrScanner");
    qmlRegisterType<MyLineSeries>("App", 1, 0, "MyLineSeries");
    qmlRegisterType<CurveLoader>("App", 1, 0, "CurveLoader");
    qmlRegisterType<DeviceService>("App", 1, 0, "DeviceService");
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

    /* 1. 启动后台任务线程 */
    TaskQueueWorker worker;
    worker.start();

    /* 2. 创建 LabKey libcurl 客户端
     *   - 不继承 QObject
     *   - 不存在线程亲缘性问题
     *   - 可以安全放在 std::thread 里使用
     */
    auto* labkey = new LabKeyClientCurl(
        "https://lims.pribolab.net:13101",
        "/QuantitativeFluorescence",
        "Basic YXBpa2V5OmQ3NzNlOTNiZDI0NGJmMDA5MzU0MWQzZDY4YWNiMjU2ODA5NGJmNjA3ZjMxNzdiNmZkYjBiZjQ0NjBlMzQ5MjM=",
        "/tmp/labkey_cookie.txt"  // cookie 文件，等价 curl -c / -b
    );

    // /* 3. 投递 LabKey 任务到后台线程 */
    // worker.pushTask([labkey]() {
    //     qDebug() << "[TASK] LabKey start";

    //     std::string err;
    //     std::string csrf;
    //     QJsonObject json;

    //     /* 3.1 获取 CSRF Token */
    //     if (!labkey->fetchToken(csrf, err)) {
    //         qWarning() << "[TASK] fetchToken failed:" << err.c_str();
    //         return;
    //     }

    //     qDebug() << "[TASK] CSRF =" << csrf.c_str();

    //     /* 3.2 获取 MethodLibrary */
    //     if (!labkey->fetchMethodLibrary("test", "1", json, err)) {
    //         qWarning() << "[TASK] fetchMethodLibrary failed:" << err.c_str();
    //     } else {
    //         qDebug() << "[TASK] MethodLibrary:";
    //         qDebug().noquote()
    //             << QJsonDocument(json).toJson(QJsonDocument::Indented);
    //     }

    //     /* 3.3 上传实验数据（失败自动刷新 CSRF 再试） */
    //     if (!labkey->uploadRunWithRetry("/root/labkey_test/data.json",
    //                                     json, err)) {
    //         qWarning() << "[TASK] uploadRun failed:" << err.c_str();
    //     } else {
    //         qDebug() << "[TASK] uploadRun success:";
    //         qDebug().noquote()
    //             << QJsonDocument(json).toJson(QJsonDocument::Indented);
    //     }

    //     qDebug() << "[TASK] LabKey finished";
    // });

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
    // F4 modbus 设备控制
    // ======================
    DeviceManager* deviceMgr = new DeviceManager(&app);
    deviceMgr->start();

    // ======================
    // DBWorker + QThread
    // ======================
    QThread* dbThread = new QThread();
    DBWorker* db = new DBWorker(dbPath);
    //   db->initialize();
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
    PrinterDeviceController printerCtrl(&settingsVm);
    QrRepoModel* qrRepoModel = new QrRepoModel(db);
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
    printerCtrl.start();
    // ======================
    // QrScanner
    // ======================
    QrScanner qrScanner;
    QObject::connect(db, &DBWorker::settingsLoaded,
                     &settingsVm, &SettingsViewModel::onSettingsLoaded,
                     Qt::QueuedConnection);
    LabKeyService labkeyService;
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
    engine.rootContext()->setContextProperty("printerCtrl", &printerCtrl);
    engine.rootContext()->setContextProperty("deviceService", deviceMgr->service());
    engine.rootContext()->setContextProperty("dbWorker", qrRepoModel);
    engine.rootContext()->setContextProperty("labkeyService", &labkeyService);
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
