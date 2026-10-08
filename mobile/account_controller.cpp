#include "account_controller.hpp"
#include <QCryptographicHash>
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QJsonDocument>
#include <QLockFile>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr int iterations = 600000;
QString canonical(const QString& email) { return email.trimmed().toLower(); }
QString accountPath(const QString& dir, const QString& email) {
    const auto id = QCryptographicHash::hash(email.toUtf8(), QCryptographicHash::Sha256).toHex();
    return dir + "/" + QString::fromLatin1(id) + ".json";
}
bool validEmail(const QString& email) {
    static const QRegularExpression pattern(QStringLiteral("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$"));
    return email.size() <= 254 && pattern.match(email).hasMatch();
}
bool validName(const QString& name) {
    if (name.size() < 2 || name.size() > 32) return false;
    for (const auto ch : name) if (!ch.isPrint()) return false;
    return true;
}
QJsonObject load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool write(const QString& path, const QJsonObject& object) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const auto json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (file.write(json) != json.size()) return false;
    return file.commit();
}
bool verify(const QByteArray& a, const QByteArray& b) {
    if (a.size() != 32 || b.size() != 32) return false;
    volatile unsigned int different = 0;
    for (int i = 0; i < 32; ++i) different = different | (uchar(a[i]) ^ uchar(b[i]));
    return different == 0;
}
struct Result { QJsonObject profile; QString error; };
}

