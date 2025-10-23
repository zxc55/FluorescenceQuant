#include "IIOReaderThread.h"
#include <QDebug>
#include <QProcess>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define IIO_PATH "/sys/bus/iio/devices/iio:device0"
#define IIO_DEV  "/dev/iio:device0"
IIOReaderThread::IIOReaderThread(QObject *parent)
    : QObject(parent),timer(nullptr),fd(-1),running(false),samplespPerRead(10)
{
    qDebug() << "IIOReaderThread::IIOReaderThread()";
    setupIIODevice();
    fd = open(IIO_DEV,O_RDONLY);
    if(fd < 0)
    {
        qDebug() << "open IIO device error:" << strerror(errno);
        return;
    }
    else
    {
        qDebug() << "open IIO device success";
    }
}

IIOReaderThread::~IIOReaderThread()
{
    qDebug() << "IIOReaderThread::~IIOReaderThread()";
    stop();
    cleanupIIODevice();
    if (fd >= 0)
    {
        close(fd);
    }
    
}

void IIOReaderThread::start(int intervalMs, int samplesPerRead)
{
    QMutexLocker locker(&mtx);
    if(running)return;

    this->samplespPerRead = samplesPerRead;
    running = true;
    if(!timer)
    {
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &IIOReaderThread::onTimeout);
        timer->setInterval(Qt::PreciseTimer);
    }
}

void IIOReaderThread::stop()
{
    QMutexLocker locker(&mtx);
    running = false;
    if (timer && timer->isActive())
    {
        timer->stop();
        emit batchFinished();
        qDebug() << "ADS1115 采集停止";
    }
    
}

bool IIOReaderThread::setupIIODevice()
{ 
    qDebug() << "配置 IIO buffer";
    QProcess::execute(QString("echo 1 > %1/scan_elements/in_voltage0_en").arg(IIO_PATH));
    QProcess::execute(QString("echo irqtrig172 > %1/trigger/current_trigger").arg(IIO_PATH));
    QProcess::execute(QString("echo 8 > %1/buffer/length").arg(IIO_PATH));
    QProcess::execute(QString("echo 1 > %1/buffer/enable").arg(IIO_PATH));
     return true;
}

void IIOReaderThread::cleanupIIODevice()
{ 
    QProcess::execute(QString("echo 0 > %1/buffer/enable").arg(IIO_PATH));
    qDebug() << "关闭 IIO buffer";
}