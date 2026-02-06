// main.cpp
#include <linux/input-event-codes.h>

#include <QApplication>
#include <QCoreApplication>
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
#include "OtaManager.h"
#include "ProjectsViewModel.h"
#include "SettingsViewModel.h"
#include "UserViewModel.h"
// 扫码
#include "APP/Scanner/QrImageProvider.h"
#include "APP/Scanner/QrScanner.h"
#include "DeviceManager.h"
// 状态对象
#include <WiFiController.h>
#include <fcntl.h>        // open
#include <linux/input.h>  // EVIOCGNAME
#include <sys/ioctl.h>    // ioctl
#include <unistd.h>       // close

#include <QJsonDocument>
#include <QRegularExpression>

#include "DeviceService.h"
#include "DeviceStatusObject.h"
#include "LabKeyClient.h"
#include "LabKeyService.h"
#include "QrMethodConfigViewModel.h"  // 新增：方法配置表的 ViewModel 头文件（每行注释）
#include "QrRepoModel.h"
#include "TaskQueueWorker.h"
static bool ensureDir(const QString& path) {
    QDir d;
    return d.exists(path) ? true : d.mkpath(path);
}
// static QString getTouchFromScript() {
//     QProcess p;
//     p.start("/usr/bin/find_touch.sh");
//     if (!p.waitForFinished(2000))
//         return QString();

//     QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
//     // out = "Touch device: /dev/input/event3\n"

//     int idx = out.indexOf("/dev/input/");
//     if (idx < 0)
//         return QString();

//     QString dev = out.mid(idx).trimmed();
//     return dev;
// }
static QString detectTouchEventDevice()  // 扫描触摸 event 设备节点
{
    QDir dir("/dev/input");                            // 指向输入设备目录
    if (!dir.exists()) {                               // 如果目录不存在
        qWarning() << "[Touch] /dev/input not exist";  // 打印错误
        return QString();                              // 返回空
    }

    QStringList nodes = dir.entryList(QStringList() << "event*",   // 获取所有 event* 节点
                                      QDir::System | QDir::Files,  // 只取系统文件与普通文件
                                      QDir::Name);                 // 按名字排序

    for (const QString& node : nodes) {       // 遍历每一个 event 节点
        QString path = "/dev/input/" + node;  // 拼出完整路径

        int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);  // 非阻塞打开，避免卡死
        if (fd < 0) {                                                            // 打开失败
            continue;                                                            // 跳过
        }

        char name[256] = {0};                                 // 设备名缓存
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {  // 读取设备名
            ::close(fd);                                      // 关闭 fd
            continue;                                         // 跳过
        }

        ::close(fd);  // 关闭 fd

        QString devName = QString::fromLocal8Bit(name).toLower().trimmed();  // 转小写便于匹配

        if (devName.contains("gt9") || devName.contains("goodix") ||            // 匹配 gt9xx/gt911/goodix
            devName.contains("touch") || devName.contains("ts")) {              // 兜底匹配 touch/ts
            qDebug() << "[Touch] matched name=" << devName << "path=" << path;  // 打印匹配结果
            return path;                                                        // 返回触摸节点
        }
    }

    qWarning() << "[Touch] no touch event found";  // 没找到触摸设备
    return QString();                              // 返回空
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
    qDebug() << "---------------version : 1.0.4--------------";
#ifndef LOCAL_BUILD
    // 输入法 / 触摸驱动
    qputenv("QT_QPA_PLATFORM",
            QByteArray("linuxfb:tty=/dev/fb0:size=1024x600:mmsize=271x159,tslib=1"));
    QString touchDev = detectTouchEventDevice();

    if (!touchDev.isEmpty()) {
        qputenv("TSLIB_TSDEVICE", touchDev.toUtf8());
        qDebug() << "Use touch:" << touchDev;
    } else {
        qDebug() << "Touch not found, fallback";
    }

    qputenv("QT_QPA_PLATFORM", "linuxfb");
    qputenv("QT_QPA_FB_TSLIB", "1");

    //   qputenv("TSLIB_TSDEVICE", "/dev/input/event3");
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
    qRegisterMetaType<QrMethodConfigRow>("QrMethodConfigRow");                    // 新增：方法配置行结构体元类型注册（每行注释）
    qRegisterMetaType<QVector<QrMethodConfigRow>>("QVector<QrMethodConfigRow>");  // 新增：方法配置行列表元类型注册（每行注释）
    qRegisterMetaType<QrMethodConfigRow>("QrMethodConfigRow");                    // 新增：方法配置行（每行注释）
    qRegisterMetaType<QVector<QrMethodConfigRow>>("QVector<QrMethodConfigRow>");  // 新增：方法配置行列表（每行注释）

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
    LabKeyService* labkeyService = new LabKeyService(&app);

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
    // QrMethodConfigViewModel qrMethodConfigVm(db);  // 新增：方法配置表 ViewModel（每行注释）
    auto* qrMethodConfigVm = new QrMethodConfigViewModel(db, &mainVm);
    PrinterDeviceController printerCtrl(&settingsVm);
    QrRepoModel* qrRepoModel = new QrRepoModel(db);
    mainVm.setMethodConfigVm(qrMethodConfigVm);
    // ==== 绑定 DB ====
    settingsVm.bindWorker(db);
    userVm.bindWorker(db);

    // DB 初始化后加载数据
    QObject::connect(db, &DBWorker::ready, [&]() {
        db->postLoadSettings();
        db->postLoadUsers();
        db->postLoadProjects();
        db->postLoadHistory();
        db->postLoadQrMethodConfigs();
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

    // LabKeyService labkeyService;
    // ======================
    // 加载 QSS
    // ======================
    QFile qss(":/styles/main.qss");
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(qss.readAll());
    // === 创建 WiFiController 实例 ===
    // 生命周期由 main 管理，整个应用唯一
    WiFiController wifiController;
    OtaManager otaManager;
    //
    DeviceManager* deviceMgr = new DeviceManager(&app);
    deviceMgr->start();
    // ======================
    // QML 引擎
    // ======================
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("mainViewModel", &mainVm);
    engine.rootContext()->setContextProperty("settingsVm", &settingsVm);
    engine.rootContext()->setContextProperty("userVm", &userVm);
    engine.rootContext()->setContextProperty("projectsVm", &projectsVm);
    engine.rootContext()->setContextProperty("qrMethodConfigVm", qrMethodConfigVm);
    engine.rootContext()->setContextProperty("historyVm", &historyVm);
    engine.rootContext()->setContextProperty("keys", &keysProxy);
    engine.rootContext()->setContextProperty("qrScanner", &qrScanner);
    engine.rootContext()->setContextProperty("printerCtrl", &printerCtrl);
    engine.rootContext()->setContextProperty("deviceService", deviceMgr->service());
    engine.rootContext()->setContextProperty("dbWorker", qrRepoModel);
    engine.rootContext()->setContextProperty("labkeyService", labkeyService);
    engine.rootContext()->setContextProperty("wifiController", &wifiController);
    engine.rootContext()->setContextProperty("otaManager", &otaManager);
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
