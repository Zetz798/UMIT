#include "app_settings.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
const QString secret = QStringLiteral("a strong test password");
const QString replacement = QStringLiteral("a different test password");

QJsonObject readRecord(const QString& path)
{
    QSettings settings(path, QSettings::IniFormat);
    return QJsonDocument::fromJson(settings.value("privacy/credential").toByteArray()).object();
}

void writeRecord(const QString& path, const QJsonObject& record)
{
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue("privacy/credential", QJsonDocument(record).toJson(QJsonDocument::Compact));
    settings.sync();
}
}

class AppSettingsTests : public QObject {
    Q_OBJECT
private slots:
    void preferencesPersistAndResetOnlyColors();
    void invalidInputCannotReplacePreferences();
    void passwordLifecycleAndStorage();
    void passwordValidation();
    void independentSalts();
    void lockoutSurvivesRestart();
    void malformedRecordFailsClosed();
    void lockInterruptsInFlightUnlock();
};

void AppSettingsTests::preferencesPersistAndResetOnlyColors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("settings.ini");
    {
        AppSettings settings(path);
        QVERIFY(!settings.lockEnabled());
        settings.setLanguage("kk");
        settings.setBackgroundColor(QColor("#fedcba"));
        settings.setTextColor(QColor("#112233"));
        settings.setDarkMode(true);
        QVERIFY(settings.backgroundColor() != QColor("#fedcba"));
        settings.setBackgroundColor(QColor("#202020"));
        settings.setTextColor(QColor("#eeeeee"));
    }
    AppSettings restored(path);
    QCOMPARE(restored.language(), "kk");
    QVERIFY(restored.darkMode());
    QCOMPARE(restored.backgroundColor(), QColor("#202020"));
    restored.setDarkMode(false);
    QCOMPARE(restored.backgroundColor(), QColor("#fedcba"));
    QCOMPARE(restored.textColor(), QColor("#112233"));
    restored.setDarkMode(true);
    restored.resetColors();
    QCOMPARE(restored.language(), "kk");
    QVERIFY(restored.darkMode());
    QCOMPARE(restored.backgroundColor(), QColor("#111827"));
    restored.setDarkMode(false);
    QCOMPARE(restored.backgroundColor(), QColor("#f5f7fb"));
}

void AppSettingsTests::invalidInputCannotReplacePreferences()
{
    QTemporaryDir dir;
    AppSettings settings(dir.filePath("settings.ini"));
    settings.setLanguage("xx");
    QCOMPARE(settings.language(), "ru");
    QCOMPARE(settings.errorCode(), "invalid_language");
    const auto background = settings.backgroundColor();
    settings.setBackgroundColor(QColor("transparent"));
    QCOMPARE(settings.backgroundColor(), background);
    QCOMPARE(settings.errorCode(), "invalid_color");
}

void AppSettingsTests::passwordLifecycleAndStorage()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("settings.ini");
    {
        AppSettings settings(path);
        QSignalSpy done(&settings, &AppSettings::operationFinished);
        settings.setAppPassword({}, secret, secret);
        QVERIFY(settings.busy());
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
        QVERIFY(done.takeFirst().at(0).toBool());
        QVERIFY(settings.lockEnabled());
        QVERIFY(!settings.locked());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains(secret.toUtf8()));
    }
    AppSettings settings(path);
    QVERIFY(settings.locked());
    settings.resetColors();
    QVERIFY(settings.locked());
    QVERIFY(settings.lockEnabled());
    QSignalSpy done(&settings, &AppSettings::operationFinished);
    settings.unlock("wrong valid password");
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
    QVERIFY(!done.takeFirst().at(0).toBool());
    QVERIFY(settings.locked());
    settings.unlock(secret);
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
    QVERIFY(done.takeFirst().at(0).toBool());
    QVERIFY(!settings.locked());
    settings.setAppPassword(secret, replacement, replacement);
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
    QVERIFY(done.takeFirst().at(0).toBool());
    settings.lockNow();
    settings.unlock(secret);
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
    QVERIFY(!done.takeFirst().at(0).toBool());
    settings.unlock(replacement);
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
    QVERIFY(done.takeFirst().at(0).toBool());
    settings.disableAppPassword(replacement);
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
    QVERIFY(done.takeFirst().at(0).toBool());
    QVERIFY(!settings.lockEnabled());
    AppSettings restored(path);
    QVERIFY(!restored.lockEnabled());
    QVERIFY(!restored.locked());
}

