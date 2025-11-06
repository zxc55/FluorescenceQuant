#include "UserViewModel.h"

#include <QDebug>

UserViewModel::UserViewModel(QObject* p) : QObject(p) {}

void UserViewModel::bindWorker(DBWorker* w) {
    worker_ = w;
    // 连接 DB 回调
    QObject::connect(worker_, &DBWorker::authResult, this, &UserViewModel::onAuthResult);
    QObject::connect(worker_, &DBWorker::usersLoaded, this, &UserViewModel::onUsersLoaded);
    QObject::connect(worker_, &DBWorker::userAdded, this, &UserViewModel::onUserAdded);
    QObject::connect(worker_, &DBWorker::userDeleted, this, &UserViewModel::onUserDeleted);
    QObject::connect(worker_, &DBWorker::passwordReset, this, &UserViewModel::onPasswordReset);
}

void UserViewModel::login(const QString& u, const QString& p) {
    loginMsg_.clear();
    emit loginMsgChanged();
    if (!worker_)
        return;
    worker_->postAuthLogin(u, p);
}
void UserViewModel::logout() {
    loggedIn_ = false;
    username_.clear();
    role_.clear();
    emit loggedInChanged();
}

void UserViewModel::loadUsers() {
    if (!worker_)
        return;
    if (role_ != "Admin")
        return;  // UI 层简单限制；真正安全应服务端检查（内置）
    worker_->postLoadUsers();
}
void UserViewModel::addUser(const QString& u, const QString& p, const QString& role, const QString& note) {
    if (!worker_)
        return;
    if (role_ != "Admin")
        return;
    worker_->postAddUser(u, p, role, note);
}
void UserViewModel::deleteUser(const QString& u) {
    if (!worker_)
        return;
    if (role_ != "Admin")
        return;
    worker_->postDeleteUser(u);
}
void UserViewModel::resetPassword(const QString& u, const QString& newPass) {
    if (!worker_)
        return;
    if (role_ != "Admin")
        return;
    worker_->postResetPassword(u, newPass);
}

void UserViewModel::onAuthResult(bool ok, const QString& u, const QString& role) {
    if (!ok) {
        loginMsg_ = "用户名或密码错误";
        emit loginMsgChanged();
        return;
    }
    loggedIn_ = true;
    username_ = u;
    role_ = role;
    emit loggedInChanged();
    // 登录后，若是管理员，自动加载一次用户列表
    if (role_ == "Admin" && worker_)
        worker_->postLoadUsers();
}
void UserViewModel::onUsersLoaded(const QVector<UserRow>& rows) {
    users_.setItems(rows);
}
void UserViewModel::onUserAdded(bool ok, const QString& u) {
    if (!ok)
        qWarning() << "[UserVM] add fail" << u;
}
void UserViewModel::onUserDeleted(bool ok, const QString& u) {
    if (!ok)
        qWarning() << "[UserVM] del fail" << u;
}
void UserViewModel::onPasswordReset(bool ok, const QString& u) {
    if (!ok)
        qWarning() << "[UserVM] reset fail" << u;
}
