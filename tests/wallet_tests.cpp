#include "phantom_protocol.hpp"
#include "wallet_controller.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QtTest>
extern "C" {
#include "tweetnacl.h"
}

namespace {
unsigned char* bytes(QByteArray& b) { return reinterpret_cast<unsigned char*>(b.data()); }
const unsigned char* bytes(const QByteArray& b) { return reinterpret_cast<const unsigned char*>(b.constData()); }
QUrl replyFor(const QUrl& request, const QByteArray& json) {
    const QUrlQuery requestQuery(request);
    const auto dappKey = phantom::unbase58(requestQuery.queryItemValue("dapp_encryption_public_key"), 32).value();
    const QByteArray phantomSecret(32, '\x42');
    QByteArray phantomPublic(32, '\0');
    crypto_scalarmult_base(bytes(phantomPublic), bytes(phantomSecret));
    const QByteArray nonce(24, '\x17');
    QByteArray padded(32, '\0');
    padded += json;
    QByteArray encrypted(padded.size(), '\0');
    crypto_box(bytes(encrypted), bytes(padded), static_cast<unsigned long long>(padded.size()),
               bytes(nonce), bytes(dappKey), bytes(phantomSecret));
    QUrl callback(requestQuery.queryItemValue("redirect_link", QUrl::FullyDecoded));
    QUrlQuery query(callback);
    query.addQueryItem("phantom_encryption_public_key", phantom::base58(phantomPublic));
    query.addQueryItem("nonce", phantom::base58(nonce));
    query.addQueryItem("data", phantom::base58(encrypted.mid(16)));
    callback.setQuery(query);
    return callback;
}
const QByteArray validJson = R"({"public_key":"11111111111111111111111111111111","session":"123456789ABCDEFGH"})";
}