AccountController::AccountController(QObject* parent)
    : AccountController(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/accounts", parent) {}
AccountController::AccountController(const QString& dataDirectory, QObject* parent)
    : QObject(parent), directory_(dataDirectory) {}
QUrl AccountController::avatarUrl() const {
    const auto name = profile_.value("avatar").toString();
    if (name.isEmpty() || QFileInfo(name).fileName() != name || !name.endsWith(".png")) return {};
    return QUrl::fromLocalFile(directory_ + "/avatars/" + name);
}
void AccountController::fail(const QString& code) {
    error_ = code; notice_.clear(); emit changed(); emit operationFinished(false);
}
void AccountController::clearFeedback() { error_.clear(); notice_.clear(); emit changed(); }
bool AccountController::ready(bool requireLogin) {
    if (busy_) return false;
    if (app_locked_) { fail("app_locked"); return false; }
    if (requireLogin && !loggedIn()) { fail("login_required"); return false; }
    return true;
}
void AccountController::setAppLocked(bool locked) {
    app_locked_ = locked;
    if (locked) ++epoch_; // Late password results cannot reopen a locked UI session.
}
void AccountController::registerAccount(const QString& input, const QString& password,
                                       const QString& confirm, const QString& nickname) {
    if (!ready()) return;
    if (loggedIn()) { fail("generic_error"); return; }
    if (!validEmail(canonical(input))) { fail("email_invalid"); return; }
    if (!validName(nickname.trimmed())) { fail("nickname_invalid"); return; }
    if (password.size() < 8 || password.size() > 128) { fail("password_length"); return; }
    if (password != confirm) { fail("password_mismatch"); return; }
    authenticate(canonical(input), password, nickname.trimmed(), true);
}
void AccountController::login(const QString& input, const QString& password) {
    if (!ready()) return;
    if (loggedIn()) { fail("generic_error"); return; }
    if (!validEmail(canonical(input))) { fail("email_invalid"); return; }
    if (password.size() > 128) { fail("password_length"); return; }
    authenticate(canonical(input), password, {}, false);
}
void AccountController::authenticate(const QString& email, const QString& password,
                                     const QString& nickname, bool create) {
    clearFeedback(); busy_ = true; emit changed();
    const auto epoch = epoch_;
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, epoch, create] {
        const Result result = watcher->result(); watcher->deleteLater(); busy_ = false;
        if (epoch != epoch_) { fail(app_locked_ ? "app_locked" : "generic_error"); return; }
        if (!result.error.isEmpty()) { fail(result.error); return; }
        profile_ = result.profile;
        notice_ = create ? "registered" : "logged_in";
        emit changed(); emit sessionChanged(); emit operationFinished(true);
    });
    watcher->setFuture(QtConcurrent::run([dir = directory_, email, nickname, create, secret = password.toUtf8()]() mutable {
        if (!QDir().mkpath(dir)) return Result{{}, "storage_error"};
        QLockFile lock(dir + "/accounts.lock");
        if (!lock.tryLock(0)) return Result{{}, "storage_error"};
        const QString path = accountPath(dir, email);
        QJsonObject profile = load(path);
        if (create && QFile::exists(path)) return Result{{}, "account_exists"};
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const QString throttlePath = dir + "/login-attempts.json";
        auto throttle = load(throttlePath);
        if (throttle.value("until").toString().toLongLong() > now) return Result{{}, "locked_out"};
        QByteArray salt;
        if (create) {
            quint32 random[4]; QRandomGenerator::system()->fillRange(random);
            salt = QByteArray(reinterpret_cast<const char*>(random), sizeof(random));
        } else salt = QByteArray::fromBase64(profile.value("salt").toString().toLatin1());
        const bool validRecord = profile.value("email").toString() == email &&
            profile.value("iterations").toInt() == iterations &&
            profile.value("version").toInt() == 1 && salt.size() == 16;
        const auto hash = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, secret,
            salt.size() == 16 ? salt : QByteArray(16, '\x21'), iterations, 32);
        secret.fill('\0');
        if (hash.size() != 32) return Result{{}, "generic_error"};
        if (create) {
            profile = {{"version", 1}, {"email", email}, {"nickname", nickname}, {"iterations", iterations},
                {"salt", QString::fromLatin1(salt.toBase64())}, {"verifier", QString::fromLatin1(hash.toBase64())}};
            if (!write(path, profile)) return Result{{}, "storage_error"};
        } else if (!validRecord || !verify(hash, QByteArray::fromBase64(profile.value("verifier").toString().toLatin1()))) {
            int failures = throttle.value("failures").toInt();
            if (throttle.value("until").toString().toLongLong() != 0) failures = 0;
            ++failures;
            if (!write(throttlePath, {{"failures", failures}, {"until", QString::number(failures >= 5 ? now + 60000 : 0)}}))
                return Result{{}, "storage_error"};
            return Result{{}, failures >= 5 ? "locked_out" : "invalid_credentials"};
        }
        if (!write(throttlePath, {{"failures", 0}, {"until", "0"}})) return Result{{}, "storage_error"};
        return Result{profile, {}};
    }));
}
void AccountController::logout() {
    ++epoch_; profile_ = {}; clearFeedback(); notice_ = "logged_out";
    emit changed(); emit sessionChanged();
}
bool AccountController::saveProfile(QJsonObject updated, const QString& notice) {
    QLockFile lock(directory_ + "/accounts.lock");
    if (!lock.tryLock(0) || !write(accountPath(directory_, email()), updated)) { fail("storage_error"); return false; }
    const auto oldAvatar = avatarUrl();
    profile_ = std::move(updated); error_.clear(); notice_ = notice;
    if (!oldAvatar.isEmpty() && oldAvatar != avatarUrl()) QFile::remove(oldAvatar.toLocalFile());
    emit changed(); emit operationFinished(true);
    return true;
}
void AccountController::updateProfile(const QString& nickname) {
    if (!ready(true)) return;
    if (!validName(nickname.trimmed())) { fail("nickname_invalid"); return; }
    auto updated = profile_; updated["nickname"] = nickname.trimmed(); saveProfile(updated, "profile_saved");
}
void AccountController::removeAvatar() {
    if (!ready(true)) return;
    auto updated = profile_; updated.remove("avatar"); saveProfile(updated, "avatar_removed");
}
void AccountController::setAvatar(const QUrl& source) {
    if (!ready(true)) return;
    if (!source.isLocalFile() && source.scheme() != "content") { fail("avatar_invalid"); return; }
    clearFeedback(); busy_ = true; emit changed();
    const auto epoch = epoch_;
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, epoch] {
        const QImage image = watcher->result(); watcher->deleteLater(); busy_ = false;
        if (epoch != epoch_ || !loggedIn() || app_locked_) { fail("login_required"); return; }
        if (image.isNull()) { fail("avatar_invalid"); return; }
        const QString name = QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
        if (!QDir().mkpath(directory_ + "/avatars")) { fail("storage_error"); return; }
        QSaveFile output(directory_ + "/avatars/" + name);
        if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") || !output.commit()) { fail("storage_error"); return; }
        auto updated = profile_; updated["avatar"] = name;
        if (!saveProfile(updated, "avatar_saved")) QFile::remove(directory_ + "/avatars/" + name);
    });
    watcher->setFuture(QtConcurrent::run([source] {
        QFile file(source.isLocalFile() ? source.toLocalFile() : source.toString());
        if (!file.open(QIODevice::ReadOnly) || file.size() > 20 * 1024 * 1024) return QImage();
        const auto encoded = file.read(20 * 1024 * 1024 + 1);
        if (encoded.size() > 20 * 1024 * 1024) return QImage();
        QBuffer buffer; buffer.setData(encoded); buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer); reader.setAutoTransform(true);
        const auto format = reader.format().toLower();
        if (format != "png" && format != "jpeg" && format != "jpg" && format != "webp") return QImage();
        const auto size = reader.size();
        if (!size.isValid() || qint64(size.width()) * size.height() > 64000000) return QImage();
        reader.setScaledSize(size.scaled(512, 512, Qt::KeepAspectRatio));
        return reader.read().scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }));
}
