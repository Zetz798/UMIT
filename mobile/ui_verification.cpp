// Compiled only with UMIT_UI_VERIFY on desktop. Real controllers, isolated temporary data.
#include "app_settings.hpp"
#include "account_controller.hpp"
#include "ticket_controller.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>
#include <memory>

void startUiVerification(QObject* root, AppSettings* settings, AccountController* account, const QString& path) {
    QDir().mkpath(path);
    auto* window = qobject_cast<QQuickWindow*>(root);
    window->setWidth(320); window->setHeight(820);
    auto step = std::make_shared<int>(0);
    auto* timer = new QTimer(root); timer->setInterval(600);
    const auto require = [](bool condition, const char* reason) {
        if (!condition) { qCritical() << "UI_VERIFY_FAILED" << reason; QCoreApplication::exit(2); }
        return condition;
    };
    const auto capture = [=](const QString& name) {
        return require(window->grabWindow().save(path + "/" + name + ".png"), "capture failed");
    };
    const auto field = [=](const char* name, const QString& value) {
        auto* object = root->findChild<QObject*>(name);
        if (!require(object != nullptr, name)) return false;
        object->setProperty("text", value); return true;
    };
    QObject::connect(timer, &QTimer::timeout, root, [=] {
        if (account->busy() || settings->busy()) return;
        auto* loader = root->findChild<QObject*>("pageLoader");
        auto* page = loader ? loader->property("item").value<QObject*>() : nullptr;
        auto* tickets = root->findChild<TicketController*>("ticketController");
        switch ((*step)++) {
        case 0: capture("01-home-ru-320"); root->setProperty("currentTab", 2); break;
        case 1: if (!require(page, "profile page missing")) return; page->setProperty("registering", true); break;
        case 2: {
            capture("02-register-ru-320");
            if (!field("accountEmail", "demo@example.test") || !field("registerNickname", "Үміт") ||
                !field("accountPassword", "demo-password-123") || !field("registerConfirmation", "demo-password-123")) return;
            auto* button = root->findChild<QObject*>("authenticateButton");
            if (!require(button, "auth button missing")) return;
            QMetaObject::invokeMethod(button, "clicked"); break;
        }
        case 3:
            if (!require(account->loggedIn(), "register did not log in")) return;
            capture("03-profile-ru-320"); root->setProperty("currentTab", 1); break;
        case 4:
            if (!require(tickets, "ticket controller missing")) return;
            tickets->issueTicket(); tickets->redeemTicket(tickets->lastTicketId()); break;
        case 5: capture("04-tickets-ru-320"); root->setProperty("currentTab", 3); break;
        case 6: capture("05-settings-ru-320"); settings->setLanguage("kk"); root->setProperty("currentTab", 0); break;
        case 7: capture("06-home-kk-320"); settings->setLanguage("en"); settings->setDarkMode(true); window->setWidth(390); root->setProperty("currentTab", 2); break;
        case 8: capture("07-profile-en-dark"); root->setProperty("currentTab", 3); break;
        case 9: capture("08-settings-en-dark"); root->setProperty("currentTab", 4); break;
        case 10: capture("09-support-en-dark"); settings->setAppPassword({}, "app-pass-123", "app-pass-123"); break;
        case 11: if (!require(settings->lockEnabled(), "app password failed")) return; settings->lockNow(); break;
        case 12: {
            capture("10-locked-en-dark");
            if (!require(loader && !loader->property("active").toBool(), "content accessible when locked")) return;
            if (!field("unlockPassword", "app-pass-123")) return;
            auto* button = root->findChild<QObject*>("unlockButton");
            if (!require(button, "unlock button missing")) return;
            QMetaObject::invokeMethod(button, "clicked"); break;
        }
        case 13:
            if (!require(!settings->locked(), "UI unlock failed")) return;
            account->logout(); root->setProperty("currentTab", 1); break;
        case 14:
            if (!require(!account->loggedIn() && tickets && tickets->lastTicketId().isEmpty(), "logout leaked tickets")) return;
            capture("11-logged-out-en-dark");
            qInfo() << "UI_VERIFY_PASSED"; timer->stop(); QCoreApplication::quit(); break;
        }
    });
    QTimer::singleShot(120000, root, [] { qCritical() << "UI_VERIFY_TIMEOUT"; QCoreApplication::exit(3); });
    timer->start();
}
