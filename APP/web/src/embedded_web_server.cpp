#include "embedded_web_server.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <set>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#include "httplib.h"

#ifndef APP_DEFAULT_WEB_ROOT
#define APP_DEFAULT_WEB_ROOT "/mnt/SDCARD/www"
#endif

#ifndef APP_DEFAULT_BROWSE_ROOT
#define APP_DEFAULT_BROWSE_ROOT "/mnt/SDCARD"
#endif

#ifndef APP_DB_PATH
#define APP_DB_PATH "/mnt/SDCARD/app/db/app.db"
#endif

namespace {

// ======================== 数据结构 ========================

struct ProjectInfoData {
    int id = 0;
    int project_id = 0;
    std::string project_name;
    std::string sample_no;
    std::string sample_source;
    std::string sample_name;
    std::string standard_curve;
    std::string batch_code;
    double detected_conc = 0.0;
    double reference_value = 0.0;
    std::string result;
    std::string detected_time;
    std::string detected_unit;
    std::string detected_person;
    std::string dilution_info;
    double c_value = 0.0;
    double t_value = 0.0;
    double ratio_value = 0.0;
};

// ======================== Qt SQLite 工具 ========================

static QString webDbConnectionName() {
    return QString("web_sqlite_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

static QSqlDatabase openWebDatabase(const std::string& db_path, QString& err) {
    const QString connName = webDbConnectionName();

    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (db.isOpen()) {
            return db;
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(QString::fromStdString(db_path));

    if (!db.open()) {
        err = db.lastError().text();
        qCritical() << "[Web][DB] open failed:" << err;
    }

    return db;
}

// ======================== 工具函数 ========================

std::string json_escape(const std::string& src) {
    std::string out;
    out.reserve(src.size() + 16);
    for (unsigned char c : src) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char tmp[8];
                std::snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                out += tmp;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    return out;
}

void send_json(httplib::Response& res, int status, const std::string& body) {
    res.status = status;
    res.set_header("Cache-Control", "no-cache");
    res.set_content(body, "application/json; charset=utf-8");
}

void send_json_error(httplib::Response& res, int http_status, int code,
                     const std::string& message) {
    std::ostringstream oss;
    oss << "{\"code\":" << code
        << ",\"message\":\"" << json_escape(message)
        << "\",\"data\":null}";
    send_json(res, http_status, oss.str());
}

int parse_int_safe(const std::string& s, int fallback) {
    if (s.empty())
        return fallback;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (!end || *end != '\0')
        return fallback;
    return static_cast<int>(v);
}

// ======================== 数据库查询 ========================

bool query_project_detail(const std::string& db_path,
                          const std::string& sample_no,
                          ProjectInfoData& out,
                          std::string& err) {
    QString qerr;
    QSqlDatabase db = openWebDatabase(db_path, qerr);
    if (!db.isOpen()) {
        err = qerr.toStdString();
        return false;
    }

    QSqlQuery q(db);
    q.prepare(
        "SELECT id, projectId, projectName, sampleNo, sampleSource, sampleName, "
        "standardCurve, batchCode, detectedConc, referenceValue, result, detectedTime, "
        "detectedUnit, detectedPerson, dilutionInfo, \"C\", \"T\", ratio "
        "FROM project_info WHERE sampleNo = ? "
        "ORDER BY id DESC LIMIT 1");
    q.addBindValue(QString::fromStdString(sample_no));
    qDebug() << "[Web][DB] query project detail:" << QString::fromStdString(sample_no);
    qDebug() << q.lastQuery();
    qDebug() << QString::fromStdString(db_path);
    if (!q.exec()) {
        err = q.lastError().text().toStdString();
        qDebug() << "[Web][DB] query project detail failed:" << q.lastError().text();
        return false;
    }
    if (!q.next()) {
        err = "not found";
        return false;
    }
    qDebug() << "----[Web][DB] query project detail result:---";

    out.id = q.value(0).toInt();
    out.project_id = q.value(1).toInt();
    out.project_name = q.value(2).toString().toStdString();
    out.sample_no = q.value(3).toString().toStdString();
    out.sample_source = q.value(4).toString().toStdString();
    out.sample_name = q.value(5).toString().toStdString();
    out.standard_curve = q.value(6).toString().toStdString();
    out.batch_code = q.value(7).toString().toStdString();
    out.detected_conc = q.value(8).toDouble();
    out.reference_value = q.value(9).toDouble();
    out.result = q.value(10).toString().toStdString();
    out.detected_time = q.value(11).toString().toStdString();
    out.detected_unit = q.value(12).toString().toStdString();
    out.detected_person = q.value(13).toString().toStdString();
    out.dilution_info = q.value(14).toString().toStdString();
    out.c_value = q.value(15).toDouble();
    out.t_value = q.value(16).toDouble();
    out.ratio_value = q.value(17).toDouble();

    return true;
}
void append_project_json(std::ostringstream& oss, const ProjectInfoData& p) {
    oss.setf(std::ios::fixed);
    oss.precision(3);
    oss << "{"
        << "\"id\":" << p.id << ','
        << "\"projectId\":" << p.project_id << ','
        << "\"projectName\":\"" << json_escape(p.project_name) << "\","
        << "\"sampleNo\":\"" << json_escape(p.sample_no) << "\","
        << "\"sampleSource\":\"" << json_escape(p.sample_source) << "\","
        << "\"sampleName\":\"" << json_escape(p.sample_name) << "\","
        << "\"standardCurve\":\"" << json_escape(p.standard_curve) << "\","
        << "\"batchCode\":\"" << json_escape(p.batch_code) << "\","
        << "\"detectedConc\":" << p.detected_conc << ','
        << "\"referenceValue\":" << p.reference_value << ','
        << "\"result\":\"" << json_escape(p.result) << "\","
        << "\"detectedTime\":\"" << json_escape(p.detected_time) << "\","
        << "\"detectedUnit\":\"" << json_escape(p.detected_unit) << "\","
        << "\"detectedPerson\":\"" << json_escape(p.detected_person) << "\","
        << "\"dilutionInfo\":\"" << json_escape(p.dilution_info) << "\","
        << "\"C\":" << p.c_value << ','
        << "\"T\":" << p.t_value << ','
        << "\"ratio\":" << p.ratio_value << ','
        << "\"radio\":" << p.ratio_value
        << "}";
}
// ======================== 路由注册 ========================
std::string get_param(const httplib::Request& req, const char* name,
                      const std::string& fallback = "") {
    if (req.has_param(name)) {
        return req.get_param_value(name);
    }
    return fallback;
}
bool query_project_list(const std::string& db_path,
                        const std::string& keyword,
                        int limit,
                        int offset,
                        long long& total,
                        std::vector<ProjectInfoData>& out,
                        std::string& err) {
    // ========= 1. 打开数据库（线程独立连接） =========
    const QString connName =
        QString("web_list_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    QSqlDatabase db;
    if (QSqlDatabase::contains(connName)) {
        db = QSqlDatabase::database(connName);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(QString::fromStdString(db_path));
    }

    if (!db.open()) {
        err = db.lastError().text().toStdString();
        return false;
    }

    // ========= 2. COUNT =========
    {
        QSqlQuery qc(db);
        if (keyword.empty()) {
            qc.prepare("SELECT COUNT(1) FROM project_info");
        } else {
            qc.prepare(
                "SELECT COUNT(1) FROM project_info "
                "WHERE sampleNo LIKE ? OR projectName LIKE ? OR sampleName LIKE ?");
            const QString like = "%" + QString::fromStdString(keyword) + "%";
            qc.addBindValue(like);
            qc.addBindValue(like);
            qc.addBindValue(like);
        }

        if (!qc.exec() || !qc.next()) {
            err = qc.lastError().text().toStdString();
            return false;
        }
        total = qc.value(0).toLongLong();
    }

    // ========= 3. 数据查询 =========
    QSqlQuery q(db);
    if (keyword.empty()) {
        q.prepare(
            "SELECT id, projectId, projectName, sampleNo, sampleSource, sampleName, "
            "standardCurve, batchCode, detectedConc, referenceValue, result, detectedTime, "
            "detectedUnit, detectedPerson, dilutionInfo, \"C\", \"T\", ratio "
            "FROM project_info "
            "ORDER BY id DESC LIMIT ? OFFSET ?");
        q.addBindValue(limit);
        q.addBindValue(offset);
    } else {
        q.prepare(
            "SELECT id, projectId, projectName, sampleNo, sampleSource, sampleName, "
            "standardCurve, batchCode, detectedConc, referenceValue, result, detectedTime, "
            "detectedUnit, detectedPerson, dilutionInfo, \"C\", \"T\", ratio "
            "FROM project_info "
            "WHERE sampleNo LIKE ? OR projectName LIKE ? OR sampleName LIKE ? "
            "ORDER BY id DESC LIMIT ? OFFSET ?");
        const QString like = "%" + QString::fromStdString(keyword) + "%";
        q.addBindValue(like);
        q.addBindValue(like);
        q.addBindValue(like);
        q.addBindValue(limit);
        q.addBindValue(offset);
    }

    if (!q.exec()) {
        err = q.lastError().text().toStdString();
        return false;
    }

    // ========= 4. 填充数据 =========
    out.clear();
    out.reserve(static_cast<size_t>(limit));

    while (q.next()) {
        ProjectInfoData p;
        p.id = q.value(0).toInt();
        p.project_id = q.value(1).toInt();
        p.project_name = q.value(2).toString().toStdString();
        p.sample_no = q.value(3).toString().toStdString();
        p.sample_source = q.value(4).toString().toStdString();
        p.sample_name = q.value(5).toString().toStdString();
        p.standard_curve = q.value(6).toString().toStdString();
        p.batch_code = q.value(7).toString().toStdString();
        p.detected_conc = q.value(8).toDouble();
        p.reference_value = q.value(9).toDouble();
        p.result = q.value(10).toString().toStdString();
        p.detected_time = q.value(11).toString().toStdString();
        p.detected_unit = q.value(12).toString().toStdString();
        p.detected_person = q.value(13).toString().toStdString();
        p.dilution_info = q.value(14).toString().toStdString();
        p.c_value = q.value(15).toDouble();
        p.t_value = q.value(16).toDouble();
        p.ratio_value = q.value(17).toDouble();

        out.push_back(std::move(p));
    }

    return true;
}

void register_routes(httplib::Server& svr, embedded::ServerConfig& cfg) {
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        send_json(res, 200,
                  "{\"code\":0,\"message\":\"成功\",\"data\":{\"status\":\"up\"}}");
    });

    svr.Get("/api/detect/detail", [&cfg](const httplib::Request& req,
                                         httplib::Response& res) {
        const auto sample_no = req.get_param_value("sampleNo");
        if (sample_no.empty()) {
            send_json_error(res, 400, 1004, "sampleNo不能为空");
            return;
        }

        ProjectInfoData p;
        std::string err;
        if (!query_project_detail(cfg.db_path, sample_no, p, err)) {
            send_json_error(res, 404, 1404, "样品不存在");
            return;
        }

        std::ostringstream oss;
        oss << "{\"code\":0,\"message\":\"成功\",\"data\":";
        append_project_json(oss, p);
        oss << '}';
        send_json(res, 200, oss.str());
    });
    svr.Get("/api/project/list", [&cfg](const httplib::Request& req, httplib::Response& res) {
        int limit = parse_int_safe(get_param(req, "limit", "20"), 20);
        if (limit < 1) {
            limit = 1;
        } else if (limit > 100) {
            limit = 100;
        }
        int page = parse_int_safe(get_param(req, "page", "1"), 1);
        if (page < 1) {
            page = 1;
        }
        const int offset = (page - 1) * limit;

        const std::string keyword = get_param(req, "keyword", "");
        std::vector<ProjectInfoData> items;
        long long total = 0;
        std::string db_err;
        if (!query_project_list(cfg.db_path, keyword, limit, offset, total, items, db_err)) {
            send_json_error(res, 500, 1500, "项目列表查询失败");
            return;
        }

        std::ostringstream oss;
        oss << "{\"code\":0,\"message\":\"成功\",\"data\":{\"page\":" << page
            << ",\"pageSize\":" << limit << ",\"total\":" << total << ",\"items\":[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            append_project_json(oss, items[i]);
        }
        oss << "]}}";
        send_json(res, 200, oss.str());
    });
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/projects.html");
    });
    svr.Get("/projects", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/projects.html");
    });
    svr.Get("/detect", [](const httplib::Request& req, httplib::Response& res) {
        std::string redirect_to = "/detect.html";
        const auto qpos = req.target.find('?');
        if (qpos != std::string::npos && qpos + 1 < req.target.size()) {
            redirect_to += req.target.substr(qpos);
        }
        res.set_redirect(redirect_to);
    });

    svr.Get("/api/detect/curve", [&cfg](const httplib::Request& req, httplib::Response& res) {
        const std::string sample_no = get_param(req, "sampleNo", "");
        if (sample_no.empty()) {
            send_json_error(res, 400, 1004, "sampleNo不能为空");
            return;
        }

        QList<double> adc_values;
        std::string db_err;
        QString qerr;
        QSqlDatabase db = openWebDatabase(cfg.db_path, qerr);
        if (!db.isOpen()) {
            send_json_error(res, 404, 1404, "数据库打开失败");
            return;
        }

        QSqlQuery q(db);
        q.prepare(
            "SELECT  adcValues FROM adc_data WHERE sampleNo = ? ORDER BY id ASC");
        q.addBindValue(QString::fromStdString(sample_no));
        if (!q.exec()) {
            send_json_error(res, 404, 1404, "获取曲线失败");
            return;
        }
        // if (!q.next()) {
        //     send_json_error(res, 404, 1404, "111111");
        //     return;
        // }
        while (q.next()) {
            QString json = q.value(0).toString();
            QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
            if (!doc.isArray())
                continue;

            QJsonArray arr = doc.array();
            for (auto v : arr) adc_values.append(v.toDouble());
        }
        // adc_values = q.value(0).value<QVector<double>>();
        if (adc_values.empty()) {
            send_json_error(res, 404, 1404, "曲线数据为空");
            return;
        }

        double min_v = 0.0;
        double max_v = 0.0;
        double sum = 0.0;
        for (size_t i = 0; i < adc_values.size(); ++i) {
            const double v = adc_values[i];
            if (i == 0 || v < min_v) {
                min_v = v;
            }
            if (i == 0 || v > max_v) {
                max_v = v;
            }
            sum += v;
        }

        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(3);
        oss << "{\"code\":0,\"message\":\"成功\",\"data\":{\"sampleNo\":\""
            << json_escape(sample_no) << "\",\"pointCount\":" << adc_values.size()
            << ",\"xAxisName\":\"数据点\",\"yAxisName\":\"电压值\""
            << ",\"stats\":{\"min\":" << min_v << ",\"max\":" << max_v
            << ",\"avg\":" << (sum / static_cast<double>(adc_values.size())) << "},\"adcValues\":[";
        for (size_t i = 0; i < adc_values.size(); ++i) {
            if (i > 0) {
                oss << ',';
            }
            oss << adc_values[i];
        }
        oss << "]}}";
        send_json(res, 200, oss.str());
    });
}

}  // namespace

