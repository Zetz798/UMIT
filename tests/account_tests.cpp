#include "account_controller.hpp"
#include <QTemporaryDir>
#include <QTest>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QSignalSpy>

class AccountTests : public QObject {
    Q_OBJECT
private slots:
    void accountLifecycle() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto data = dir.filePath("accounts");
        {
            AccountController account(data);
            account.registerAccount(" Person@Example.test ", "test-password-123", "test-password-123", "User One");
            QVERIFY(account.busy());
            QTRY_VERIFY_WITH_TIMEOUT(!account.busy(), 30000);
            QVERIFY2(account.loggedIn(), qPrintable(account.errorCode()));
            QCOMPARE(account.email(), "person@example.test");
            QCOMPARE(account.displayName(), "User One");
            account.updateProfile(QString::fromUtf8("Үміт"));
            QCOMPARE(account.displayName(), QString::fromUtf8("Үміт"));
            const auto files = QDir(data).entryList({"*.json"}, QDir::Files);
            for (const auto& name : files) {
                QFile f(data + "/" + name); QVERIFY(f.open(QIODevice::ReadOnly));
                QVERIFY(!f.readAll().contains("test-password-123"));
            }
            QImage avatar(100, 200, QImage::Format_RGB32); avatar.fill(Qt::green);
            QVERIFY(avatar.save(dir.filePath("source.png")));
            account.setAvatar(QUrl::fromLocalFile(dir.filePath("source.png")));
            QTRY_VERIFY_WITH_TIMEOUT(!account.busy(), 30000);
            QVERIFY2(!account.avatarUrl().isEmpty(), qPrintable(account.errorCode()));
            QVERIFY(QFile::remove(dir.filePath("source.png")));
            QVERIFY(QFile::exists(account.avatarUrl().toLocalFile()));
            account.logout(); QVERIFY(!account.loggedIn()); QVERIFY(account.avatarUrl().isEmpty());
        }
        AccountController account(data);
        QVERIFY(!account.loggedIn());
        account.login("person@example.test", "wrong-password");
        QTRY_VERIFY_WITH_TIMEOUT(!account.busy(), 30000);
        QVERIFY(!account.loggedIn()); QCOMPARE(account.errorCode(), "invalid_credentials");
        account.login("PERSON@example.test", "test-password-123");
        QTRY_VERIFY_WITH_TIMEOUT(!account.busy(), 30000);
        QVERIFY(account.loggedIn()); QCOMPARE(account.displayName(), QString::fromUtf8("Үміт"));
        const auto oldAvatar = account.avatarUrl();
        QVERIFY(QFile::exists(oldAvatar.toLocalFile()));
        account.removeAvatar(); QVERIFY(account.avatarUrl().isEmpty());
        QVERIFY(!QFile::exists(oldAvatar.toLocalFile()));
        account.logout();
        account.registerAccount("person@example.test", "test-password-123", "test-password-123", "Copy");
        QTRY_VERIFY_WITH_TIMEOUT(!account.busy(), 30000);
        QCOMPARE(account.errorCode(), "account_exists");
    }
    void inputAndSessionGuards() {
        QTemporaryDir dir; AccountController account(dir.path());
        account.registerAccount("invalid", "12345678", "12345678", "Test");
        QCOMPARE(account.errorCode(), "email_invalid");
        account.registerAccount("test@example.test", "12345678", "different", "Test");
        QCOMPARE(account.errorCode(), "password_mismatch");
        account.updateProfile("Not signed in"); QCOMPARE(account.errorCode(), "login_required");
        account.setAppLocked(true);
        account.registerAccount("test@example.test", "12345678", "12345678", "Test");
        QCOMPARE(account.errorCode(), "app_locked");
        QVERIFY(!account.busy());
        account.setAppLocked(false);
        account.registerAccount("test@example.test", "12345678", "12345678", "Test");
        account.logout(); // A late worker must not sign the user back in.
        QTRY_VERIFY_WITH_TIMEOUT(!account.busy(), 30000);
        QVERIFY(!account.loggedIn());
        account.login("test@example.test", "12345678");
        QTRY_VERIFY_WITH_TIMEOUT(!account.busy(), 30000);
        QVERIFY(account.loggedIn());
        account.setAvatar(QUrl("https://example.test/photo.png"));
        QCOMPARE(account.errorCode(), "avatar_invalid");
    }
    void throttlesAcrossRestart() {
        QTemporaryDir dir;
        {
            AccountController account(dir.path());
            for (int i = 0; i < 5; ++i) {
                account.login("missing@example.test", "wrong-pass");
                QTRY_VERIFY_WITH_TIMEOUT(!account.busy(), 30000);
            }
            QCOMPARE(account.errorCode(), "locked_out");
        }
        AccountController restored(dir.path());
        restored.login("missing@example.test", "wrong-pass");
        QTRY_VERIFY_WITH_TIMEOUT(!restored.busy(), 30000);
        QCOMPARE(restored.errorCode(), "locked_out");
    }
};
QTEST_GUILESS_MAIN(AccountTests)
#include "account_tests.moc"
