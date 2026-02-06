#include "OtaManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <climits>
#include <thread>

#include "sha256.h"
#include "unzip.h"
// ================== OTA 调试开关 ==================
#define OTA_DEBUG 1

#if OTA_DEBUG
#define OTA_LOG(msg) qDebug() << "[OTA]" << msg
#else
#define OTA_LOG(msg) \
    do {             \
    } while (0)
#endif

// ================== ThingsBoard 配置 ==================
static const QString TB_HOST = "http://iot.pribolab.net:9090";
static const QString TB_TOKEN = "7a34q5OzQ75b2jLh5qae";

// ================== OTA 路径（全部在 SD 卡） ==================
static const QString OTA_DIR = "/mnt/SDCARD/update";
static const QString OTA_STATE_DIR = OTA_DIR + "/state";
static const QString OTA_STATE_FILE = OTA_STATE_DIR + "/ota_state.json";

static const QString OTA_PKG = OTA_DIR + "/update.zip";
static const QString OTA_WORK = OTA_DIR + "/work";
static const QString OTA_SCRIPTS_DIR = OTA_DIR + "/scripts";

// =====================================================

OtaManager::OtaManager(QObject* parent)
    : QObject(parent) {
    OTA_LOG("OtaManager constructed");
    loadLocalVersion();
    otaThread_ = std::thread(&OtaManager::otaThreadLoop, this);
}
OtaManager::~OtaManager() {
    otaExit_ = true;
    otaCv_.notify_one();
    if (otaThread_.joinable())
        otaThread_.join();
}
// ================== 执行 shell 命令 ==================
QString OtaManager::exec(const QString& cmd) {
    OTA_LOG("exec:" << cmd);

    QProcess p;
    p.start("/bin/sh", {"-c", cmd});
    p.waitForFinished(-1);

    QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    QString err = QString::fromLocal8Bit(p.readAllStandardError());

    if (!out.isEmpty())
        OTA_LOG("stdout:" << out.trimmed());

    if (!err.isEmpty())
        OTA_LOG("stderr:" << err.trimmed());

    return out;
}

// ================== 读取本地版本 ==================
bool OtaManager::loadLocalVersion() {
    OTA_LOG("loadLocalVersion");

    // QDir().mkpath(OTA_STATE_DIR);

    // QFile f(OTA_STATE_FILE);
    // if (!f.open(QIODevice::ReadOnly)) {
    //     OTA_LOG("state file not exist, init default version");

    //     curTitle = "FLA_T113";
    //     curVersion = "1.0.0";
    //     return saveLocalVersion();
    // }

    // QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    // f.close();

    // if (!doc.isObject()) {
    //     OTA_LOG("state file json invalid");
    //     return false;
    // }

    // QJsonObject o = doc.object();
    // curTitle = o.value("title").toString();
    // curVersion = o.value("version").toString();
    curTitle = APP_TITLE;
    curVersion = APP_VERSION;
    OTA_LOG("local version:" << curTitle << curVersion);
    return true;
}

// ================== 保存本地版本 ==================
bool OtaManager::saveLocalVersion() {
    OTA_LOG("saveLocalVersion:" << curTitle << curVersion);

    QDir().mkpath(OTA_STATE_DIR);

    QFile f(OTA_STATE_FILE);
    if (!f.open(QIODevice::WriteOnly)) {
        OTA_LOG("open state file failed");
        return false;
    }

    QJsonObject o;
    o["title"] = curTitle;
    o["version"] = curVersion;

    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    f.close();
    return true;
}

