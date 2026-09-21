#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QTimer>
#include <QWebSocket>

class ShareTransport : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY connectedChanged)
public:
    explicit ShareTransport(QObject *parent = nullptr);

    Q_INVOKABLE virtual void setBaseUrl(const QString &url);
    Q_INVOKABLE void setSessionCookie(const QString &value);
    Q_INVOKABLE virtual void join(const QString &mapId);
    Q_INVOKABLE virtual void leave();
    Q_INVOKABLE virtual void submit(quint64 counter, const QString &hash, const QByteArray &changes);
    Q_INVOKABLE void setDeviceId(const QString &id);

    virtual bool connected() const;
    QString errorCode() const;
    QString deviceId() const;

    static QByteArray encodeSubmit(const QString &mapId, const QString &deviceId, quint64 counter,
                                   const QString &hash, const QByteArray &changes);

signals:
    void connectedChanged();
    void joined(QString mapId, QString role);
    void committed(quint64 seq, QString deviceId, quint64 counter, QString hash, QByteArray state, QString sender);
    void presence(QStringList accounts);
    void rejected(QString code);

private:
    QUrl liveUrl(const QString &mapId) const;
    void openSocket(const QString &mapId);
    void handleConnected();
    void handleMessage(const QString &message);
    void handlePresence(const QJsonObject &object);
    void scheduleReconnect();
    void resetBackoff();

    QString m_baseUrl;
    QString m_sessionCookie;
    QString m_deviceId;
    QString m_mapId;
    QString m_errorCode;
    bool m_connected = false;
    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    QTimer m_pingTimer;
    int m_backoffMs = 1000;
    QStringList m_accounts;
};
