#include "LabKeyClient.h"

#include <curl/curl.h>

#include <QJsonDocument>
#include <fstream>
#include <sstream>

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* s = static_cast<std::string*>(userdata);
    s->append(ptr, size * nmemb);
    return size * nmemb;
}

LabKeyClientCurl::LabKeyClientCurl(const std::string& baseUrl,
                                   const std::string& projectPath,
                                   const std::string& authBasic,
                                   const std::string& cookieFile)
    : m_baseUrl(baseUrl),
      m_projectPath(projectPath),
      m_authBasic(authBasic),
      m_cookieFile(cookieFile) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

bool LabKeyClientCurl::httpGet(const std::string& url,
                               std::string& response,
                               std::string& err) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        err = "curl_easy_init failed";
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers,
                                ("Authorization: " + m_authBasic).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // -k
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, m_cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, m_cookieFile.c_str());

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        err = curl_easy_strerror(rc);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);
    return true;
}

bool LabKeyClientCurl::httpPost(const std::string& url,
                                const std::string& body,
                                std::string& response,
                                std::string& err) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        err = "curl_easy_init failed";
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers,
                                ("Authorization: " + m_authBasic).c_str());
    headers = curl_slist_append(headers,
                                ("X-LABKEY-CSRF: " + m_csrf).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // -k
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, m_cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, m_cookieFile.c_str());

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        err = curl_easy_strerror(rc);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);
    return true;
}

bool LabKeyClientCurl::fetchToken(std::string& outCsrf, std::string& err) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string resp;
    if (!httpGet(m_baseUrl + "/login-whoAmI.api", resp, err))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(resp));

    QJsonObject obj = doc.object();
    if (!obj.value("success").toBool())
        return false;

    m_csrf = obj.value("CSRF").toString().toStdString();
    outCsrf = m_csrf;
    return true;
}

bool LabKeyClientCurl::uploadRunWithRetry(const std::string& jsonFile,
                                          QJsonObject& outResp,
                                          std::string& err) {
    std::ifstream ifs(jsonFile);
    if (!ifs.is_open()) {
        err = "open json file failed";
        return false;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();

    std::string resp;
    if (httpPost(m_baseUrl + m_projectPath + "/assay-importRun.api",
                 ss.str(), resp, err)) {
        outResp = QJsonDocument::fromJson(
                      QByteArray::fromStdString(resp))
                      .object();
        if (outResp.value("success").toBool())
            return true;
    }

    if (!fetchToken(m_csrf, err))
        return false;

    resp.clear();
    bool ok = httpPost(m_baseUrl + m_projectPath + "/assay-importRun.api",
                       ss.str(), resp, err);
    outResp = QJsonDocument::fromJson(
                  QByteArray::fromStdString(resp))
                  .object();
    return ok;
}

bool LabKeyClientCurl::fetchMethodLibrary(const std::string& type,
                                          const std::string& serial,
                                          QJsonObject& outResp,
                                          std::string& err) {
    std::string url =
        m_baseUrl + m_projectPath +
        "/query-selectRows.api?schemaName=lists"
        "&query.queryName=MethodLibrary"
        "&query.type~eq=" +
        type +
        "&query.serial~eq=" + serial +
        "&includeMetadata=false&includeTotalCount=false";

    std::string resp;
    if (!httpGet(url, resp, err))
        return false;

    outResp = QJsonDocument::fromJson(
                  QByteArray::fromStdString(resp))
                  .object();
    return true;
}
