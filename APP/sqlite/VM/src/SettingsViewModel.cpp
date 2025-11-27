#include "SettingsViewModel.h"

#include <QDebug>

SettingsViewModel::SettingsViewModel(QObject* parent)
    : QObject(parent) {
    // nothing
}

// ✅ 新增：bindWorker 实现
void SettingsViewModel::bindWorker(DBWorker* w) {
    m_worker = w;
    if (!m_worker) {
        qWarning() << "[SettingsVM] bindWorker(nullptr)";
        return;
    }

    // 让 DBWorker 加载到的设置能回填到 VM / QML
    QObject::connect(m_worker, &DBWorker::settingsLoaded,
                     this, &SettingsViewModel::onSettingsLoaded,
                     Qt::QueuedConnection);
}

void SettingsViewModel::onSettingsLoaded(const AppSettingsRow& row) {
    m_data = row;
    emit changed();
}

// ============ setters ============
void SettingsViewModel::setManufacturer(const QString& v) {
    if (m_data.manufacturer == v)
        return;
    m_data.manufacturer = v;
    emit changed();
}

void SettingsViewModel::setAutoPrint(bool v) {
    if (m_data.autoPrint == v)
        return;
    m_data.autoPrint = v;
    emit changed();
}

void SettingsViewModel::setAutoUpload(bool v) {
    if (m_data.autoUpload == v)
        return;
    m_data.autoUpload = v;
    emit changed();
}

void SettingsViewModel::setAutoIdGen(bool v) {
    if (m_data.autoIdGen == v)
        return;
    m_data.autoIdGen = v;
    emit changed();
}

void SettingsViewModel::setMicroSwitch(bool v) {
    if (m_data.microSwitch == v)
        return;
    m_data.microSwitch = v;
    emit changed();
}

void SettingsViewModel::setManufacturerPrint(bool v) {
    if (m_data.manufacturerPrint == v)
        return;
    m_data.manufacturerPrint = v;
    emit changed();
}

void SettingsViewModel::setBacklight(int v) {
    if (m_data.backlight == v)
        return;
    m_data.backlight = v;
    emit changed();
}

void SettingsViewModel::setLang(const QString& v) {
    if (m_data.lang == v)
        return;
    m_data.lang = v;
    emit changed();
}

// ✅ 如果你有 setEngineerMode 的实现，就保持这样；如果不用，干脆删掉
void SettingsViewModel::setEngineerMode(bool v) {
    if (m_data.engineerMode == v)
        return;
    m_data.engineerMode = v;
    emit changed();
}

void SettingsViewModel::save() {
    if (!m_worker) {
        qWarning() << "[SettingsVM] no worker!";
        return;
    }
    m_worker->postUpdateSettings(m_data);
}
