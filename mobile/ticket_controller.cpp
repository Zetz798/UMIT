#include "ticket_controller.hpp"

#include <exception>
#include <utility>

TicketController::TicketController(QObject* parent) : QObject(parent) {
    resetSession();
}

QString TicketController::text(const char* ru, const char* kk, const char* en) const {
    return QString::fromUtf8(language_ == QStringLiteral("kk") ? kk :
                             language_ == QStringLiteral("en") ? en : ru);
}

void TicketController::setLanguage(const QString& code) {
    const QString normalized = code == QStringLiteral("kz") ? QStringLiteral("kk") : code;
    if (normalized != QStringLiteral("ru") && normalized != QStringLiteral("kk") &&
        normalized != QStringLiteral("en")) return;
    if (language_ == normalized) return;
    language_ = normalized;
    refreshTexts();
}

void TicketController::resetSession() {
    // Each UMIT login receives a fresh local demo; no previous account's tickets remain.
    auto manager = std::make_unique<tickets::TicketManager>();
    manager->createEvent(event_id_, "Mobile demo");
    manager_ = std::move(manager);
    last_ticket_id_.clear();
    emit ticketIssued();
    clearResult();
}

void TicketController::setResult(Message message, qulonglong ticket_id) {
    message_ = message;
    result_ticket_id_ = ticket_id;
    refreshTexts();
}

void TicketController::refreshTexts() {
    result_kind_ = QStringLiteral("error");
    switch (message_) {
    case Message::Idle:
        result_kind_ = QStringLiteral("idle");
        status_text_.clear();
        detail_text_.clear();
        break;
    case Message::IssueFailed:
        status_text_ = text("Билет не выпущен", "Билет шығарылмады", "Ticket not issued");
        detail_text_ = text("Не удалось создать билет. Попробуйте ещё раз.",
                            "Билетті жасау мүмкін болмады. Қайталап көріңіз.",
                            "Could not create the ticket. Please try again.");
        break;
    case Message::InvalidId:
        status_text_ = text("Проверьте ID", "ID нөмірін тексеріңіз", "Check the ticket ID");
        detail_text_ = text("Введите положительный числовой ID выпущенного билета.",
                            "Шығарылған билеттің нөлден үлкен сандық ID нөмірін енгізіңіз.",
                            "Enter the issued ticket's positive numeric ID.");
        break;
    case Message::Granted:
        result_kind_ = QStringLiteral("granted");
        status_text_ = text("Проход разрешён", "Кіруге рұқсат берілді", "Entry allowed");
        detail_text_ = text("Билет №%1 погашен в локальном демо.",
                            "№%1 билет жергілікті демода пайдаланылды деп белгіленді.",
                            "Ticket #%1 was redeemed in the local demo.").arg(result_ticket_id_);
        break;
    case Message::Used:
    case Message::Missing:
    case Message::WrongEvent:
        result_kind_ = QStringLiteral("denied");
        status_text_ = text("Проход запрещён", "Кіруге рұқсат жоқ", "Entry denied");
        if (message_ == Message::Used) {
            detail_text_ = text("Билет №%1 уже использован.", "№%1 билет бұрын пайдаланылған.",
                                "Ticket #%1 has already been used.").arg(result_ticket_id_);
        } else if (message_ == Message::Missing) {
            detail_text_ = text("Билет №%1 не найден в этой сессии.",
                                "№%1 билет осы сессияда табылмады.",
                                "Ticket #%1 was not found in this session.").arg(result_ticket_id_);
        } else {
            detail_text_ = text("Билет относится к другому мероприятию.",
                                "Билет басқа іс-шараға арналған.",
                                "This ticket belongs to a different event.");
        }
        break;
    case Message::Failed:
        status_text_ = text("Ошибка проверки", "Тексеру қатесі", "Verification failed");
        detail_text_ = text("Разрешение на проход не выдано.", "Кіруге рұқсат берілген жоқ.",
                            "Entry permission was not granted.");
        break;
    }
    emit resultChanged();
}

void TicketController::clearResult() {
    setResult(Message::Idle);
}

void TicketController::issueTicket() {
    clearResult();
    try {
        const auto ticket = manager_->createTicket(event_id_);
        last_ticket_id_ = QString::number(static_cast<qulonglong>(ticket.id));
        emit ticketIssued();
    } catch (const std::exception&) {
        setResult(Message::IssueFailed);
    }
}

void TicketController::redeemTicket(const QString& ticket_id) {
    // Reject signs, exponents, overflow and non-ASCII digits; never use JS Number.
    const QString value = ticket_id.trimmed();
    bool digits_only = !value.isEmpty() && value.size() <= 20;
    for (const QChar ch : value) {
        if (ch < QLatin1Char('0') || ch > QLatin1Char('9')) {
            digits_only = false;
            break;
        }
    }
    bool parsed = false;
    const auto id = value.toULongLong(&parsed, 10);
    if (!digits_only || !parsed || id == 0) {
        setResult(Message::InvalidId);
        return;
    }

    try {
        // No preview-based grant: only this atomic operation decides the result.
        switch (manager_->useTicket(static_cast<tickets::TicketId>(id), event_id_)) {
        case tickets::UseResult::Granted: setResult(Message::Granted, id); break;
        case tickets::UseResult::AlreadyUsed: setResult(Message::Used, id); break;
        case tickets::UseResult::NotFound: setResult(Message::Missing, id); break;
        case tickets::UseResult::WrongEvent: setResult(Message::WrongEvent, id); break;
        }
    } catch (const std::exception&) {
        setResult(Message::Failed);
    }
}