// ======================== EmbeddedWebServer 实现 ========================

namespace embedded {

struct EmbeddedWebServer::Impl {
    explicit Impl(ServerConfig c) : cfg(std::move(c)) {}
    ServerConfig cfg;
    httplib::Server server;
    std::thread worker;
    bool started = false;
};

ServerConfig EmbeddedWebServer::defaultConfig() {
    ServerConfig cfg;
    cfg.host = "0.0.0.0";
    cfg.port = 8080;
    cfg.web_root = APP_DEFAULT_WEB_ROOT;
    cfg.browse_root = APP_DEFAULT_BROWSE_ROOT;
    cfg.db_path = APP_DB_PATH;
    return cfg;
}

EmbeddedWebServer::EmbeddedWebServer(ServerConfig cfg)
    : impl_(new Impl(std::move(cfg))) {
}

EmbeddedWebServer::~EmbeddedWebServer() {
    stop();
}

bool EmbeddedWebServer::start(std::string& err) {
    auto& cfg = impl_->cfg;

    qInfo() << "[Web] start"
            << "host =" << cfg.host.c_str()
            << "port =" << cfg.port
            << "web_root =" << cfg.web_root.c_str();

    // 1️⃣ 挂载静态目录
    if (!impl_->server.set_mount_point("/", cfg.web_root)) {
        err = "挂载静态目录失败";
        return false;
    }

    // 2️⃣ 注册路由
    qDebug() << "[Web] register routes";
    register_routes(impl_->server, cfg);
    qDebug() << "[Web] routes registered";

    // 3️⃣ 先 bind（关键）
    if (!impl_->server.bind_to_port(cfg.host.c_str(), cfg.port)) {
        err = "bind 端口失败，可能已被占用";
        qCritical() << "[Web] bind failed";
        return false;
    }

    // 4️⃣ listen 放线程
    impl_->worker = std::thread([this]() {
        qInfo() << "[Web] listening...";
        impl_->server.listen_after_bind();
        qWarning() << "[Web] listen thread exit";
    });

    // 5️⃣ 等待 server ready
    impl_->server.wait_until_ready();

    impl_->started = true;
    qInfo() << "[Web] started OK";
    return true;
}
void EmbeddedWebServer::stop() {
    if (!impl_)
        return;
    impl_->server.stop();
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
    impl_->started = false;
}

bool EmbeddedWebServer::isRunning() const {
    return impl_ && impl_->started;
}

}  // namespace embedded
