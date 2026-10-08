#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace tickets {

using EventId = std::uint64_t;
using TicketId = std::uint64_t;

struct Event {
    EventId id;
    std::string name;
};

struct Ticket {
    TicketId id;
    EventId event_id;
    bool used = false;
};

enum class VerifyResult { Valid, NotFound, WrongEvent, AlreadyUsed };
enum class UseResult { Granted, NotFound, WrongEvent, AlreadyUsed };

// Stage 1: trusted callers, one process, no persistent storage.
class TicketManager {
public:
    void createEvent(EventId id, std::string name);
    Ticket createTicket(EventId event_id);

    // Returns a copy: callers cannot modify the manager's stored ticket.
    std::optional<Ticket> getTicket(TicketId id) const;
    // Informational snapshot only; does not reserve or consume the ticket.
    VerifyResult verifyTicket(TicketId id, EventId expected_event) const;
    // Only Granted allows entry. Check and update share the same lock.
    UseResult useTicket(TicketId id, EventId expected_event);

private:
    // Caller must hold mutex_.
    VerifyResult checkTicketLocked(TicketId id, EventId expected_event) const;

    mutable std::mutex mutex_;
    std::unordered_map<EventId, Event> events_;
    std::unordered_map<TicketId, Ticket> tickets_;
    TicketId next_ticket_id_ = 1; // Local identifier, NOT a secret.
};

} // namespace tickets
