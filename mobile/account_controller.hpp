#pragma once
#include <QObject>
#include <QJsonObject>
#include <QUrl>

// Device-local demonstration accounts. No claim of remote identity or email verification.
class AccountController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString email READ email NOTIFY changed)
    Q_PROPERTY(QString displayName READ displayName NOTIFY changed)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY changed)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY changed)
    Q_PROPERTY(QString noticeCode READ noticeCode NOTIFY changed)
    Q_PROPERTY(bool isLocal READ isLocal CONSTANT)
public:
    explicit AccountController(QObject* parent = nullptr);
    explicit AccountController(const QString& dataDirectory, QObject* parent = nullptr);
    bool loggedIn() const { return !profile_.isEmpty(); }
    bool busy() const { return busy_; }
    QString email() const { return profile_.value("email").toString(); }
    QString displayName() const { return profile_.value("nickname").toString(); }
    QUrl avatarUrl() const;
    QString errorCode() const { return error_; }
    QString noticeCode() const { return notice_; }
    bool isLocal() const { return true; }
    Q_INVOKABLE void registerAccount(const QString& email, const QString& password,
                                     const QString& confirm, const QString& nickname);
    Q_INVOKABLE void login(const QString& email, const QString& password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void updateProfile(const QString& nickname);
    Q_INVOKABLE void setAvatar(const QUrl& source);
    Q_INVOKABLE void removeAvatar();
    Q_INVOKABLE void clearFeedback();
    void setAppLocked(bool locked);
signals:
    void changed();
    void sessionChanged();
    void operationFinished(bool success);
private:
    QString directory_;
    QJsonObject profile_;
    bool busy_ = false;
    bool app_locked_ = false;
    quint64 epoch_ = 0;
    QString error_, notice_;
    bool ready(bool requireLogin = false);
    void fail(const QString& code);
    void authenticate(const QString& email, const QString& password, const QString& nickname, bool create);
    bool saveProfile(QJsonObject updated, const QString& notice);
};
