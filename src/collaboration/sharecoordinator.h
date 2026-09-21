#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QByteArray>

class Engine;
class ShareClient;
class ShareTransport;
class ShareSettings;
class CollaborationEngineBridge;
class CollaborationSession;

class ShareCoordinator : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY signedInChanged)
    Q_PROPERTY(QString shareStatus READ shareStatus NOTIFY shareStatusChanged)
    Q_PROPERTY(QStringList presence READ presence NOTIFY presenceChanged)
    Q_PROPERTY(QString mapId READ mapId NOTIFY mapChanged)
public:
    explicit ShareCoordinator(Engine *engine, QObject *parent = nullptr);
    ShareCoordinator(Engine *engine, ShareClient *client, ShareTransport *transport,
                     ShareSettings *settings, QObject *parent = nullptr);

    QString serverUrl() const;
    bool signedIn() const;
    QString accountName() const;
    QString shareStatus() const;
    QStringList presence() const;
    QString mapId() const;

    Q_INVOKABLE void chooseServer(const QString &url);
    Q_INVOKABLE void signIn(const QString &name, const QString &password);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void shareCurrentMap();
    Q_INVOKABLE void inviteOnMap(const QString &account, const QString &role);
    Q_INVOKABLE void joinSharedMap(const QString &mapId);
    Q_INVOKABLE void disconnectSharing();

    QString deviceId() const;
    bool attachMapId(const QString &mapId);

signals:
    void serverChanged();
    void signedInChanged();
    void shareStatusChanged();
    void presenceChanged();
    void mapChanged();

private:
    void ensureSession();
    void handleSignedIn();
    void handleMapCreated(const QString &mapId);
    void handleInviteAccepted(const QString &mapId, const QString &role);
    void handleMapStateReady(const QString &mapId, const QByteArray &state, quint64 seq);
    void handleJoined(const QString &mapId, const QString &role);
    void handleCommitted(quint64 seq, const QString &deviceId, quint64 counter,
                         const QString &hash, const QByteArray &state, const QString &sender);
    void handlePresence(const QStringList &accounts);
    void handleTransportConnected();
    void handleRejected(const QString &code);
    void applyRemote(const QByteArray &state, quint64 seq, quint64 nextCounter);
    void drainOutbox();
    void adoptAttachedMap();
    void setShareStatus(const QString &status);

    Engine *m_engine = nullptr;
    ShareClient *m_client = nullptr;
    ShareTransport *m_transport = nullptr;
    ShareSettings *m_settings = nullptr;
    CollaborationEngineBridge *m_bridge = nullptr;
    CollaborationSession *m_session = nullptr;
    QStringList m_presence;
    QTimer m_debounce;
    QString m_mapId;
    QString m_role;
    QString m_deviceId;
    QString m_shareStatus = "Offline";
    quint64 m_deviceCounter = 0;
    QString m_pendingHash;
    QByteArray m_pendingChanges;
};
