#include "phantom_protocol.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QUrlQuery>
#include <algorithm>
#include <cstring>
#include <vector>
extern "C" {
#include "tweetnacl.h"
}

// TweetNaCl delegates randomness to the application. Use the OS CSPRNG, never rand().
extern "C" void randombytes(unsigned char* out, unsigned long long count) {
    while (count) {
        const auto word = QRandomGenerator::system()->generate();
        const auto n = std::min<unsigned long long>(count, sizeof(word));
        std::memcpy(out, &word, static_cast<size_t>(n));
        out += n;
        count -= n;
    }
}

namespace phantom {
namespace {
constexpr char alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const unsigned char* ptr(const QByteArray& a) {
    return reinterpret_cast<const unsigned char*>(a.constData());
}
unsigned char* ptr(QByteArray& a) { return reinterpret_cast<unsigned char*>(a.data()); }
}

QString base58(const QByteArray& bytes) {
    int zeros = 0;
    while (zeros < bytes.size() && bytes[zeros] == 0) ++zeros;
    std::vector<unsigned char> digits(static_cast<size_t>((bytes.size() - zeros) * 138 / 100 + 1));
    int length = 0;
    for (qsizetype i = zeros; i < bytes.size(); ++i) {
        int carry = static_cast<unsigned char>(bytes[i]);
        int j = 0;
        for (auto it = digits.rbegin(); (carry || j < length) && it != digits.rend(); ++it, ++j) {
            carry += 256 * *it;
            *it = static_cast<unsigned char>(carry % 58);
            carry /= 58;
        }
        length = j;
    }
    QString result(zeros, QLatin1Char('1'));
    for (auto it = digits.end() - length; it != digits.end(); ++it) result += QLatin1Char(alphabet[*it]);
    return result;
}

std::optional<QByteArray> unbase58(const QString& text, int maxBytes) {
    if (text.isEmpty() || text.size() > maxBytes * 138 / 100 + 1) return std::nullopt;
    int zeros = 0;
    while (zeros < text.size() && text[zeros] == QLatin1Char('1')) ++zeros;
    if (zeros > maxBytes) return std::nullopt;
    std::vector<unsigned char> bytes(static_cast<size_t>((text.size() - zeros) * 733 / 1000 + 1));
    int length = 0;
    for (qsizetype i = zeros; i < text.size(); ++i) {
        if (text[i].unicode() > 127 || text[i].isNull()) return std::nullopt;
        const auto digit = std::strchr(alphabet, text[i].toLatin1());
        if (!digit) return std::nullopt;
        int carry = static_cast<int>(digit - alphabet);
        int j = 0;
        for (auto it = bytes.rbegin(); (carry || j < length) && it != bytes.rend(); ++it, ++j) {
            carry += 58 * *it;
            *it = static_cast<unsigned char>(carry % 256);
            carry /= 256;
        }
        length = j;
        if (zeros + length > maxBytes) return std::nullopt;
    }
    QByteArray result(zeros, '\0');
    for (auto it = bytes.end() - length; it != bytes.end(); ++it) result += static_cast<char>(*it);
    return result;
}

QByteArray randomBytes(int count) {
    QByteArray bytes(count, '\0');
    randombytes(ptr(bytes), static_cast<unsigned long long>(count));
    return bytes;
}

void erase(QByteArray& bytes) {
    volatile char* p = bytes.data();
    for (qsizetype i = 0; i < bytes.size(); ++i) p[i] = 0;
    bytes.clear();
}

Connection::~Connection() { reset(); }
void Connection::reset() {
    erase(secret_);
    erase(session_);
    address_.clear();
    state_.clear();
}

QUrl Connection::begin() {
    reset();
    secret_ = randomBytes(crypto_box_SECRETKEYBYTES);
    QByteArray publicKey(crypto_box_PUBLICKEYBYTES, '\0');
    crypto_scalarmult_base(ptr(publicKey), ptr(secret_));
    state_ = base58(randomBytes(24));
    QUrl callback(QStringLiteral("ticketdemo://phantom/connect"));
    QUrlQuery callbackQuery;
    callbackQuery.addQueryItem(QStringLiteral("state"), state_);
    callback.setQuery(callbackQuery);
    QUrl url(QStringLiteral("https://phantom.com/ul/v1/connect"));
    QUrlQuery query;
    // This demo has no website. Use its own URI identity, not someone else's domain.
    query.addQueryItem(QStringLiteral("app_url"), QStringLiteral("ticketdemo://app"));
    query.addQueryItem(QStringLiteral("cluster"), QStringLiteral("devnet"));
    query.addQueryItem(QStringLiteral("dapp_encryption_public_key"), base58(publicKey));
    query.addQueryItem(QStringLiteral("redirect_link"), callback.toString(QUrl::FullyEncoded));
    url.setQuery(query);
    return url;
}

Connection::Reply Connection::accept(const QUrl& url) {
    if (!pending() || url.scheme() != QStringLiteral("ticketdemo") ||
        url.host() != QStringLiteral("phantom") || url.path() != QStringLiteral("/connect") ||
        !url.userInfo().isEmpty() || url.port() != -1 || url.hasFragment() ||
        url.toEncoded().size() > 16384) return Reply::Ignored;
    const QUrlQuery query(url);
    const auto states = query.allQueryItemValues(QStringLiteral("state"), QUrl::FullyDecoded);
    if (states.size() != 1 || states.first() != state_) return Reply::Ignored;
    auto fail = [this](Reply result) { reset(); return result; };
    // Reject ambiguous callbacks instead of choosing one of multiple values.
    for (const auto& name : {"errorCode", "nonce", "data", "phantom_encryption_public_key"}) {
        if (query.allQueryItemValues(QLatin1String(name)).size() > 1) return fail(Reply::Invalid);
    }
    if (query.hasQueryItem(QStringLiteral("errorCode"))) return fail(Reply::Rejected);
    auto key = unbase58(query.queryItemValue(QStringLiteral("phantom_encryption_public_key")), 32);
    auto nonce = unbase58(query.queryItemValue(QStringLiteral("nonce")), 24);
    auto data = unbase58(query.queryItemValue(QStringLiteral("data")), 8192);
    if (!key || key->size() != 32 || !nonce || nonce->size() != 24 || !data || data->size() < 16)
        return fail(Reply::Invalid);
    // Reject low-order X25519 peer keys; TweetNaCl itself does not check this.
    QByteArray dh(32, '\0');
    crypto_scalarmult(ptr(dh), ptr(secret_), ptr(*key));
    const bool lowOrder = dh == QByteArray(32, '\0');
    erase(dh);
    if (lowOrder) return fail(Reply::Invalid);
    // NaCl C uses zero padding; Phantom/TweetNaCl.js use the compact box format.
    QByteArray padded(16, '\0');
    padded += *data;
    QByteArray plain(padded.size(), '\0');
    if (crypto_box_open(ptr(plain), ptr(padded), static_cast<unsigned long long>(padded.size()),
                        ptr(*nonce), ptr(*key), ptr(secret_)) != 0) {
        erase(plain);
        return fail(Reply::Invalid);
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(plain.mid(32), &error);
    erase(plain);
    const auto object = document.object();
    const auto address = object.value(QStringLiteral("public_key")).toString();
    const auto publicKey = unbase58(address, 32);
    const auto session = object.value(QStringLiteral("session")).toString();
    if (error.error != QJsonParseError::NoError || !publicKey || publicKey->size() != 32 ||
        session.isEmpty() || !unbase58(session, 4096)) return fail(Reply::Invalid);
    address_ = address;
    session_ = session.toLatin1();
    state_.clear(); // A response may only complete one pending request.
    return Reply::Connected;
}
}
