#include "wallet_controller.hpp"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QThread>
#ifdef Q_OS_ANDROID
#include <QFutureWatcher>
#include <QJniObject>
#include <QtConcurrentRun>
#endif

QString parseDevnetBalance(const QByteArray& response) {
    if (response.size() > 16384) return {};
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(response, &error);
    const auto obj = doc.object();
    const auto value = obj.value(QStringLiteral("result")).toObject().value(QStringLiteral("value"));
    const auto lamports = value.toInteger(-1);
    if (error.error != QJsonParseError::NoError || obj.value(QStringLiteral("jsonrpc")) != QStringLiteral("2.0") ||
        obj.value(QStringLiteral("id")).toInt(-1) != 1 || obj.contains(QStringLiteral("error")) ||
        !value.isDouble() || lamports < 0 || value.toDouble() != static_cast<double>(lamports)) return {};
    QString fraction = QString::number(lamports % 1000000000).rightJustified(9, QLatin1Char('0'));
    while (fraction.endsWith(QLatin1Char('0'))) fraction.chop(1);
    return QString::number(lamports / 1000000000) +
        (fraction.isEmpty() ? QString() : QStringLiteral(".") + fraction) + QStringLiteral(" SOL · Devnet");
}

WalletController::WalletController(QObject* parent) : QObject(parent) {
    QDesktopServices::setUrlHandler(QStringLiteral("ticketdemo"), this, "handleUrl");
    timeout_.setSingleShot(true);
    timeout_.setInterval(180000);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        resetConnection();
        status_ = Status::Timeout;
        emit changed();
    });
}
WalletController::~WalletController() {
    QDesktopServices::unsetUrlHandler(QStringLiteral("ticketdemo"));
}
QString WalletController::buttonText() const {
    if (busy()) return text("Ожидаем Phantom…", "Phantom жауабын күтудеміз…", "Waiting for Phantom…");
    return connected() ? shortAddress() : text("Подключить кошелёк", "Әмиянды қосу", "Connect wallet");
}
QString WalletController::shortAddress() const {
    const auto value = connection_.address();
    return value.isEmpty() ? QString() : value.left(4) + QStringLiteral("…") + value.right(4);
}
QString WalletController::text(const char* ru, const char* kk, const char* en) const {
    return QString::fromUtf8(language_ == QStringLiteral("kk") ? kk :
                             language_ == QStringLiteral("en") ? en : ru);
}
void WalletController::setLanguage(const QString& code) {
    const QString normalized = code == QStringLiteral("kz") ? QStringLiteral("kk") : code;
    if (normalized != QStringLiteral("ru") && normalized != QStringLiteral("kk") &&
        normalized != QStringLiteral("en")) return;
    if (language_ == normalized) return;
    language_ = normalized;
    emit changed();
}
QString WalletController::balanceText() const {
    switch (balance_state_) {
    case Balance::Unknown: return QStringLiteral("— SOL · Devnet");
    case Balance::Loading: return text("Загружаем баланс…", "Баланс жүктелуде…", "Loading balance…");
    case Balance::Ready: return balance_;
    case Balance::Unavailable:
        return text("Баланс недоступен · Devnet. Повторите обновление.",
                    "Баланс қолжетімсіз · Devnet. Қайта жаңартыңыз.",
                    "Balance unavailable · Devnet. Try refreshing again.");
    }
    return {};
}
QString WalletController::statusText() const {
    switch (status_) {
    case Status::Initial:
        return text("Подключите Phantom для просмотра баланса Devnet.",
                    "Devnet балансын көру үшін Phantom әмиянын қосыңыз.",
                    "Connect Phantom to view your Devnet balance.");
    case Status::Timeout:
        return text("Phantom не ответил. Нажмите «Подключить кошелёк» ещё раз.",
                    "Phantom жауап бермеді. «Әмиянды қосу» түймесін қайта басыңыз.",
                    "Phantom did not respond. Tap Connect wallet again.");
    case Status::Reset:
        return text("Кошелёк отключён в приложении. Для смены аккаунта подключитесь заново.",
                    "Әмиян қолданбадан ажыратылды. Аккаунтты ауыстыру үшін қайта қосылыңыз.",
                    "Wallet disconnected from the app. Connect again to switch accounts.");
    case Status::MobileOnly:
        return text("Подключение Phantom доступно в мобильной сборке. На компьютере браузерное расширение не подключается к Qt-окну.",
                    "Phantom қосылымы мобильді нұсқада қолжетімді. Компьютердегі браузер кеңейтімі Qt терезесіне қосылмайды.",
                    "Phantom connection is available in the mobile build. The desktop browser extension cannot connect to this Qt window.");
    case Status::Confirm:
        return text("Подтвердите подключение в Phantom. Сеть: Devnet.",
                    "Phantom ішінде қосылымды растаңыз. Желі: Devnet.",
                    "Approve the connection in Phantom. Network: Devnet.");
    case Status::OpenFailed:
        return text("Не удалось открыть Phantom. Установите или откройте кошелёк на этом телефоне и повторите подключение.",
                    "Phantom ашылмады. Осы телефонға әмиянды орнатыңыз немесе ашыңыз да, қайта қосылыңыз.",
                    "Could not open Phantom. Install or open the wallet on this phone, then connect again.");
    case Status::Connected:
        return text("Phantom подключён. Билеты пока работают в локальном демо.",
                    "Phantom қосылды. Билеттер әзірге жергілікті демода жұмыс істейді.",
                    "Phantom connected. Tickets currently work in the local demo.");
    case Status::Rejected:
        return text("Подключение отклонено в Phantom. Можно повторить.",
                    "Phantom ішінде қосылым қабылданбады. Қайта қосылып көріңіз.",
                    "Connection declined in Phantom. You can try again.");
    case Status::InvalidReply:
        return text("Не удалось проверить ответ Phantom. Подключитесь заново.",
                    "Phantom жауабын тексеру мүмкін болмады. Қайта қосылыңыз.",
                    "Could not verify Phantom's response. Connect again.");
    }
    return {};
}
void WalletController::resetConnection() {
    ++generation_; // Ignore replies belonging to an earlier address/session.
    timeout_.stop();
    connection_.reset();
    refreshing_ = false;
    balance_.clear();
    balance_state_ = Balance::Unknown;
    status_ = Status::Reset;
    emit changed();
}
void WalletController::connectWallet() {
    if (busy()) return;
    if (connected()) { refreshBalance(); return; }
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    status_ = Status::MobileOnly;
    emit changed();
    return;
#else
    ++generation_;
    const QUrl url = connection_.begin();
    status_ = Status::Confirm;
    timeout_.start();
    emit changed();
#ifdef Q_OS_ANDROID
    const auto context = QNativeInterface::QAndroidApplication::context();
    const auto text = QJniObject::fromString(url.toString(QUrl::FullyEncoded));
    const bool opened = QJniObject::callStaticMethod<jboolean>(
        "org/example/ticketdemo/PhantomBridge", "openPhantom",
        "(Landroid/content/Context;Ljava/lang/String;)Z", context.object(), text.object<jstring>());
#else
    const bool opened = QDesktopServices::openUrl(url);
#endif
    if (!opened) {
        resetConnection();
        status_ = Status::OpenFailed;
        emit changed();
    }
#endif
}
void WalletController::handleUrl(const QUrl& url) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, url] { handleUrl(url); }, Qt::QueuedConnection);
        return;
    }
    const auto result = connection_.accept(url);
    if (result == phantom::Connection::Reply::Ignored) return;
    timeout_.stop();
    if (result == phantom::Connection::Reply::Connected) {
        status_ = Status::Connected;
        emit changed();
        refreshBalance();
    } else {
        status_ = result == phantom::Connection::Reply::Rejected ? Status::Rejected : Status::InvalidReply;
        emit changed();
    }
}
void WalletController::refreshBalance() {
    if (!connected() || refreshing_) return;
    refreshing_ = true;
    balance_state_ = Balance::Loading;
    emit changed();
    const quint64 generation = generation_;
    const QJsonObject payload{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "getBalance"},
        {"params", QJsonArray{connection_.address(), QJsonObject{{"commitment", "finalized"}}}}};
    const auto body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
#ifdef Q_OS_ANDROID
    // Android's certificate store and HTTPS stack; no missing OpenSSL .so at runtime.
    auto* watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, generation] {
        receiveBalance(watcher->result(), generation);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([body] {
        const auto argument = QJniObject::fromString(QString::fromUtf8(body));
        const auto response = QJniObject::callStaticObjectMethod(
            "org/example/ticketdemo/PhantomBridge", "devnetBalance",
            "(Ljava/lang/String;)Ljava/lang/String;", argument.object<jstring>());
        return response.toString().toUtf8();
    }));
#else
    QNetworkRequest request(QUrl(QStringLiteral("https://api.devnet.solana.com")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15000);
    auto* reply = network_.post(request, body);
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 16384) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        const bool ok = reply->error() == QNetworkReply::NoError &&
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        receiveBalance(ok ? reply->readAll() : QByteArray(), generation);
        reply->deleteLater();
    });
#endif
}
void WalletController::receiveBalance(const QByteArray& data, quint64 generation) {
    if (generation != generation_ || !connected()) return;
    refreshing_ = false;
    balance_ = parseDevnetBalance(data);
    balance_state_ = balance_.isEmpty() ? Balance::Unavailable : Balance::Ready;
    emit changed();
}
