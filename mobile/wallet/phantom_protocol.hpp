#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <optional>

namespace phantom {
QString base58(const QByteArray& bytes);
std::optional<QByteArray> unbase58(const QString& text, int maxBytes);
QByteArray randomBytes(int count);
void erase(QByteArray& bytes);

// Only the encryption key is generated here. The wallet's signing key stays in Phantom.
class Connection {
public:
    enum class Reply { Ignored, Connected, Rejected, Invalid };
    ~Connection();
    Connection() = default;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    QUrl begin();
    Reply accept(const QUrl& url);
    void reset();
    bool pending() const { return !state_.isEmpty(); }
    QString address() const { return address_; }

private:
    QByteArray secret_;
    QByteArray session_;
    QString state_;
    QString address_;
};
}
