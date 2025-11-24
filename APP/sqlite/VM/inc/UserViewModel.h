#ifndef USERVIEWMODEL_H_
#define USERVIEWMODEL_H_

#include <QAbstractListModel>
#include <QObject>
#include <QVector>

#include "DBWorker.h"
#include "DTO.h"

// ———————————————————————————————
// 用户列表模型（给管理员查看用户列表）
// ———————————————————————————————
class UsersListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        UsernameRole = Qt::UserRole + 1,
        RoleNameRole,
        NoteRole
    };
    Q_ENUM(Roles)

    explicit UsersListModel(QObject* p = nullptr) : QAbstractListModel(p) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : items_.size();
    }

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
        return {
            {UsernameRole, "username"},
            {RoleNameRole, "roleName"},
            {NoteRole, "note"}};
    }

    void setItems(const QVector<UserRow>& v) {
        beginResetModel();
        items_ = v;
        endResetModel();
    }

private:
    QVector<UserRow> items_;
};

// ———————————————————————————————
// 用户 ViewModel：登录、登出、权限、读取用户列表
// ———————————————————————————————
class UserViewModel : public QObject {
    Q_OBJECT

    // 登录状态
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)

    // 当前用户名
    Q_PROPERTY(QString username READ username NOTIFY loggedInChanged)

    // 当前角色（admin / engineer / operator）
    Q_PROPERTY(QString roleName READ roleName NOTIFY loggedInChanged)

    // 登录错误消息
    Q_PROPERTY(QString loginMsg READ loginMsg NOTIFY loginMsgChanged)

    // 用户列表模型（只读、仅管理员使用）
    Q_PROPERTY(UsersListModel* users READ users CONSTANT)

public:
    explicit UserViewModel(QObject* p = nullptr);
    void bindWorker(DBWorker* w);

    // ——— Getter ———
    bool loggedIn() const { return loggedIn_; }
    QString username() const { return username_; }
    QString roleName() const { return role_; }
    QString loginMsg() const { return loginMsg_; }
    UsersListModel* users() { return &users_; }

    // ——— QML 可调用方法 ———
    Q_INVOKABLE void login(const QString& u, const QString& p);  // 登录
    Q_INVOKABLE void logout();                                   // 登出
    Q_INVOKABLE void loadUsers();                                // 加载用户列表（仅管理员）

signals:
    void loggedInChanged();
    void loginMsgChanged();

private:
    // 数据库后台线程回调
    void onAuthResult(bool ok, const QString& u, const QString& role);
    void onUsersLoaded(const QVector<UserRow>& rows);

private:
    DBWorker* worker_ = nullptr;

    bool loggedIn_ = false;
    QString username_;
    QString role_;  // admin / engineer / operator
    QString loginMsg_;

    UsersListModel users_;
};

#endif  // USERVIEWMODEL_H_