void AppSettingsTests::passwordValidation()
{
    QTemporaryDir dir;
    AppSettings settings(dir.filePath("settings.ini"));
    settings.setAppPassword({}, "short", "short");
    QCOMPARE(settings.errorCode(), "password_length");
    settings.setAppPassword({}, secret, replacement);
    QCOMPARE(settings.errorCode(), "password_mismatch");
    settings.setAppPassword({}, QString(129, 'a'), QString(129, 'a'));
    QCOMPARE(settings.errorCode(), "password_length");
    QVERIFY(!settings.lockEnabled());
    QVERIFY(!settings.busy());
}

void AppSettingsTests::independentSalts()
{
    QTemporaryDir dir;
    AppSettings one(dir.filePath("one.ini"));
    AppSettings two(dir.filePath("two.ini"));
    one.setAppPassword({}, secret, secret);
    two.setAppPassword({}, secret, secret);
    QTRY_VERIFY_WITH_TIMEOUT(!one.busy() && !two.busy(), 30000);
    const auto first = readRecord(dir.filePath("one.ini"));
    const auto second = readRecord(dir.filePath("two.ini"));
    QVERIFY(first.value("salt") != second.value("salt"));
    QVERIFY(first.value("verifier") != second.value("verifier"));
    QCOMPARE(first.value("iterations").toInt(), 600000);
}

void AppSettingsTests::lockoutSurvivesRestart()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("settings.ini");
    {
        AppSettings settings(path);
        settings.setAppPassword({}, secret, secret);
        QTRY_VERIFY_WITH_TIMEOUT(!settings.busy(), 30000);
        settings.lockNow();
        for (int attempt = 0; attempt < 5; ++attempt)
            settings.unlock("bad");
        QCOMPARE(settings.errorCode(), "lockout");
        QVERIFY(settings.lockRemainingSeconds() > 0);
    }
    {
        AppSettings restored(path);
        restored.unlock(secret);
        QVERIFY(restored.locked());
        QVERIFY(!restored.busy());
        QCOMPARE(restored.errorCode(), "lockout");
        QVERIFY(restored.lockRemainingSeconds() > 0);
    }
    auto record = readRecord(path);
    record["blockedUntil"] = QStringLiteral("1"); // An expired, persisted cooldown.
    writeRecord(path, record);
    AppSettings expired(path);
    expired.unlock(secret);
    QTRY_VERIFY_WITH_TIMEOUT(!expired.busy(), 30000);
    QVERIFY(!expired.locked());
    QCOMPARE(readRecord(path).value("failedAttempts").toInt(), 0);
}

void AppSettingsTests::malformedRecordFailsClosed()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("settings.ini");
    writeRecord(path, {{"version", 1}, {"iterations", 1}});
    AppSettings settings(path);
    QVERIFY(settings.locked());
    QVERIFY(settings.lockEnabled());
    QCOMPARE(settings.errorCode(), "credential_corrupt");
    settings.setAppPassword({}, secret, secret);
    QVERIFY(settings.locked());
    QVERIFY(!settings.busy());
    settings.disableAppPassword(secret);
    QVERIFY(settings.lockEnabled());
    settings.unlock(secret);
    QVERIFY(settings.locked());
}

void AppSettingsTests::lockInterruptsInFlightUnlock()
{
    QTemporaryDir dir;
    AppSettings settings(dir.filePath("settings.ini"));
    settings.setAppPassword({}, secret, secret);
    QTRY_VERIFY_WITH_TIMEOUT(!settings.busy(), 30000);
    settings.lockNow();
    QSignalSpy done(&settings, &AppSettings::operationFinished);
    settings.unlock(secret);
    QVERIFY(settings.busy());
    settings.unlock(secret); // Must not start a second worker.
    settings.disableAppPassword(secret);
    settings.lockNow(); // e.g. the app moves into the background during derivation.
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 30000);
    QVERIFY(!done.takeFirst().at(0).toBool());
    QVERIFY(settings.locked());
    QVERIFY(settings.lockEnabled());
    QCOMPARE(settings.errorCode(), "app_locked");
}

QTEST_GUILESS_MAIN(AppSettingsTests)
#include "app_settings_tests.moc"
