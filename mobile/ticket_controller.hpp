#pragma once

#include "ticket_manager.hpp"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <memory>

// One instance shared by both screens. This is an in-process demo authority.
class TicketController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString lastTicketId READ lastTicketId NOTIFY ticketIssued)
    Q_PROPERTY(QString statusText READ statusText NOTIFY resultChanged)
    Q_PROPERTY(QString detailText READ detailText NOTIFY resultChanged)
    Q_PROPERTY(QString resultKind READ resultKind NOTIFY resultChanged)

public:
    explicit TicketController(QObject* parent = nullptr);

    // IDs cross the QML boundary as strings, preserving the full uint64 range.
    QString lastTicketId() const { return last_ticket_id_; }
    QString statusText() const { return status_text_; }
    QString detailText() const { return detail_text_; }
    QString resultKind() const { return result_kind_; }

    Q_INVOKABLE void issueTicket();
    Q_INVOKABLE void redeemTicket(const QString& ticket_id);
    Q_INVOKABLE void clearResult();
    Q_INVOKABLE void setLanguage(const QString& code);
    Q_INVOKABLE void resetSession();

signals:
    void ticketIssued();
    void resultChanged();

private:
    enum class Message { Idle, IssueFailed, InvalidId, Granted, Used, Missing, WrongEvent, Failed };
    void setResult(Message message, qulonglong ticket_id = 0);
    void refreshTexts();
    QString text(const char* ru, const char* kk, const char* en) const;

    static constexpr tickets::EventId event_id_ = 100;
    std::unique_ptr<tickets::TicketManager> manager_;
    QString language_ = QStringLiteral("ru");
    Message message_ = Message::Idle;
    qulonglong result_ticket_id_ = 0;
    QString last_ticket_id_;
    QString status_text_;
    QString detail_text_;
    QString result_kind_ = QStringLiteral("idle");
};