// ================== UI 入口：检查更新 ==================
void OtaManager::checkUpdate() {
    OTA_LOG("checkUpdate begin");

    emit info("正在检查更新…");

    // 1. SD 卡检查
    if (!QDir(OTA_DIR).exists()) {
        OTA_LOG("SD card not found:" << OTA_DIR);
        emit error("未检测到 SD 卡，无法升级");
        emit finished(false);
        return;
    }

    // 2. 查询固件属性（fw_*）
    if (!queryAttributes()) {
        OTA_LOG("no update needed");
        emit info("当前已是最新版本");
        // emit finished(true);
        emit NoUpdate();
        return;
    }

    OTA_LOG("remote version:" << fwTitle << fwVersion);
    OTA_LOG("local  version:" << curTitle << curVersion);

    emit info(QString("发现新版本 %1").arg(fwVersion));
    emit updateAvailable(fwVersion);
    // // 3. 上报 DOWNLOADING
    // exec(QString(
    //          "curl -s -X POST %1/api/v1/%2/telemetry "
    //          "-H \"Content-Type: application/json\" "
    //          "-d '{\"fw_state\":\"DOWNLOADING\"}'")
    //          .arg(TB_HOST, TB_TOKEN));

    // // 4. 下载固件包
    // if (!downloadPackage()) {
    //     OTA_LOG("download failed");

    //     exec(QString(
    //              "curl -s -X POST %1/api/v1/%2/telemetry "
    //              "-H \"Content-Type: application/json\" "
    //              "-d '{\"fw_state\":\"FAILED\",\"fw_error\":\"download failed\"}'")
    //              .arg(TB_HOST, TB_TOKEN));

    //     emit error("下载失败");
    //     emit finished(false);
    //     return;
    // }

    // // 5. 上报 DOWNLOADED
    // exec(QString(
    //          "curl -s -X POST %1/api/v1/%2/telemetry "
    //          "-H \"Content-Type: application/json\" "
    //          "-d '{\"fw_state\":\"DOWNLOADED\"}'")
    //          .arg(TB_HOST, TB_TOKEN));

    // // 6. 校验固件包
    // if (!verifyPackage()) {
    //     OTA_LOG("verify failed");

    //     exec(QString(
    //              "curl -s -X POST %1/api/v1/%2/telemetry "
    //              "-H \"Content-Type: application/json\" "
    //              "-d '{\"fw_state\":\"FAILED\",\"fw_error\":\"verify failed\"}'")
    //              .arg(TB_HOST, TB_TOKEN));

    //     emit error("升级包校验失败");
    //     emit finished(false);
    //   return;
    // }

    // // 7. VERIFIED
    // exec(QString(
    //          "curl -s -X POST %1/api/v1/%2/telemetry "
    //          "-H \"Content-Type: application/json\" "
    //          "-d '{\"fw_state\":\"VERIFIED\"}'")
    //          .arg(TB_HOST, TB_TOKEN));

    // emit info("开始升级，请勿断电");

    // // 8. UPDATING
    // exec(QString(
    //          "curl -s -X POST %1/api/v1/%2/telemetry "
    //          "-H \"Content-Type: application/json\" "
    //          "-d '{\"fw_state\":\"UPDATING\"}'")
    //          .arg(TB_HOST, TB_TOKEN));

    // // 9. 执行升级脚本
    // if (!applyUpdate()) {
    //     OTA_LOG("applyUpdate failed");

    //     exec(QString(
    //              "curl -s -X POST %1/api/v1/%2/telemetry "
    //              "-H \"Content-Type: application/json\" "
    //              "-d '{\"fw_state\":\"FAILED\",\"fw_error\":\"apply failed\"}'")
    //              .arg(TB_HOST, TB_TOKEN));

    //     emit error("升级脚本执行失败");
    //     emit finished(false);
    //     return;
    // }

    // // 10. 升级成功，保存本地版本
    // curTitle = fwTitle;
    // curVersion = fwVersion;
    // saveLocalVersion();

    // // 11. UPDATED
    // exec(QString(
    //          "curl -s -X POST %1/api/v1/%2/telemetry "
    //          "-H \"Content-Type: application/json\" "
    //          "-d '{"
    //          "\"fw_state\":\"UPDATED\","
    //          "\"current_fw_title\":\"%3\","
    //          "\"current_fw_version\":\"%4\""
    //          "}'")
    //          .arg(TB_HOST, TB_TOKEN, curTitle, curVersion));

    // OTA_LOG("update success");

    // emit info("升级完成，重启生效");
    // emit finished(true);
    // emit versionChanged();
}
void OtaManager::startUpdate() {
    emit info("升级中请勿断电");
    QCoreApplication::processEvents();
    // 3. 上报 DOWNLOADING
    exec(QString(
             "curl -s -X POST %1/api/v1/%2/telemetry "
             "-H \"Content-Type: application/json\" "
             "-d '{\"fw_state\":\"DOWNLOADING\"}'")
             .arg(TB_HOST, TB_TOKEN));

    // 4. 下载固件包
    if (!downloadPackage()) {
        OTA_LOG("download failed");

        exec(QString(
                 "curl -s -X POST %1/api/v1/%2/telemetry "
                 "-H \"Content-Type: application/json\" "
                 "-d '{\"fw_state\":\"FAILED\",\"fw_error\":\"download failed\"}'")
                 .arg(TB_HOST, TB_TOKEN));

        emit error("下载失败");
        emit finished(false);
        return;
    }

    // 5. 上报 DOWNLOADED
    exec(QString(
             "curl -s -X POST %1/api/v1/%2/telemetry "
             "-H \"Content-Type: application/json\" "
             "-d '{\"fw_state\":\"DOWNLOADED\"}'")
             .arg(TB_HOST, TB_TOKEN));

    // 6. 校验固件包
    if (!verifyPackage()) {
        OTA_LOG("verify failed");

        exec(QString(
                 "curl -s -X POST %1/api/v1/%2/telemetry "
                 "-H \"Content-Type: application/json\" "
                 "-d '{\"fw_state\":\"FAILED\",\"fw_error\":\"verify failed\"}'")
                 .arg(TB_HOST, TB_TOKEN));

        emit error("升级包校验失败");
        emit finished(false);
        return;
    }

    // 7. VERIFIED
    exec(QString(
             "curl -s -X POST %1/api/v1/%2/telemetry "
             "-H \"Content-Type: application/json\" "
             "-d '{\"fw_state\":\"VERIFIED\"}'")
             .arg(TB_HOST, TB_TOKEN));

    emit info("开始升级，请勿断电");

    // 8. UPDATING
    exec(QString(
             "curl -s -X POST %1/api/v1/%2/telemetry "
             "-H \"Content-Type: application/json\" "
             "-d '{\"fw_state\":\"UPDATING\"}'")
             .arg(TB_HOST, TB_TOKEN));

    // 9. 执行升级脚本
    if (!applyUpdate()) {
        OTA_LOG("applyUpdate failed");

        exec(QString(
                 "curl -s -X POST %1/api/v1/%2/telemetry "
                 "-H \"Content-Type: application/json\" "
                 "-d '{\"fw_state\":\"FAILED\",\"fw_error\":\"apply failed\"}'")
                 .arg(TB_HOST, TB_TOKEN));

        emit error("升级脚本执行失败");
        emit finished(false);
        return;
    }
    qDebug("startUpdate --------------------------------success");
    // 10. 升级成功，保存本地版本
    curTitle = fwTitle;
    curVersion = fwVersion;
    saveLocalVersion();
    qDebug("saveLocalVersion success");
    // 11. UPDATED
    exec(QString(
             "curl -s -X POST %1/api/v1/%2/telemetry "
             "-H \"Content-Type: application/json\" "
             "-d '{"
             "\"fw_state\":\"UPDATED\","
             "\"current_fw_title\":\"%3\","
             "\"current_fw_version\":\"%4\""
             "}'")
             .arg(TB_HOST, TB_TOKEN, curTitle, curVersion));

    OTA_LOG("update success");

    emit info("升级完成，重启生效");
    emit finished(true);
    emit versionChanged();
}

