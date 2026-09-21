#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ShareSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QStringList presets CONSTANT)
public:
    explicit ShareSettings(QObject *parent = nullptr);
    Q_INVOKABLE void setCustomUrl(const QString &url);
    QString serverUrl() const { return m_serverUrl; }
    void setServerUrl(const QString &url);
    QStringList presets() const { return {"share.mindarchy.xyz", "http://localhost:8080"}; }
signals:
    void serverUrlChanged();
private:
    QString m_serverUrl;
};

QString shareServerOverride(int &argc, char *argv[]);
