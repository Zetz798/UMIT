#include "app_settings.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cstring>
#include <limits>

namespace {
constexpr int iterations = 600000;
constexpr int salt_size = 16;
constexpr int verifier_size = 32;
constexpr int attempt_limit = 5;
constexpr qint64 cooldown_ms = 60000;
const QString credential_key = QStringLiteral("privacy/credential");

QByteArray randomSalt()
{
    quint32 words[salt_size / sizeof(quint32)];
    QRandomGenerator::system()->fillRange(words);
    return QByteArray(reinterpret_cast<const char*>(words), sizeof(words));
}

QByteArray derive(const QByteArray& password, const QByteArray& salt)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                             password, salt, iterations, verifier_size);
}

bool sameVerifier(const QByteArray& left, const QByteArray& right)
{
    if (left.size() != verifier_size || right.size() != verifier_size)
        return false;
    // Fixed-length comparison does not exit when the first differing byte is found.
    volatile unsigned int difference = 0;
    for (int i = 0; i < verifier_size; ++i)
        difference = difference | (static_cast<unsigned char>(left[i]) ^
                                    static_cast<unsigned char>(right[i]));
    return difference == 0;
}

bool validPassword(const QString& password)
{
    return password.size() >= 8 && password.size() <= 128 && password.toUtf8().size() <= 512;
}

struct WorkResult {
    bool verified = false;
    bool cryptoError = false;
    QByteArray salt;
    QByteArray verifier;
};
} // namespace

AppSettings::AppSettings(QObject* parent)
    : QObject(parent), settings_(std::make_unique<QSettings>(
          QSettings::IniFormat, QSettings::UserScope, QStringLiteral("UMIT"), QStringLiteral("UMIT")))
{
    initialize();
}

AppSettings::AppSettings(const QString& storageFile, QObject* parent)
    : QObject(parent), settings_(std::make_unique<QSettings>(storageFile, QSettings::IniFormat))
{
    initialize();
}

AppSettings::~AppSettings() = default;

void AppSettings::initialize()
{
    const QString stored_language = settings_->value(QStringLiteral("appearance/language"), "ru").toString();
    if (stored_language == "ru" || stored_language == "kk" || stored_language == "en")
        language_ = stored_language;
    dark_mode_ = settings_->value(QStringLiteral("appearance/darkMode"), false).toBool();
    connect(&lockout_timer_, &QTimer::timeout, this, &AppSettings::tickLockout);
    lockout_timer_.setInterval(250);

    if (!settings_->contains(credential_key) && settings_->status() == QSettings::NoError)
        return;

    // A record that exists but cannot be parsed must never silently disable the lock.
    lock_enabled_ = true;
    locked_ = true;
    const auto document = QJsonDocument::fromJson(settings_->value(credential_key).toByteArray());
    const QJsonObject record = document.object();
    const auto salt_text = record.value("salt").toString().toLatin1();
    const auto verifier_text = record.value("verifier").toString().toLatin1();
    const auto salt = QByteArray::fromBase64(salt_text, QByteArray::AbortOnBase64DecodingErrors);
    const auto verifier = QByteArray::fromBase64(verifier_text, QByteArray::AbortOnBase64DecodingErrors);
    const int failures = record.value("failedAttempts").toInt(-1);
    bool timestamp_ok = false;
    const qint64 deadline = record.value("blockedUntil").toString().toLongLong(&timestamp_ok);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (settings_->status() != QSettings::NoError || !document.isObject() ||
        record.value("version").toInt(-1) != 1 || record.value("kdf").toString() != "pbkdf2-sha256" ||
        record.value("iterations").toInt(-1) != iterations ||
        salt.size() != salt_size || salt.toBase64() != salt_text ||
        verifier.size() != verifier_size || verifier.toBase64() != verifier_text ||
        failures < 0 || failures > attempt_limit || !timestamp_ok || deadline < 0 ||
        deadline > now + cooldown_ms * 2 || (failures == attempt_limit && deadline == 0) ||
        (failures < attempt_limit && deadline != 0)) {
        credential_corrupt_ = true;
        setError(QStringLiteral("credential_corrupt"));
        return;
    }
    credential_ = {salt, verifier, failures, deadline};
    tickLockout();
    if (lockRemainingSeconds() > 0)
        lockout_timer_.start();
}

QString AppSettings::colorKey(const QString& role) const
{
    return QStringLiteral("appearance/colors/%1/%2").arg(dark_mode_ ? "dark" : "light", role);
}

QColor AppSettings::backgroundColor() const
{
    const QColor color(settings_->value(colorKey(QStringLiteral("background"))).toString());
    return color.isValid() && color.alpha() == 255 ? color : QColor(dark_mode_ ? "#111827" : "#f5f7fb");
}

