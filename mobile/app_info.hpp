#pragma once
#include <QObject>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

class AppInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString supportEmail READ supportEmail CONSTANT)
    Q_PROPERTY(QString supportPhone READ supportPhone CONSTANT)
    Q_PROPERTY(QUrl logoUrl READ logoUrl CONSTANT)
public:
    explicit AppInfo(QObject* parent = nullptr) : QObject(parent) {
        QFile config(":/config/app_config.json");
        if (config.open(QIODevice::ReadOnly)) values_ = QJsonDocument::fromJson(config.readAll()).object();
    }
    QString supportEmail() const { return values_.value("supportEmail").toString(); }
    QString supportPhone() const { return values_.value("supportPhone").toString(); }
    QUrl logoUrl() const { return QFile::exists(":/branding/logo.png") ? QUrl("qrc:/branding/logo.png") : QUrl(); }
private:
    QJsonObject values_;
};
