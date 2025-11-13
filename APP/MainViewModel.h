#ifndef MAINVIEWMODEL_H_
#define MAINVIEWMODEL_H_

#include <QObject>
#include <QQueue>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVector>
#include <mutex>

class IIODeviceController;

class MainViewModel : public QObject {
    Q_OBJECT
public:
    explicit MainViewModel(QObject* parent = nullptr);
    ~MainViewModel();

public slots:
    void startReading();
    void stopReading();
    void setCurrentSample(const QString& sampleNo);
    Q_INVOKABLE QVariantList getAdcDataBySample(const QString& sampleNo);
    Q_INVOKABLE QVariantList getAdcData(const QString& sampleNo);
signals:
    void newDataBatch(const QVector<double>& values);

private slots:
    void onNewAdcData(const QVector<double>& values);
    void flushBufferToDb();  // 唤醒数据库线程写入

private:
    IIODeviceController* deviceController{nullptr};
    QString readerConnName_ = "reader";
    void initReaderDb();
    QString currentSampleNo_;
    QVector<double> buffer_;
    QTimer flushTimer_;

    // === 新增：数据库线程 ===
    QThread dbThread_;
    std::mutex queueMutex_;
    QQueue<QVector<double>> writeQueue_;  // 等待写入队列

    void dbWriterLoop();  // 数据库后台写入函数
};

#endif  // MAINVIEWMODEL_H_