// ================== 查询 TB 共享属性 ==================
bool OtaManager::queryAttributes() {
    OTA_LOG("queryAttributes");

    QString cmd = QString(
                      "curl -s \"%1/api/v1/%2/attributes?"
                      "sharedKeys=fw_title,fw_version,fw_checksum,fw_checksum_algorithm\"")
                      .arg(TB_HOST, TB_TOKEN);

    QString json = exec(cmd);
    OTA_LOG("stdout:" << json);

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject())
        return false;

    QJsonObject shared = doc.object().value("shared").toObject();
    if (!shared.contains("fw_version")) {
        OTA_LOG("fw_version not found");
        return false;
    }

    fwTitle = shared.value("fw_title").toString();
    fwVersion = shared.value("fw_version").toString();
    fwChecksum = shared.value("fw_checksum").toString();
    fwAlgo = shared.value("fw_checksum_algorithm").toString();

    OTA_LOG("remote version:" << fwTitle << fwVersion);

    // 本地版本一致，不升级
    if (fwTitle == curTitle && fwVersion == curVersion)
        return false;

    return true;
}

// ================== 下载 OTA 包 ==================
bool OtaManager::downloadPackage() {
    OTA_LOG("downloadPackage");

    QDir().mkpath(OTA_DIR);

    QString cmd = QString(
                      "curl -L "
                      "-w \"\\nhttp_code=%%{http_code} size=%%{size_download}\\n\" "
                      "\"%1/api/v1/%2/firmware?title=%3&version=%4\" "
                      "-o \"%5\"")
                      .arg(TB_HOST, TB_TOKEN, fwTitle, fwVersion, OTA_PKG);

    QString out = exec(cmd);
    OTA_LOG("curl out:" << out);

    QFile f(OTA_PKG);
    OTA_LOG("pkg exists:" << f.exists() << "size:" << f.size());

    return f.exists() && f.size() > 0;
}

// ================== 校验 OTA 包 ==================
bool OtaManager::verifyPackage() {
    OTA_LOG("verifyPackage algo:" << fwAlgo);

    if (fwAlgo != "SHA256") {
        OTA_LOG("unsupported checksum algo:" << fwAlgo);
        return false;
    }
    SHA256 sha;
    // const std::string filePath = OTA_PKG.toStdString();

    std::string localHash = sha.calculateSHA256FromFile(
        OTA_PKG.toStdString());

    if (localHash.empty()) {
        OTA_LOG("SHA256 calculate failed");
        return false;
    }

    OTA_LOG("local  sha256:" << QString::fromStdString(localHash));
    OTA_LOG("remote sha256:" << fwChecksum);

    if (QString::fromStdString(localHash).compare(
            fwChecksum, Qt::CaseInsensitive) != 0) {
        OTA_LOG("SHA256 mismatch");
        return false;
    }

    OTA_LOG("SHA256 verify OK");
    return true;
}

