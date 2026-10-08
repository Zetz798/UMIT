#pragma once
#include "phantom_protocol.hpp"
#include <QObject>
#include <QTimer>
#include <QNetworkAccessManager>

// Returns an empty string for errors/malformed responses, never a fabricated zero.
QString parseDevnetBalance(const QByteArray& response);

class WalletController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString buttonText READ buttonText NOTIFY changed)
    Q_PROPERTY(QString balanceText READ balanceText NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString address READ address NOTIFY changed)
    Q_PROPERTY(QString shortAddress READ shortAddress NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY changed)
public:
    explicit WalletController(QObject* parent = nullptr);
    ~WalletController() override;
    QString buttonText() const;
    QString balanceText() const;
    QString statusText() const;
    QString address() const { return connection_.address(); }
    QString shortAddress() const;
    bool connected() const { return !connection_.address().isEmpty(); }
    bool busy() const { return connection_.pending(); }
    bool refreshing() const { return refreshing_; }
    Q_INVOKABLE void connectWallet();
    Q_INVOKABLE void resetConnection();
    Q_INVOKABLE void refreshBalance();
    Q_INVOKABLE void setLanguage(const QString& code);
public slots:
    void handleUrl(const QUrl& url);
signals:
    void changed();
private:
    phantom::Connection connection_;
    QTimer timeout_;
    QNetworkAccessManager network_;
    enum class Status { Initial, Timeout, Reset, MobileOnly, Confirm, OpenFailed, Connected, Rejected, InvalidReply };
    enum class Balance { Unknown, Loading, Ready, Unavailable };
    QString text(const char* ru, const char* kk, const char* en) const;
    QString language_ = QStringLiteral("ru");
    QString balance_;
    Status status_ = Status::Initial;
    Balance balance_state_ = Balance::Unknown;
    quint64 generation_ = 0;
    bool refreshing_ = false;
    void receiveBalance(const QByteArray& data, quint64 generation);
};
