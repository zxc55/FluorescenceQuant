#include "LabKeyClient.h"

#include <curl/curl.h>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <QVariantMap>
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

    // =========================
    // ★ 明确 POST（等价 -X POST）
    // =========================
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // =========================
    // ★ 等价 --data-binary
    // =========================
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());

    // =========================
    // ★ 关键：支持 302 / 303 重定向
    // 等价：--location --post302 --post303
    // =========================
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTREDIR,
                     CURL_REDIR_POST_302 | CURL_REDIR_POST_303);

    // =========================
    // Response
    // =========================
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // =========================
    // SSL (-k)
    // =========================
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // =========================
    // Cookie (-b / -c)
    // =========================
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, m_cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, m_cookieFile.c_str());

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        err = curl_easy_strerror(rc);
        curl_easy_cleanup(curl);
        return false;
    }

    // ★ 打印 HTTP code，便于你判断
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    qInfo() << "[LabKey] HTTP code =" << httpCode;

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
bool LabKeyClientCurl::uploadRun(const QVariantMap& record,
                                 int* httpStatus,
                                 QString* rawResp,
                                 QString* err,
                                 QString* outJson) {
    // =========================
    // 1) 组 JSON
    // =========================
    QJsonObject row;
    row["company"] = record.value("company").toString();
    row["sample"] = record.value("sample").toString();
    row["T_Value"] = record.value("T_Value").toDouble();
    row["C_Value"] = record.value("C_Value").toDouble();
    row["T/C"] = record.value("T/C").toDouble();
    row["concentration"] = record.value("concentration").toDouble();
    row["result"] = record.value("result").toString();
    row["date"] = record.value("date").toString();
    row["project"] = record.value("project").toString();
    row["serial"] = record.value("serial").toString();
    row["CurveFormula"] = record.value("CurveFormula").toString();
    row["DilutionFactor"] = record.value("DilutionFactor").toDouble();

    QJsonArray dataRows;
    dataRows.append(row);

    QJsonObject root;
    root["assayId"] = record.value("assayId").toInt();
    root["name"] = record.value("name").toString();
    root["dataRows"] = dataRows;

    QJsonDocument doc(root);
    QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    if (outJson)
        *outJson = jsonStr;  // ★ 回传给 Service

    // =========================
    // 2) HTTP POST
    // =========================
    std::string response;
    std::string httpErr;

    std::string url =
        m_baseUrl +
        "/QuantitativeFluorescence/assay-importRun.api";

    bool ok = httpPost(url, jsonStr.toStdString(), response, httpErr);

    if (!ok) {
        if (err)
            *err = QString::fromStdString(httpErr);
        if (httpStatus)
            *httpStatus = -1;
        return false;
    }

    if (rawResp)
        *rawResp = QString::fromStdString(response);

    // =========================
    // 3) 解析返回
    // =========================
    QJsonParseError pe;
    QJsonDocument respDoc =
        QJsonDocument::fromJson(QByteArray::fromStdString(response), &pe);

    if (pe.error != QJsonParseError::NoError || !respDoc.isObject()) {
        if (httpStatus)
            *httpStatus = -1;
        return false;
    }

    QJsonObject respObj = respDoc.object();

    const bool success = respObj.value("success").toBool(false);

    // 可选日志信息
    const int runId = respObj.value("runId").toInt(-1);
    const int batchId = respObj.value("batchId").toInt(-1);
    const QString successUrl = respObj.value("successurl").toString();

    qInfo() << "[UPLOAD] success =" << success
            << "runId =" << runId
            << "batchId =" << batchId
            << "url =" << successUrl;

    // ★ 唯一成功判定
    return success;
}