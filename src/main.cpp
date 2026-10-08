#include "ticket_manager.hpp"

#include <array>
#include <future>
#include <iostream>
#include <thread>

namespace {

const char* message(tickets::UseResult result) {
    using tickets::UseResult;
    switch (result) {
    case UseResult::Granted:     return "ACCESS GRANTED";
    case UseResult::NotFound:    return "ACCESS DENIED | INVALID (NOT FOUND)";
    case UseResult::WrongEvent:  return "ACCESS DENIED | WRONG EVENT";
    case UseResult::AlreadyUsed: return "ACCESS DENIED | ALREADY USED";
    }
    return "ACCESS DENIED | UNKNOWN RESULT";
}

} // namespace

int main() {
    using namespace tickets;
    TicketManager manager;
    manager.createEvent(100, "Local demo");
    manager.createEvent(200, "Another event");
    const auto ticket = manager.createTicket(100);

    std::cout << "LOCAL PROTOTYPE | IN MEMORY | NO BLOCKCHAIN\n"
              << "Ticket ID: " << ticket.id << "\nEvent: " << ticket.event_id
              << "\nUsed: " << std::boolalpha << ticket.used << '\n';
    if (manager.verifyTicket(ticket.id, 100) == VerifyResult::Valid) {
        std::cout << "Preview: VALID (does not grant entry)\n";
    }
    std::cout << "First scan: " << message(manager.useTicket(ticket.id, 100)) << '\n'
              << "Second scan: " << message(manager.useTicket(ticket.id, 100)) << '\n'
              << "Unknown ID: " << message(manager.useTicket(0, 100)) << '\n';

    const auto other = manager.createTicket(100);
    std::cout << "Wrong event: " << message(manager.useTicket(other.id, 200)) << '\n'
              << "Correct event: " << message(manager.useTicket(other.id, 100)) << '\n';

    const auto race_ticket = manager.createTicket(100);
    std::promise<void> start;
    const auto ready = start.get_future().share();
    std::array<UseResult, 2> results{};
    auto scan = [&](std::size_t index) {
        ready.wait();
        results[index] = manager.useTicket(race_ticket.id, 100);
    };
    std::thread scanner_a(scan, 0);
    std::thread scanner_b(scan, 1);
    start.set_value();
    scanner_a.join();
    scanner_b.join();

    std::cout << "\nTwo scanners, same ticket (winner may change):\n"
              << "Scanner A: " << message(results[0]) << '\n'
              << "Scanner B: " << message(results[1]) << '\n';
    const int granted = static_cast<int>(results[0] == UseResult::Granted)
                      + static_cast<int>(results[1] == UseResult::Granted);
    const int already_used = static_cast<int>(results[0] == UseResult::AlreadyUsed)
                           + static_cast<int>(results[1] == UseResult::AlreadyUsed);
    std::cout << "Successful entries: " << granted << " (expected 1)\n";
    return granted == 1 && already_used == 1 ? 0 : 1;
}
