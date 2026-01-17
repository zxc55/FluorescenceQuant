#include "LabKeyService.h"

LabKeyService::LabKeyService(QObject* parent)
    : QObject(parent) {
    m_worker.start();

    m_client = new LabKeyClientCurl(
        "https://lims.pribolab.net:13101",
        "/QuantitativeFluorescence",
        "Basic YXBpa2V5OmQ3NzNlOTNiZDI0NGJmMDA5MzU0MWQzZDY4YWNiMjU2ODA5NGJmNjA3ZjMxNzdiNmZkYjBiZjQ0NjBlMzQ5MjM=",
        "/tmp/labkey_cookie.txt");
}

LabKeyService::~LabKeyService() {
    m_worker.stop();
    delete m_client;
}

/*
 * QML 调用：
 *   - 自动保证 CSRF 可用
 *   - QML 不感知 token 逻辑
 */
void LabKeyService::fetchMethodLibrary(const QString& type,
                                       const QString& serial) {
    m_worker.pushTask([this, type, serial]() {
        std::string err;
        QJsonObject obj;

        /* 1️⃣ 先尝试直接获取 */
        if (m_client->fetchMethodLibrary(type.toStdString(),
                                         serial.toStdString(),
                                         obj,
                                         err)) {
            emit methodLibraryReady(obj);
            return;
        }

        std::string csrf;
        if (!m_client->fetchToken(csrf, err)) {
            emit errorOccured(QString::fromStdString(err));
            return;
        }
        /* 3️⃣ 再试一次 */
        if (!m_client->fetchMethodLibrary(type.toStdString(),
                                          serial.toStdString(),
                                          obj,
                                          err)) {
            emit errorOccured(QString::fromStdString(err));
            return;
        }

        emit methodLibraryReady(obj);
    });
}