class WalletTests : public QObject {
    Q_OBJECT
private slots:
    void tweetNaClJsInterop() {
        // Independent fixture: TweetNaCl.js 1.0.3, not encrypted by our C wrapper.
        const auto secret = QByteArray::fromHex("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
        const auto peer = QByteArray::fromHex("5714769d116bf76436ae74bc793d2c30ad1903c59ac5273805c7e2698b410c36");
        const auto nonce = QByteArray::fromHex("333435363738393a3b3c3d3e3f404142434445464748494a");
        QByteArray padded(16, '\0');
        padded += QByteArray::fromHex("6f71814ef14d220f600e85e1a8b2d75a4a07a889cdf463ce355a87d05f8a621ee6b94e63f3fba7702304");
        QByteArray decrypted(padded.size(), '\0');
        QCOMPARE(crypto_box_open(bytes(decrypted), bytes(padded), static_cast<unsigned long long>(padded.size()),
                                  bytes(nonce), bytes(peer), bytes(secret)), 0);
        QCOMPARE(decrypted.mid(32), QString::fromUtf8("Phantom Devnet: билет").toUtf8());
    }
    void base58Vectors() {
        QCOMPARE(phantom::base58(QByteArray::fromHex("00000102")), QString("115T"));
        QCOMPARE(phantom::base58(QByteArray("Hello World")), QString("JxF12TrwUP45BMd"));
        QCOMPARE(phantom::unbase58("JxF12TrwUP45BMd", 11).value(), QByteArray("Hello World"));
        QCOMPARE(phantom::unbase58(QString(32, '1'), 32).value(), QByteArray(32, '\0'));
        for (const auto& input : {QString("0"), QString("O"), QString("I"), QString("l"),
                                  QString(" 1"), QString("1\n"), QString("Я"), QString(QChar(0)), QString()})
            QVERIFY(!phantom::unbase58(input, 32));
        QVERIFY(!phantom::unbase58(QString(33, '1'), 32));
        QVERIFY(!phantom::unbase58("zzzzz", 1));
    }
    void encryptedConnection() {
        phantom::Connection connection;
        const auto request = connection.begin();
        QCOMPARE(request.host(), QString("phantom.com"));
        QCOMPARE(QUrlQuery(request).queryItemValue("cluster"), QString("devnet"));
        const auto callback = replyFor(request, validJson);
        QCOMPARE(connection.accept(callback), phantom::Connection::Reply::Connected);
        QCOMPARE(connection.address(), QString(32, '1'));
        QVERIFY(!connection.pending());
        QCOMPARE(connection.accept(callback), phantom::Connection::Reply::Ignored);
        connection.reset();
        QVERIFY(connection.address().isEmpty());
    }
    void staleStateAndWrongRoute() {
        phantom::Connection connection;
        const auto oldRequest = connection.begin();
        const auto newRequest = connection.begin();
        QCOMPARE(connection.accept(replyFor(oldRequest, validJson)), phantom::Connection::Reply::Ignored);
        auto callback = replyFor(newRequest, validJson);
        callback.setHost("evil");
        QCOMPARE(connection.accept(callback), phantom::Connection::Reply::Ignored);
        QVERIFY(connection.pending());
        QCOMPARE(connection.accept(replyFor(newRequest, validJson)), phantom::Connection::Reply::Connected);
    }
    void rejectsBadEncryptionAndPayload() {
        phantom::Connection connection;
        auto callback = replyFor(connection.begin(), validJson);
        QUrlQuery query(callback);
        auto data = phantom::unbase58(query.queryItemValue("data"), 8192).value();
        data[0] ^= 1;
        query.removeAllQueryItems("data");
        query.addQueryItem("data", phantom::base58(data));
        callback.setQuery(query);
        QCOMPARE(connection.accept(callback), phantom::Connection::Reply::Invalid);
        QVERIFY(connection.address().isEmpty());
        for (const auto& json : {QByteArray("not-json"), QByteArray("{}"),
             QByteArray(R"({"public_key":"bad","session":"123"})"),
             QByteArray(R"({"public_key":"11111111111111111111111111111111","session":""})")}) {
            QCOMPARE(connection.accept(replyFor(connection.begin(), json)), phantom::Connection::Reply::Invalid);
            QVERIFY(!connection.pending());
        }
    }
    void duplicatesCancellationAndLowOrderKeys() {
        phantom::Connection connection;
        auto request = connection.begin();
        auto callback = replyFor(request, validJson);
        QUrlQuery query(callback);
        query.addQueryItem("nonce", "1");
        callback.setQuery(query);
        QCOMPARE(connection.accept(callback), phantom::Connection::Reply::Invalid);
        request = connection.begin();
        callback = QUrl(QUrlQuery(request).queryItemValue("redirect_link", QUrl::FullyDecoded));
        query = QUrlQuery(callback);
        query.addQueryItem("errorCode", "4001");
        callback.setQuery(query);
        QCOMPARE(connection.accept(callback), phantom::Connection::Reply::Rejected);
        callback = replyFor(connection.begin(), validJson);
        query = QUrlQuery(callback);
        query.removeAllQueryItems("phantom_encryption_public_key");
        query.addQueryItem("phantom_encryption_public_key", QString(32, '1'));
        callback.setQuery(query);
        QCOMPARE(connection.accept(callback), phantom::Connection::Reply::Invalid);
        callback = replyFor(connection.begin(), validJson);
        connection.reset();
        QCOMPARE(connection.accept(callback), phantom::Connection::Reply::Ignored);
    }
    void balanceErrorsAndPrecision() {
        QCOMPARE(parseDevnetBalance(R"({"jsonrpc":"2.0","id":1,"result":{"value":1000000001}})"),
                 QString::fromUtf8("1.000000001 SOL · Devnet"));
        QCOMPARE(parseDevnetBalance(R"({"jsonrpc":"2.0","id":1,"result":{"value":0}})"),
                 QString::fromUtf8("0 SOL · Devnet"));
        QCOMPARE(parseDevnetBalance(R"({"jsonrpc":"2.0","id":1,"result":{"value":1000010000}})"),
                 QString::fromUtf8("1.00001 SOL · Devnet"));
        for (const auto& response : {QByteArray(), QByteArray("{}"),
             QByteArray(R"({"jsonrpc":"2.0","id":1,"error":{"code":-32603}})"),
             QByteArray(R"({"jsonrpc":"2.0","id":1,"result":{"value":null}})"),
             QByteArray(R"({"jsonrpc":"2.0","id":1,"result":{"value":-1}})"),
             QByteArray(R"({"jsonrpc":"2.0","id":1,"result":{"value":1.5}})"),
             QByteArray(R"({"jsonrpc":"2.0","id":2,"result":{"value":0}})")})
            QVERIFY(parseDevnetBalance(response).isEmpty());
    }
};
QTEST_GUILESS_MAIN(WalletTests)
#include "wallet_tests.moc"
