#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQmlContext>
#include "wallet/wallet_controller.hpp"
#include "app_settings.hpp"
#include "account_controller.hpp"
#include "app_info.hpp"
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QImageReader>
#ifdef UMIT_UI_VERIFY
#include <QTemporaryDir>
#include <QFontDatabase>
#include <cstdio>
void startUiVerification(QObject*, AppSettings*, AccountController*, const QString&);
#endif

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("UMIT"));
    QGuiApplication::setOrganizationName(QStringLiteral("UMIT"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.3"));
    QQuickStyle::setStyle(QStringLiteral("Material"));
    QImageReader::setAllocationLimit(64);

    WalletController wallet;
    auto storage = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#ifdef UMIT_UI_VERIFY
    QTemporaryDir testDirectory;
    const auto captureDirectory = qEnvironmentVariable("UMIT_UI_VERIFY_DIR");
    if (!captureDirectory.isEmpty()) storage = testDirectory.path();
    if (!captureDirectory.isEmpty()) {
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message) {
            std::fprintf(stderr, "%s\n", message.toUtf8().constData());
            std::fflush(stderr);
        });
        const int fontId = QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        const auto families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) app.setFont(QFont(families.first()));
    }
#endif
    QDir().mkpath(storage);
    AppSettings settings(storage + "/settings.ini");
    AccountController account(storage + "/accounts");
    AppInfo info;
    wallet.setLanguage(settings.language());
    QObject::connect(&settings, &AppSettings::languageChanged, &wallet, [&] { wallet.setLanguage(settings.language()); });
    QObject::connect(&settings, &AppSettings::lockStateChanged, &account, [&] { account.setAppLocked(settings.locked()); });
    account.setAppLocked(settings.locked());
    QObject::connect(&account, &AccountController::sessionChanged, &wallet, &WalletController::resetConnection);
    // A short trip to Phantom or the image picker keeps the session. Longer background
    // periods lock it. Cold launches always start locked when a password is configured.
    QTimer privacyTimer;
    privacyTimer.setSingleShot(true);
    privacyTimer.setInterval(30000);
    qint64 backgroundDeadline = 0;
    QObject::connect(&privacyTimer, &QTimer::timeout, &settings, &AppSettings::lockNow);
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &settings, [&](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            // A suspended process may never deliver the timer before activation.
            if (backgroundDeadline && QDateTime::currentMSecsSinceEpoch() >= backgroundDeadline) settings.lockNow();
            backgroundDeadline = 0;
            privacyTimer.stop();
        } else if ((state == Qt::ApplicationHidden || state == Qt::ApplicationSuspended) && !backgroundDeadline) {
            backgroundDeadline = QDateTime::currentMSecsSinceEpoch() + 30000;
            privacyTimer.start();
        }
    });
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("wallet"), &wallet);
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("account"), &account);
    engine.rootContext()->setContextProperty(QStringLiteral("appInfo"), &info);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("TicketApp", "Main");
#ifdef UMIT_UI_VERIFY
    if (!captureDirectory.isEmpty() && !engine.rootObjects().isEmpty())
        startUiVerification(engine.rootObjects().first(), &settings, &account, captureDirectory);
#endif
    return app.exec();
}
