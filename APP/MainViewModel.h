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

#include "QrMethodConfigViewModel.h"
class IIODeviceController;

class MainViewModel : public QObject {
    Q_OBJECT
public:
    explicit MainViewModel(QObject* parent = nullptr);
    ~MainViewModel();
    Q_INVOKABLE QVariantMap calcTC(const QVariantList& adcList, int id);
    // Q_INVOKABLE QVariantMap calcTC_FixedWindow(const QVariantList& adcList);
    struct FourPLParams {
        double A;  // 曲线高端（低浓度）
        double B;  // 斜率
        double C;  // 中点浓度
        double D;  // 曲线低端（高浓度）
        int C1;
        int C2;
        int T1;
        int T2;
    };
    void setMethodConfigVm(QrMethodConfigViewModel* vm);
public slots:
    void startReading();
    void stopReading();
    void setCurrentSample(const QString& sampleNo);
    Q_INVOKABLE QVariantList getAdcDataBySample(const QString& sampleNo);
    Q_INVOKABLE QVariantList getAdcData(const QString& sampleNo);
    Q_INVOKABLE QString generateSampleNo();

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
    QrMethodConfigViewModel* m_methodVm = nullptr;
    void dbWriterLoop();  // 数据库后台写入函数
};

#endif  // MAINVIEWMODEL_H_
