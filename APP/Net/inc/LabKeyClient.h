#ifndef LABKEYCLIENT_H_
#define LABKEYCLIENT_H_

#include <QJsonObject>
#include <QObject>
#include <QVariantMap>
#include <mutex>
#include <string>
class LabKeyClientCurl {
public:
    LabKeyClientCurl(const std::string& baseUrl,
                     const std::string& projectPath,
                     const std::string& authBasic,
                     const std::string& cookieFile);

    bool fetchToken(std::string& outCsrf, std::string& err);
    bool uploadRun(const QVariantMap& record,
                   int* httpStatus,
                   QString* rawResp,
                   QString* err,
                   QString* outJson);

    bool fetchMethodLibrary(const std::string& type,
                            const std::string& serial,
                            QJsonObject& outResp,
                            std::string& err);

private:
    bool httpGet(const std::string& url,
                 std::string& response,
                 std::string& err);

    bool httpPost(const std::string& url,
                  const std::string& body,
                  std::string& response,
                  std::string& err);

private:
    std::string m_baseUrl;
    std::string m_projectPath;
    std::string m_authBasic;
    std::string m_cookieFile;
    std::string m_csrf;
    std::mutex m_mutex;
};

#endif  // LABKEYCLIENT
