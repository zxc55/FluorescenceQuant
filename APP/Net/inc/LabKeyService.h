#ifndef LABKEYSERVICE_H
#define LABKEYSERVICE_H

#include <QJsonObject>
#include <QObject>

#include "LabKeyClient.h"
#include "TaskQueueWorker.h"

class LabKeyService : public QObject {
    Q_OBJECT
public:
    explicit LabKeyService(QObject* parent = nullptr);
    ~LabKeyService();

    // QML 只调用这一个
    Q_INVOKABLE void fetchMethodLibrary(const QString& type,
                                        const QString& serial);

signals:
    void methodLibraryReady(const QJsonObject& data);
    void errorOccured(const QString& msg);

private:
    TaskQueueWorker m_worker;
    LabKeyClientCurl* m_client{nullptr};
};

#endif