QColor AppSettings::textColor() const
{
    const QColor color(settings_->value(colorKey(QStringLiteral("text"))).toString());
    return color.isValid() && color.alpha() == 255 ? color : QColor(dark_mode_ ? "#f3f4f6" : "#172033");
}

void AppSettings::setLanguage(const QString& language)
{
    if (language != "ru" && language != "kk" && language != "en") {
        setError(QStringLiteral("invalid_language"));
        return;
    }
    if (language_ == language)
        return;
    settings_->setValue(QStringLiteral("appearance/language"), language);
    if (syncStorage()) {
        language_ = language;
        emit languageChanged();
    }
}

void AppSettings::setDarkMode(bool enabled)
{
    if (dark_mode_ == enabled)
        return;
    settings_->setValue(QStringLiteral("appearance/darkMode"), enabled);
    if (syncStorage()) {
        dark_mode_ = enabled;
        emit darkModeChanged();
        emit colorsChanged();
    }
}

void AppSettings::writeColor(const QString& role, const QColor& color)
{
    if (!color.isValid() || color.alpha() != 255) {
        setError(QStringLiteral("invalid_color"));
        return;
    }
    settings_->setValue(colorKey(role), color.name(QColor::HexRgb));
    if (syncStorage())
        emit colorsChanged();
}

void AppSettings::setBackgroundColor(const QColor& color) { writeColor(QStringLiteral("background"), color); }
void AppSettings::setTextColor(const QColor& color) { writeColor(QStringLiteral("text"), color); }

void AppSettings::resetColors()
{
    settings_->remove(QStringLiteral("appearance/colors"));
    if (syncStorage())
        emit colorsChanged();
}

bool AppSettings::syncStorage()
{
    settings_->sync();
    if (settings_->status() == QSettings::NoError)
        return true;
    setError(QStringLiteral("storage_error"));
    return false;
}

bool AppSettings::saveCredential(const Credential& credential)
{
    const QJsonObject record{{"version", 1}, {"kdf", "pbkdf2-sha256"}, {"iterations", iterations},
        {"salt", QString::fromLatin1(credential.salt.toBase64())},
        {"verifier", QString::fromLatin1(credential.verifier.toBase64())},
        {"failedAttempts", credential.failedAttempts},
        {"blockedUntil", QString::number(credential.blockedUntil)}};
    // One atomically synced QSettings value prevents partial credential metadata.
    settings_->setValue(credential_key, QJsonDocument(record).toJson(QJsonDocument::Compact));
    return syncStorage();
}

int AppSettings::lockRemainingSeconds() const
{
    const qint64 remaining = credential_.blockedUntil - QDateTime::currentMSecsSinceEpoch();
    return remaining > 0 ? static_cast<int>(std::min<qint64>((remaining + 999) / 1000,
                                             std::numeric_limits<int>::max())) : 0;
}

void AppSettings::tickLockout()
{
    const int remaining = lockRemainingSeconds();
    if (remaining != last_remaining_) {
        last_remaining_ = remaining;
        emit lockRemainingSecondsChanged();
    }
    if (remaining == 0) {
        lockout_timer_.stop();
        if (error_code_ == "lockout")
            clearError();
    }
}

bool AppSettings::canAuthenticate(bool requireUnlocked)
{
    if (busy_)
        return false;
    if (credential_corrupt_) {
        reject(QStringLiteral("credential_corrupt"));
        return false;
    }
    if (lockRemainingSeconds() > 0) {
        reject(QStringLiteral("lockout"));
        return false;
    }
    if (requireUnlocked && locked_) {
        reject(QStringLiteral("app_locked"));
        return false;
    }
    return true;
}

void AppSettings::setAppPassword(const QString& currentPassword, const QString& newPassword,
                                 const QString& confirmPassword)
{
    if (!canAuthenticate(true))
        return;
    if (!validPassword(newPassword)) {
        reject(QStringLiteral("password_length"));
        return;
    }
    if (newPassword != confirmPassword) {
        reject(QStringLiteral("password_mismatch"));
        return;
    }
    if (lock_enabled_ && !validPassword(currentPassword)) {
        recordFailure();
        emit operationFinished(false);
        return;
    }
    startOperation(Operation::SetPassword, currentPassword.toUtf8(), newPassword.toUtf8());
}

void AppSettings::disableAppPassword(const QString& currentPassword)
{
    if (!canAuthenticate(true))
        return;
    if (!lock_enabled_) {
        reject(QStringLiteral("no_password"));
        return;
    }
    if (!validPassword(currentPassword)) {
        recordFailure();
        emit operationFinished(false);
        return;
    }
    startOperation(Operation::DisablePassword, currentPassword.toUtf8());
}

