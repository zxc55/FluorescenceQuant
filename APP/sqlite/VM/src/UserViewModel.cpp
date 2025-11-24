#include "UserViewModel.h"

#include <QDebug>

UserViewModel::UserViewModel(QObject* p) : QObject(p) {}

void UserViewModel::bindWorker(DBWorker* w) {
    worker_ = w;

    // 登录回调
    QObject::connect(worker_, &DBWorker::authResult,
                     this, &UserViewModel::onAuthResult);

    // 管理员加载用户列表
    QObject::connect(worker_, &DBWorker::usersLoaded,
                     this, &UserViewModel::onUsersLoaded);
}

// ================================
// 登录（异步）
// ================================
void UserViewModel::login(const QString& u, const QString& p) {
    loginMsg_.clear();
    emit loginMsgChanged();

    if (!worker_)
        return;

    worker_->postAuthLogin(u, p);
}

// ================================
// 登出
// ================================
void UserViewModel::logout() {
    loggedIn_ = false;
    username_.clear();
    role_.clear();
    emit loggedInChanged();
}

// ================================
// 加载用户列表（仅管理员）
// ================================
void UserViewModel::loadUsers() {
    if (!worker_)
        return;
    if (role_ != "admin")
        return;

    worker_->postLoadUsers();
}

void UserViewModel::onAuthResult(bool ok, const QString& u, const QString& roleStr) {
    if (!ok) {
        // 登录失败 ——— 必须通知 QML！
        loggedIn_ = false;
        username_.clear();
        role_.clear();

        loginMsg_ = "用户名或密码错误";

        emit loginMsgChanged();
        emit loggedInChanged();  // ← 非常重要！！！

        return;
    }

    // 登录成功
    loggedIn_ = true;
    username_ = u;
    role_ = roleStr.toLower();  // ← 统一角色大小写：admin / engineer / operator

    emit loggedInChanged();  // 通知 QML 登录成功

    // 管理员加载用户列表
    // if (role_ == "admin" && worker_)
    //     worker_->postLoadUsers();
}

// ================================
// 加载用户列表回调
// ================================
void UserViewModel::onUsersLoaded(const QVector<UserRow>& rows) {
    users_.setItems(rows);
}
