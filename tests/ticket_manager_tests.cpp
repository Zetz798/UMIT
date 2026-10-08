#include "ticket_manager.hpp"

#include <array>
#include <atomic>
#include <cstdlib>
#include <future>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace tickets;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void requireInvalidArgument(Function function, const std::string& message) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}

void testIssuanceAndValidation() {
    TicketManager manager;
    manager.createEvent(1, "Concert");
    manager.createEvent(2, "Conference");
    std::set<TicketId> ids;
    for (int i = 0; i < 100; ++i) {
        const EventId event = (i % 2 == 0) ? 1 : 2;
        const Ticket ticket = manager.createTicket(event);
        require(ticket.event_id == event && !ticket.used, "Incorrect issued ticket fields");
        require(ids.insert(ticket.id).second, "Ticket IDs must be unique across events");
        const auto stored = manager.getTicket(ticket.id);
        require(stored && stored->id == ticket.id && stored->event_id == event && !stored->used,
                "Issued ticket must be stored");
    }

    const Ticket ticket = manager.createTicket(1);
    require(manager.verifyTicket(ticket.id, 1) == VerifyResult::Valid, "Fresh ticket must verify");
    require(manager.verifyTicket(ticket.id, 1) == VerifyResult::Valid, "Verification must not consume");
    auto snapshot = manager.getTicket(ticket.id);
    require(snapshot.has_value(), "Snapshot must exist");
    snapshot->id = 99999;
    snapshot->event_id = 2;
    snapshot->used = true;
    const auto unchanged = manager.getTicket(ticket.id);
    require(unchanged && unchanged->id == ticket.id && unchanged->event_id == 1 && !unchanged->used,
            "Mutating a snapshot must not change storage");

    require(manager.verifyTicket(ticket.id, 2) == VerifyResult::WrongEvent, "Wrong event must fail verification");
    require(manager.useTicket(ticket.id, 2) == UseResult::WrongEvent, "Wrong event must deny entry");
    require(manager.verifyTicket(ticket.id, 1) == VerifyResult::Valid, "Wrong event must not consume ticket");
    require(manager.useTicket(ticket.id, 1) == UseResult::Granted, "First entry must succeed");
    require(manager.verifyTicket(ticket.id, 1) == VerifyResult::AlreadyUsed, "Used ticket must fail verification");
    require(manager.useTicket(ticket.id, 1) == UseResult::AlreadyUsed, "Repeated entry must fail");
    const auto consumed = manager.getTicket(ticket.id);
    require(consumed && consumed->used, "Successful entry must persist used state");

    TicketId unknown = 0;
    while (manager.getTicket(unknown)) {
        ++unknown;
    }
    require(!manager.getTicket(unknown), "Unknown ticket must have no snapshot");
    require(manager.verifyTicket(unknown, 1) == VerifyResult::NotFound, "Unknown ticket must fail verification");
    require(manager.useTicket(unknown, 1) == UseResult::NotFound, "Unknown ticket must deny entry");
}

void testInvalidInputsAndDuplicateEvent() {
    TicketManager manager;
    requireInvalidArgument([&] { manager.createEvent(0, "Invalid"); }, "Zero event ID must fail");
    requireInvalidArgument([&] { manager.createEvent(1, ""); }, "Empty event name must fail");
    requireInvalidArgument([&] { manager.createTicket(0); }, "Rejected event must not be created");
    requireInvalidArgument([&] { manager.createTicket(1); }, "Empty-name event must not be created");
    requireInvalidArgument([&] { manager.createTicket(123); }, "Unknown event must reject issuance");
    manager.createEvent(1, "Original");
    const Ticket unused = manager.createTicket(1);
    const Ticket used = manager.createTicket(1);
    require(manager.useTicket(used.id, 1) == UseResult::Granted, "Setup entry must succeed");
    requireInvalidArgument([&] { manager.createEvent(1, "Replacement"); }, "Duplicate event must fail");
    const auto unusedAfter = manager.getTicket(unused.id);
    const auto usedAfter = manager.getTicket(used.id);
    require(unusedAfter && unusedAfter->id == unused.id && unusedAfter->event_id == 1 && !unusedAfter->used,
            "Duplicate event must preserve unused tickets");
    require(usedAfter && usedAfter->id == used.id && usedAfter->event_id == 1 && usedAfter->used,
            "Duplicate event must preserve used tickets");
    require(manager.verifyTicket(unused.id, 1) == VerifyResult::Valid, "Existing event must remain usable");
    require(manager.createTicket(1).event_id == 1, "Existing event must still allow issuance");
}

void testConcurrentEntry() {
    TicketManager manager;
    manager.createEvent(1, "Concurrent entry");
    for (int round = 0; round < 100; ++round) {
        const Ticket ticket = manager.createTicket(1);
        std::promise<void> startPromise;
        const std::shared_future<void> start = startPromise.get_future().share();
        std::atomic<int> ready{0};
        std::array<UseResult, 2> results{UseResult::NotFound, UseResult::NotFound};
        auto enter = [&](std::size_t slot) {
            ready.fetch_add(1);
            start.wait();
            results[slot] = manager.useTicket(ticket.id, 1);
        };
        std::thread first(enter, 0);
        std::thread second(enter, 1);
        while (ready.load() != 2) {
            std::this_thread::yield();
        }
        startPromise.set_value();
        first.join();
        second.join();
        const bool firstWins = results[0] == UseResult::Granted && results[1] == UseResult::AlreadyUsed;
        const bool secondWins = results[1] == UseResult::Granted && results[0] == UseResult::AlreadyUsed;
        require(firstWins || secondWins, "Concurrent entry must grant exactly once, round " + std::to_string(round));
        require(manager.verifyTicket(ticket.id, 1) == VerifyResult::AlreadyUsed, "Race must persist used state");
    }
}
} // namespace

int main() {
    try {
        testIssuanceAndValidation();
        testInvalidInputsAndDuplicateEvent();
        testConcurrentEntry();
        std::cout << "All ticket manager tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Ticket manager test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
