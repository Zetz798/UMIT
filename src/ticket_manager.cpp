#include "ticket_manager.hpp"

#include <stdexcept>
#include <utility>

namespace tickets {

void TicketManager::createEvent(EventId id, std::string name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id == 0 || name.empty()) {
        throw std::invalid_argument("Event needs a nonzero ID and a name");
    }
    if (events_.find(id) != events_.end()) {
        throw std::invalid_argument("Event already exists");
    }
    events_.emplace(id, Event{id, std::move(name)});
}

Ticket TicketManager::createTicket(EventId event_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.find(event_id) == events_.end()) {
        throw std::invalid_argument("Event does not exist");
    }
    if (next_ticket_id_ == 0) {
        throw std::overflow_error("Local ticket IDs exhausted");
    }
    const Ticket ticket{next_ticket_id_, event_id, false};
    tickets_.emplace(ticket.id, ticket);
    ++next_ticket_id_; // Unsigned wrap reaches 0; the next call then fails.
    return ticket;
}

std::optional<Ticket> TicketManager::getTicket(TicketId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tickets_.find(id);
    if (it == tickets_.end()) {
        return std::nullopt;
    }
    return it->second;
}

VerifyResult TicketManager::checkTicketLocked(TicketId id,
                                             EventId expected_event) const {
    const auto it = tickets_.find(id);
    if (it == tickets_.end()) {
        return VerifyResult::NotFound;
    }
    if (it->second.event_id != expected_event) {
        return VerifyResult::WrongEvent;
    }
    return it->second.used ? VerifyResult::AlreadyUsed : VerifyResult::Valid;
}

VerifyResult TicketManager::verifyTicket(TicketId id,
                                        EventId expected_event) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return checkTicketLocked(id, expected_event);
}

UseResult TicketManager::useTicket(TicketId id, EventId expected_event) {
    std::lock_guard<std::mutex> lock(mutex_);
    // The lock covers BOTH the check and the write, including all early returns.
    switch (checkTicketLocked(id, expected_event)) {
    case VerifyResult::NotFound:    return UseResult::NotFound;
    case VerifyResult::WrongEvent:  return UseResult::WrongEvent;
    case VerifyResult::AlreadyUsed: return UseResult::AlreadyUsed;
    case VerifyResult::Valid:
        tickets_.at(id).used = true;
        return UseResult::Granted;
    }
    throw std::logic_error("Unexpected verification result");
}

} // namespace tickets
