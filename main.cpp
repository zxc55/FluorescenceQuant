#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QThread>
#include <QVariantList>

#include "APP/MainViewModel.h"
#include "ModbusWorkerThread.h"
#include "MotorController.h"
#include "PrinterManager.h"

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
    QApplication app(argc, argv);
    QApplication::setOverrideCursor(Qt::BlankCursor);

    // ✅ 元类型注册（QML 才能收 newDataBatch）
    qRegisterMetaType<QVector<uint16_t>>("QVector<uint16_t>");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QVariantList>("QVariantList");

    // ✅ 注册 MotorController 给 QML 使用
    qmlRegisterType<MotorController>("Motor", 1, 0, "MotorController");

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

    engine.rootContext()->setContextProperty("mainViewModel", &viewModel);
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