// ================== 执行升级脚本（SD 卡） ==================

bool OtaManager::applyUpdate() {
    OTA_LOG("applyUpdate begin");

    // 1. 清理并创建工作目录
    exec("rm -rf " + OTA_WORK);
    exec("mkdir -p " + OTA_WORK);

    OTA_LOG("extract zip:" << OTA_PKG << "to" << OTA_WORK);

    // 2. 解压 ZIP（你已有的 minizip 解压）
    if (!Unzip::extractFile(OTA_PKG.toStdString(),
                            OTA_WORK.toStdString())) {
        OTA_LOG("zip extract failed");
        return false;
    }

    // 3. 兼容 ZIP 多一层目录的 payload 路径
    QString payloadDir = OTA_WORK + "/payload";
    QString payloadDirAlt = OTA_WORK + "/update/payload";

    if (QDir(payloadDir).exists()) {
        OTA_LOG("payload dir:" << payloadDir);
    } else if (QDir(payloadDirAlt).exists()) {
        OTA_LOG("payload dir (alt):" << payloadDirAlt);

        // 把 update/payload 挪到 work/payload
        exec(QString("mv %1 %2")
                 .arg(payloadDirAlt)
                 .arg(payloadDir));
    } else {
        OTA_LOG("payload not found");
        return false;
    }

    // 4. 同样兼容 scripts 目录（可选）
    QString scriptsDir = OTA_WORK + "/scripts";
    QString scriptsDirAlt = OTA_WORK + "/update/scripts";

    if (!QDir(scriptsDir).exists() && QDir(scriptsDirAlt).exists()) {
        OTA_LOG("scripts dir (alt):" << scriptsDirAlt);
        exec(QString("mv %1 %2")
                 .arg(scriptsDirAlt)
                 .arg(scriptsDir));
    }

    // 5. 优先使用 ZIP 中自带的升级脚本
    QString scriptPath = OTA_SCRIPTS_DIR + "/apply_update.sh";
    QString newScript = scriptsDir + "/apply_update.sh";

    if (QFile::exists(newScript)) {
        OTA_LOG("found new apply_update.sh in package");

        QDir().mkpath(OTA_SCRIPTS_DIR);

        exec(QString("cp %1 %2/apply_update.new")
                 .arg(newScript)
                 .arg(OTA_SCRIPTS_DIR));

        exec(QString("chmod +x %1/apply_update.new")
                 .arg(OTA_SCRIPTS_DIR));

        // shell 语法检查
        if (system(QString("sh -n %1/apply_update.new")
                       .arg(OTA_SCRIPTS_DIR)
                       .toLocal8Bit()
                       .constData()) != 0) {
            OTA_LOG("apply_update.sh syntax error");
            return false;
        }

        exec(QString("mv %1/apply_update.new %1/apply_update.sh")
                 .arg(OTA_SCRIPTS_DIR));
    } else {
        OTA_LOG("no script in package, use existing script");
    }

    // 6. 最终执行升级脚本
    if (!QFile::exists(scriptPath)) {
        OTA_LOG("apply_update.sh not found:" << scriptPath);
        return false;
    }

    QString cmd = QString("%1 %2")
                      .arg(scriptPath)
                      .arg(payloadDir);

    OTA_LOG("run script:" << cmd);

    int ret = system(cmd.toLocal8Bit().constData());
    OTA_LOG("script return:" << ret);

    return ret == 0;
}
void OtaManager::startUpdateAsync() {
    OTA_LOG("startUpdateAsync");

    // 把 startUpdate 投递回 OtaManager 所在线程（GUI）
    QMetaObject::invokeMethod(
        this,
        "startUpdate",
        Qt::QueuedConnection);
}
void OtaManager::requestStartUpdate() {
    OTA_LOG("requestStartUpdate");

    {
        std::lock_guard<std::mutex> lock(otaMutex_);
        otaTaskPending_ = true;
    }
    otaCv_.notify_one();
}
void OtaManager::otaThreadLoop() {
    OTA_LOG("OTA worker thread started");

    while (!otaExit_) {
        std::unique_lock<std::mutex> lock(otaMutex_);

        // 没任务就睡
        otaCv_.wait(lock, [&]() {
            return otaTaskPending_ || otaExit_;
        });

        if (otaExit_)
            break;

        otaTaskPending_ = false;
        lock.unlock();

        OTA_LOG("OTA worker wakeup, startUpdate");

        // ⚠️ 这里是真正跑升级的地方
        startUpdate();
    }

    OTA_LOG("OTA worker thread exit");
}
