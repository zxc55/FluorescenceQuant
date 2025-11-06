#ifndef USERVIEWMODEL_H_
#define USERVIEWMODEL_H_
#include <QAbstractListModel>
#include <QObject>
#include <QVector>

#include "DBWorker.h"
#include "DTO.h"

// —— 用户列表模型（给 QML ListView 用）——
class UsersListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { UsernameRole = Qt::UserRole + 1,
                 RoleNameRole,
                 NoteRole };
    Q_ENUM(Roles)
    explicit UsersListModel(QObject* p = nullptr) : QAbstractListModel(p) {}
    int rowCount(const QModelIndex& parent = QModelIndex()) const override { return parent.isValid() ? 0 : items_.size(); }
    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid())
            return {};
        const auto& r = items_[idx.row()];
        switch (role) {
        case UsernameRole:
            return r.username;
        case RoleNameRole:
            return r.roleName;
        case NoteRole:
            return r.note;
        }
        return {};
    }
    QHash<int, QByteArray> roleNames() const override {
        return {{UsernameRole, "username"}, {RoleNameRole, "roleName"}, {NoteRole, "note"}};
    }
    void setItems(const QVector<UserRow>& v) {
        beginResetModel();
        items_ = v;
        endResetModel();
    }

private:
    QVector<UserRow> items_;
};

// —— 用户 VM：登录、登出、权限、用户管理 ——
class UserViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)     // 是否已登录
    Q_PROPERTY(QString username READ username NOTIFY loggedInChanged)  // 当前用户名
    Q_PROPERTY(QString roleName READ roleName NOTIFY loggedInChanged)  // 当前角色
    Q_PROPERTY(QString loginMsg READ loginMsg NOTIFY loginMsgChanged)  // 登录错误消息
    Q_PROPERTY(UsersListModel* users READ users CONSTANT)              // 用户列表模型（仅管理员可用）

public:
    explicit UserViewModel(QObject* p = nullptr);
    void bindWorker(DBWorker* w);

    // —— Getter ——
    bool loggedIn() const { return loggedIn_; }
    QString username() const { return username_; }
    QString roleName() const { return role_; }
    QString loginMsg() const { return loginMsg_; }
    UsersListModel* users() { return &users_; }

    // —— 登录 / 登出 ——
    Q_INVOKABLE void login(const QString& u, const QString& p);  // 登录
    Q_INVOKABLE void logout();                                   // 登出

    // —— 用户管理（管理员才应在 UI 暴露）——
    Q_INVOKABLE void loadUsers();  // 刷新列表
    Q_INVOKABLE void addUser(const QString& u, const QString& p, const QString& role, const QString& note);
    Q_INVOKABLE void deleteUser(const QString& u);
    Q_INVOKABLE void resetPassword(const QString& u, const QString& newPass = "123456");

signals:
    void loggedInChanged();
    void loginMsgChanged();

private:
    void onAuthResult(bool ok, const QString& u, const QString& role);
    void onUsersLoaded(const QVector<UserRow>& rows);
    void onUserAdded(bool ok, const QString& u);
    void onUserDeleted(bool ok, const QString& u);
    void onPasswordReset(bool ok, const QString& u);

private:
    DBWorker* worker_ = nullptr;
    bool loggedIn_ = false;
    QString username_;
    QString role_;
    QString loginMsg_;
    UsersListModel users_;
};
#endif  // USERVIEWMODEL_H_