void AppSettings::unlock(const QString& password)
{
    if (!canAuthenticate(false))
        return;
    if (!lock_enabled_) {
        reject(QStringLiteral("no_password"));
        return;
    }
    if (!validPassword(password)) {
        recordFailure();
        emit operationFinished(false);
        return;
    }
    startOperation(Operation::Unlock, password.toUtf8());
}

void AppSettings::startOperation(Operation operation, QByteArray currentPassword, QByteArray newPassword)
{
    clearError();
    busy_ = true;
    emit busyChanged();
    const bool had_password = lock_enabled_;
    const quint64 started_epoch = lock_epoch_;
    const Credential saved = credential_;
    auto* watcher = new QFutureWatcher<WorkResult>(this);
    connect(watcher, &QFutureWatcher<WorkResult>::finished, this,
            [this, watcher, operation, had_password, started_epoch] {
        const WorkResult result = watcher->result();
        watcher->deleteLater();
        if (result.cryptoError) {
            setError(QStringLiteral("crypto_error"));
            finish(false);
            return;
        }
        if (!result.verified) {
            recordFailure();
            finish(false);
            return;
        }
        if (started_epoch != lock_epoch_ && had_password) {
            setError(QStringLiteral("app_locked"));
            finish(false);
            return;
        }
        if (operation == Operation::DisablePassword) {
            settings_->remove(credential_key);
            if (!syncStorage()) {
                // Restore the known verifier in memory if saving failed.
                saveCredential(credential_);
                finish(false);
                return;
            }
            credential_ = {};
            lock_enabled_ = false;
            locked_ = false;
        } else {
            Credential updated = credential_;
            updated.failedAttempts = 0;
            updated.blockedUntil = 0;
            if (operation == Operation::SetPassword) {
                updated.salt = result.salt;
                updated.verifier = result.verifier;
            }
            if (!saveCredential(updated)) {
                // An uncertain initial write must not create an unlocked session.
                if (!had_password) {
                    credential_ = updated;
                    lock_enabled_ = true;
                    locked_ = true;
                    emit lockStateChanged();
                }
                finish(false);
                return;
            }
            credential_ = updated;
            lock_enabled_ = true;
            locked_ = started_epoch != lock_epoch_;
        }
        tickLockout();
        emit lockStateChanged();
        finish(true);
    });
    watcher->setFuture(QtConcurrent::run([operation, had_password, saved,
                                        currentPassword = std::move(currentPassword),
                                        newPassword = std::move(newPassword)]() mutable {
        WorkResult result;
        if (had_password) {
            QByteArray candidate = derive(currentPassword, saved.salt);
            result.cryptoError = candidate.size() != verifier_size;
            result.verified = sameVerifier(candidate, saved.verifier);
            candidate.fill('\0');
        } else {
            result.verified = true;
        }
        currentPassword.fill('\0');
        if (result.verified && operation == Operation::SetPassword) {
            result.salt = randomSalt();
            result.verifier = derive(newPassword, result.salt);
            result.cryptoError = result.verifier.size() != verifier_size;
        }
        newPassword.fill('\0');
        return result;
    }));
}

void AppSettings::recordFailure()
{
    if (credential_.blockedUntil != 0 && lockRemainingSeconds() == 0) {
        credential_.failedAttempts = 0;
        credential_.blockedUntil = 0;
    }
    credential_.failedAttempts = std::min(credential_.failedAttempts + 1, attempt_limit);
    if (credential_.failedAttempts >= attempt_limit) {
        credential_.blockedUntil = QDateTime::currentMSecsSinceEpoch() + cooldown_ms;
        locked_ = true;
        emit lockStateChanged();
        lockout_timer_.start();
    }
    if (saveCredential(credential_))
        setError(lockRemainingSeconds() > 0 ? QStringLiteral("lockout") : QStringLiteral("wrong_password"));
    else {
        locked_ = true;
        emit lockStateChanged();
    }
    tickLockout();
}

void AppSettings::lockNow()
{
    ++lock_epoch_;
    if (lock_enabled_ && !locked_) {
        locked_ = true;
        emit lockStateChanged();
    }
}

void AppSettings::setError(const QString& code)
{
    if (error_code_ != code) {
        error_code_ = code;
        emit errorChanged();
    }
}

void AppSettings::clearError() { setError({}); }
void AppSettings::reject(const QString& code) { setError(code); emit operationFinished(false); }
void AppSettings::finish(bool success) { busy_ = false; emit busyChanged(); emit operationFinished(success); }
