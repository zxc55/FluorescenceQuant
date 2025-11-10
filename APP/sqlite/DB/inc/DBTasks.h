#pragma once
#include <QString>
#include <QVariantMap>

#include "DTO.h"
enum class DBTaskType {
    EnsureAllSchemas,
    LoadSettings,
    UpdateSettings,
    AuthLogin,
    LoadUsers,
    AddUser,
    DeleteUser,
    ResetPassword,
    // —— 项目 —— //
    LoadProjects,
    DeleteProject,
    // —— 历史记录 —— //
    LoadHistory,
    InsertHistory
};

struct DBTask {
    DBTaskType type = DBTaskType::EnsureAllSchemas;

    // 通用参数位（整型用 p1；字符串用 s1..s4；设置结构体用 settings）
    int p1 = 0;
    QString s1, s2, s3, s4;
    AppSettingsRow settings;
    QVariantMap info;  // ✅ 新增字段：用于历史记录插入数据
    HistoryRow history;
    // 便捷构造
    static DBTask ensureAll() {
        DBTask t;
        t.type = DBTaskType::EnsureAllSchemas;
        return t;
    }
    static DBTask loadSettings() {
        DBTask t;
        t.type = DBTaskType::LoadSettings;
        return t;
    }
    static DBTask updateSettings(const AppSettingsRow& row) {
        DBTask t;
        t.type = DBTaskType::UpdateSettings;
        t.settings = row;
        return t;
    }

    static DBTask authLogin(const QString& u, const QString& p) {
        DBTask t;
        t.type = DBTaskType::AuthLogin;
        t.s1 = u;
        t.s2 = p;
        return t;
    }
    static DBTask loadUsers() {
        DBTask t;
        t.type = DBTaskType::LoadUsers;
        return t;
    }
    static DBTask addUser(const QString& u, const QString& p, const QString& role, const QString& note) {
        DBTask t;
        t.type = DBTaskType::AddUser;
        t.s1 = u;
        t.s2 = p;
        t.s3 = role;
        t.s4 = note;
        return t;
    }
    static DBTask deleteUser(const QString& u) {
        DBTask t;
        t.type = DBTaskType::DeleteUser;
        t.s1 = u;
        return t;
    }
    static DBTask resetPassword(const QString& u, const QString& p) {
        DBTask t;
        t.type = DBTaskType::ResetPassword;
        t.s1 = u;
        t.s2 = p;
        return t;
    }

    // —— 项目 —— //
    static DBTask loadProjects() {
        DBTask t;
        t.type = DBTaskType::LoadProjects;
        return t;
    }
    static DBTask deleteProject(int id) {
        DBTask t;
        t.type = DBTaskType::DeleteProject;
        t.p1 = id;
        return t;
    }
    // =====================
    // 历史记录任务
    // =====================
    static DBTask loadHistory() {
        DBTask t;
        t.type = DBTaskType::LoadHistory;
        return t;
    }

    static DBTask insertHistory(const HistoryRow& row) {
        DBTask t;
        t.type = DBTaskType::InsertHistory;
        t.history = row;  // ✅ 直接存结构体
        return t;
    }
};
