#ifndef MAINVIEWMODEL_H_
#define MAINVIEWMODEL_H_

#include <QObject>
#include <QVector>

class IIODeviceController;

class MainViewModel : public QObject {
    Q_OBJECT
public:
    explicit MainViewModel(QObject* parent = nullptr);

public slots:
    void startReading();
    void stopReading();

signals:
    // 直接给 QML 的批量数据
    void newDataBatch(const QVector<double>& values);

private:
    IIODeviceController* deviceController{nullptr};
};
#endif  // MAINVIEWMODEL_H_