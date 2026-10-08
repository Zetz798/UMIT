#pragma once

#include <QColor>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <memory>

// Device-local preferences and an independent privacy lock. This does not encrypt
// account data and is deliberately separate from the UMIT and wallet sessions.
class AppSettings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY colorsChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY lockStateChanged)
    Q_PROPERTY(bool lockEnabled READ lockEnabled NOTIFY lockStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY errorChanged)
    Q_PROPERTY(int lockRemainingSeconds READ lockRemainingSeconds NOTIFY lockRemainingSecondsChanged)

public:
    explicit AppSettings(QObject* parent = nullptr);
    explicit AppSettings(const QString& storageFile, QObject* parent = nullptr);
    ~AppSettings() override;

    QString language() const { return language_; }
    bool darkMode() const { return dark_mode_; }
    QColor backgroundColor() const;
    QColor textColor() const;
    bool locked() const { return locked_; }
    bool lockEnabled() const { return lock_enabled_; }
    bool busy() const { return busy_; }
    QString errorCode() const { return error_code_; }
    int lockRemainingSeconds() const;

    void setLanguage(const QString& language);
    void setDarkMode(bool enabled);
    void setBackgroundColor(const QColor& color);
    void setTextColor(const QColor& color);
    Q_INVOKABLE void resetColors();
    Q_INVOKABLE void setAppPassword(const QString& currentPassword,
                                   const QString& newPassword,
                                   const QString& confirmPassword);
    Q_INVOKABLE void disableAppPassword(const QString& currentPassword);
    Q_INVOKABLE void unlock(const QString& password);
    Q_INVOKABLE void lockNow();
    Q_INVOKABLE void clearError();

signals:
    void languageChanged();
    void darkModeChanged();
    void colorsChanged();
    void lockStateChanged();
    void busyChanged();
    void errorChanged();
    void lockRemainingSecondsChanged();
    void operationFinished(bool success);

private:
    struct Credential {
        QByteArray salt;
        QByteArray verifier;
        int failedAttempts = 0;
        qint64 blockedUntil = 0;
    };
    enum class Operation { Unlock, SetPassword, DisablePassword };
    void initialize();
    bool saveCredential(const Credential& credential);
    bool syncStorage();
    bool canAuthenticate(bool requireUnlocked);
    void startOperation(Operation operation, QByteArray currentPassword,
                        QByteArray newPassword = {});
    void reject(const QString& code);
    void setError(const QString& code);
    void finish(bool success);
    void recordFailure();
    void tickLockout();
    void writeColor(const QString& role, const QColor& color);
    QString colorKey(const QString& role) const;

    std::unique_ptr<QSettings> settings_;
    QString language_ = QStringLiteral("ru");
    bool dark_mode_ = false;
    bool locked_ = false;
    bool lock_enabled_ = false;
    bool busy_ = false;
    bool credential_corrupt_ = false;
    QString error_code_;
    Credential credential_;
    quint64 lock_epoch_ = 0;
    QTimer lockout_timer_;
    int last_remaining_ = 0;
};
