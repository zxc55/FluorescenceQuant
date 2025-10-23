#pragma once

#include <QObject>
#include <QTimer>
#include <QMutex>

class IIOReaderThread : public QObject
{ 
    Q_OBJECT
public:

    IIOReaderThread(QObject *parent = nullptr);

    ~IIOReaderThread();

    void start(int samplesPerRead = 50, int readInterval = 10);

    void stop();

signals:
    
    void dataRead(double value);
    void batchFinished();

private slots:
    void onTimeout();

private:

    bool setupIIODevice();

    void  cleanupIIODevice();

    QTimer *timer;

    QMutex mtx;

    int fd;

    bool running;

    int samplespPerRead;
};


