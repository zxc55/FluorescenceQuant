#pragma once
#include <QObject>

#include "DBWorker.h"
#include "DTO.h"

class SettingsViewModel : public QObject {
    Q_OBJECT
public:
    explicit SettingsViewModel(QObject* parent = nullptr);

    // 允许延迟绑定 DBWorker（实现放到 cpp）
    void bindWorker(DBWorker* worker);

    // --- QML 可读属性 ---
    Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer NOTIFY changed)
    Q_PROPERTY(bool autoPrint READ autoPrint WRITE setAutoPrint NOTIFY changed)
    Q_PROPERTY(bool autoUpload READ autoUpload WRITE setAutoUpload NOTIFY changed)
    Q_PROPERTY(bool autoIdGen READ autoIdGen WRITE setAutoIdGen NOTIFY changed)
    Q_PROPERTY(bool microSwitch READ microSwitch WRITE setMicroSwitch NOTIFY changed)
    Q_PROPERTY(bool manufacturerPrint READ manufacturerPrint WRITE setManufacturerPrint NOTIFY changed)
    Q_PROPERTY(int backlight READ backlight WRITE setBacklight NOTIFY changed)
    Q_PROPERTY(QString lang READ lang WRITE setLang NOTIFY changed)
    Q_PROPERTY(bool printSampleSource READ printSampleSource WRITE setPrintSampleSource NOTIFY changed)
    Q_PROPERTY(bool printReferenceValue READ printReferenceValue WRITE setPrintReferenceValue NOTIFY changed)
    Q_PROPERTY(bool printDetectedPerson READ printDetectedPerson WRITE setPrintDetectedPerson NOTIFY changed)
    Q_PROPERTY(bool printDilutionInfo READ printDilutionInfo WRITE setPrintDilutionInfo NOTIFY changed)

    // 如果以后要在 QML 控制工程师模式，可以再加：
    // Q_PROPERTY(bool engineerMode READ engineerMode WRITE setEngineerMode NOTIFY changed)

    Q_INVOKABLE void save();

    // --- getters ---
    QString manufacturer() const { return m_data.manufacturer; }
    bool autoPrint() const { return m_data.autoPrint; }
    bool autoUpload() const { return m_data.autoUpload; }
    bool autoIdGen() const { return m_data.autoIdGen; }
    bool microSwitch() const { return m_data.microSwitch; }
    bool manufacturerPrint() const { return m_data.manufacturerPrint; }
    int backlight() const { return m_data.backlight; }
    QString lang() const { return m_data.lang; }
    bool printSampleSource() const { return m_data.printSampleSource; }
    bool printReferenceValue() const { return m_data.printReferenceValue; }
    bool printDetectedPerson() const { return m_data.printDetectedPerson; }
    bool printDilutionInfo() const { return m_data.printDilutionInfo; }

    // bool engineerMode() const { return m_data.engineerMode; }  // 如果需要

    // --- setters ---
    void setManufacturer(const QString& v);
    void setAutoPrint(bool v);
    void setAutoUpload(bool v);
    void setAutoIdGen(bool v);
    void setMicroSwitch(bool v);
    void setManufacturerPrint(bool v);
    void setBacklight(int v);
    void setLang(const QString& v);
    void setEngineerMode(bool v);  // ✅ 如果 cpp 有实现，就加这行
    void setPrintSampleSource(bool v);
    void setPrintReferenceValue(bool v);
    void setPrintDetectedPerson(bool v);
    void setPrintDilutionInfo(bool v);
signals:
    void changed();

public slots:
    void onSettingsLoaded(const AppSettingsRow& row);

private:
    AppSettingsRow m_data;
    DBWorker* m_worker = nullptr;
};
