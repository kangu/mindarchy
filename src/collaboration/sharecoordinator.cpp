#include "sharecoordinator.h"

#include "../documentsession.h"
#include "../engine.h"
#include "enginebridge.h"
#include "session.h"
#include "shareclient.h"
#include "sharesettings.h"
#include "sharetransport.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QUuid>

ShareCoordinator::ShareCoordinator(Engine *engine, QObject *parent)
    : ShareCoordinator(engine, nullptr, nullptr, nullptr, parent) {}

ShareCoordinator::ShareCoordinator(Engine *engine, ShareClient *client, ShareTransport *transport,
                                   ShareSettings *settings, QObject *parent)
    : QObject(parent),
      m_engine(engine),
      m_client(client ? client : new ShareClient(this)),
      m_transport(transport ? transport : new ShareTransport(this)),
      m_settings(settings ? settings : new ShareSettings(this)),
      m_deviceId(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    m_bridge = new CollaborationEngineBridge(m_engine, this);
    m_transport->setDeviceId(m_deviceId);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        if (!signedIn() || m_mapId.isEmpty()) return;
        const QByteArray bytes = m_engine->documentBytes();
        ++m_deviceCounter;
        m_pendingHash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        m_pendingChanges = bytes;
        ensureSession();
        if (m_session) m_session->queueChange(m_deviceCounter, m_pendingHash, m_pendingChanges);
        if (m_transport->connected()) {
            m_submitAttempts = 1;
            submitPayload(m_deviceCounter, m_pendingHash, m_pendingChanges);
            setShareStatus("Syncing");
        } else {
            const int queued = m_session ? m_session->pendingForSubmit().count() : 0;
            setShareStatus(QStringLiteral("Offline · %1 queued").arg(queued));
        }
    });
    connect(m_engine, &Engine::changed, this, [this] {
        if (m_bridge->applyingRemote()) return;
        if (signedIn() && !m_mapId.isEmpty()) m_debounce.start();
    });
    wireCookie();
    connect(m_client, &ShareClient::signedInChanged, this, &ShareCoordinator::handleSignedIn);
    connect(m_client, &ShareClient::loginFailed, this, [this] {
        setShareStatus("Sign in failed");
        emit signedInChanged();
    });
    connect(m_client, &ShareClient::mapCreated, this, &ShareCoordinator::handleMapCreated);
    connect(m_client, &ShareClient::inviteAccepted, this, &ShareCoordinator::handleInviteAccepted);
    connect(m_client, &ShareClient::mapStateReady, this, &ShareCoordinator::handleMapStateReady);
    connect(m_client, &ShareClient::mapsReady, this, &ShareCoordinator::handleMapsReady);
    connect(m_client, &ShareClient::error, this, [this](const QString &message) {
        setShareStatus(message);
    });
    connect(m_transport, &ShareTransport::connectedChanged, this, &ShareCoordinator::handleTransportConnected);
    connect(m_transport, &ShareTransport::joined, this, &ShareCoordinator::handleJoined);
    connect(m_transport, &ShareTransport::committed, this, &ShareCoordinator::handleCommitted);
    connect(m_transport, &ShareTransport::presence, this, &ShareCoordinator::handlePresence);
    connect(m_transport, &ShareTransport::rejected, this, &ShareCoordinator::handleRejected);

    adoptAttachedMap();
    setShareStatus(m_client->signedIn() ? "Signed in" : "Offline");
    if (!m_mapId.isEmpty() && m_client->signedIn()) {
        m_transport->setBaseUrl(m_settings->serverUrl());
        ensureSession();
        m_transport->join(m_mapId);
    }
}

QString ShareCoordinator::serverUrl() const { return m_settings->serverUrl(); }

QStringList ShareCoordinator::presets() const { return m_settings->presets(); }

bool ShareCoordinator::signedIn() const { return m_client->signedIn(); }

QString ShareCoordinator::accountName() const { return m_client->accountName(); }

QString ShareCoordinator::shareStatus() const { return m_shareStatus; }

QStringList ShareCoordinator::presence() const { return m_presence; }

QString ShareCoordinator::mapId() const { return m_mapId; }

QVariantList ShareCoordinator::sharedMaps() const { return m_client->mapSummaries(); }

void ShareCoordinator::chooseServer(const QString &url) {
    m_settings->setServerUrl(url);
    m_client->setBaseUrl(url);
    m_transport->setBaseUrl(url);
    emit serverChanged();
}

void ShareCoordinator::signIn(const QString &name, const QString &password) {
    m_client->setBaseUrl(m_settings->serverUrl());
    m_client->login(name, password);
}

void ShareCoordinator::signOut() {
    m_transport->leave();
    setShareStatus("Offline");
}

void ShareCoordinator::shareCurrentMap() {
    m_client->createMap(m_engine->documentBytes());
}

void ShareCoordinator::inviteOnMap(const QString &account, const QString &role) {
    if (m_mapId.isEmpty()) return;
    m_client->invite(m_mapId, account, role);
}

void ShareCoordinator::acceptInvite(const QString &token) {
    m_client->acceptInvite(token);
}

void ShareCoordinator::refreshSharedMaps() {
    m_client->maps();
}

void ShareCoordinator::joinSharedMap(const QString &mapId) {
    attachMapId(mapId);
    m_transport->setBaseUrl(m_settings->serverUrl());
    m_transport->join(mapId);
    m_client->fetchMapState(mapId);
}

void ShareCoordinator::disconnectSharing() {
    m_transport->leave();
    attachMapId(QString());
}

QString ShareCoordinator::deviceId() const { return m_deviceId; }

bool ShareCoordinator::attachMapId(const QString &mapId) {
    const QString path = m_engine->documentPath() + QStringLiteral(".share");
    if (mapId.isEmpty()) {
        const bool removed = QFile::remove(path);
        m_mapId.clear();
        emit mapChanged();
        return removed;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray payload = QJsonDocument(QJsonObject{{"serverUrl", m_settings->serverUrl()},
                                                         {"mapId", mapId},
                                                         {"role", m_role},
                                                         {"account", m_client->accountName()}})
                                   .toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size()) return false;
    m_mapId = mapId;
    emit mapChanged();
    return true;
}

void ShareCoordinator::adoptAttachedMap() {
    const QString path = m_engine->documentPath() + QStringLiteral(".share");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    m_mapId = object.value("mapId").toString();
    m_role = object.value("role").toString();
    emit mapChanged();
}

void ShareCoordinator::ensureSession() {
    if (m_session || m_mapId.isEmpty()) return;
    m_session = new CollaborationSession(this);
    m_session->openOffline(m_client->accountName(), DocumentSession::defaultDirectory(), m_mapId, m_role);
}

void ShareCoordinator::wireCookie() {
    connect(m_client, &ShareClient::signedInChanged, this, [this] {
        const QUrl url(m_settings->serverUrl().startsWith("http") ? m_settings->serverUrl()
                                                                  : QStringLiteral("https://") + m_settings->serverUrl());
        const auto cookies = m_client->cookieJar()->cookiesForUrl(url);
        for (const QNetworkCookie &cookie : cookies) {
            if (cookie.name() != QByteArrayLiteral("AuthSession")) continue;
            m_transport->setSessionCookie(QString::fromUtf8(cookie.value()));
            return;
        }
        m_transport->setSessionCookie(QString());
    });
}

void ShareCoordinator::handleSignedIn() {
    setShareStatus(m_client->signedIn() ? "Signed in" : "Offline");
    ensureSession();
    if (m_client->signedIn() && !m_mapId.isEmpty()) {
        m_transport->setBaseUrl(m_settings->serverUrl());
        m_transport->join(m_mapId);
        m_client->fetchMapState(m_mapId);
    }
    emit signedInChanged();
}

void ShareCoordinator::handleMapCreated(const QString &mapId) {
    m_role = "owner";
    attachMapId(mapId);
    ensureSession();
    m_transport->setBaseUrl(m_settings->serverUrl());
    m_transport->join(mapId);
}

void ShareCoordinator::handleInviteAccepted(const QString &mapId, const QString &role) {
    m_role = role;
    attachMapId(mapId);
    m_transport->setBaseUrl(m_settings->serverUrl());
    m_transport->join(mapId);
    m_client->fetchMapState(mapId);
}

void ShareCoordinator::handleMapsReady() {
    emit sharedMapsChanged();
}

void ShareCoordinator::handleMapStateReady(const QString &mapId, const QByteArray &state, quint64 seq) {
    if (mapId != m_mapId) return;
    applyRemote(state, seq, m_deviceCounter + 1);
}

void ShareCoordinator::applyRemote(const QByteArray &state, quint64 seq, quint64 nextCounter) {
    m_bridge->beginRemoteApply();
    m_engine->loadDocumentBytes(state, m_engine->documentPath());
    m_bridge->endRemoteApply();
    if (m_session) m_session->saveLocal(state, seq, nextCounter);
}

void ShareCoordinator::handleJoined(const QString &mapId, const QString &role) {
    Q_UNUSED(role);
    if (mapId != m_mapId && !m_mapId.isEmpty()) return;
    ensureSession();
    drainOutbox();
    setShareStatus("Live");
}

void ShareCoordinator::drainOutbox() {
    if (!m_session) return;
    const auto pending = m_session->pendingForSubmit();
    for (const auto &row : pending) {
        m_submitAttempts = 1;
        submitPayload(row.counter, row.hash, row.changes);
    }
}

void ShareCoordinator::submitPayload(quint64 counter, const QString &hash, const QByteArray &changes) {
    m_lastSubmitCounter = counter;
    m_lastSubmitHash = hash;
    m_lastSubmitChanges = changes;
    m_transport->submit(counter, hash, changes);
}

void ShareCoordinator::handleCommitted(quint64 seq, const QString &deviceId, quint64 counter,
                                       const QString &hash, const QByteArray &state, const QString &sender) {
    Q_UNUSED(hash);
    Q_UNUSED(sender);
    if (deviceId == m_deviceId) {
        if (m_session) m_session->clearPendingByCounter(m_mapId, counter);
        m_pendingHash.clear();
        m_pendingChanges.clear();
        m_lastSubmitCounter = 0;
        m_lastSubmitHash.clear();
        m_lastSubmitChanges.clear();
        m_submitAttempts = 0;
        setShareStatus("Live");
        return;
    }
    applyRemote(state, seq, counter + 1);
}

void ShareCoordinator::handlePresence(const QStringList &accounts) {
    m_presence = accounts;
    emit presenceChanged();
}

void ShareCoordinator::handleTransportConnected() {
    if (!m_transport->connected()) return;
    if (!m_mapId.isEmpty()) setShareStatus("Live");
}

void ShareCoordinator::handleRejected(const QString &code) {
    if (code == QStringLiteral("counter_reuse")) {
        if (m_lastSubmitCounter == 0) return;
        if (m_session) m_session->clearPendingByCounter(m_mapId, m_lastSubmitCounter);
        if (m_submitAttempts >= 2) {
            m_lastSubmitCounter = 0;
            m_lastSubmitHash.clear();
            m_lastSubmitChanges.clear();
            setShareStatus("Sync error (counter_reuse)");
            return;
        }
        ++m_submitAttempts;
        ++m_deviceCounter;
        m_pendingHash = m_lastSubmitHash;
        m_pendingChanges = m_lastSubmitChanges;
        if (m_session) m_session->queueChange(m_deviceCounter, m_pendingHash, m_pendingChanges);
        submitPayload(m_deviceCounter, m_pendingHash, m_pendingChanges);
        return;
    }
    if (code == QStringLiteral("access_revoked")) {
        if (m_session) m_session->markAccessRemoved();
        m_transport->leave();
        return;
    }
    if (code == QStringLiteral("resync_required")) {
        m_client->fetchMapState(m_mapId);
    }
}

void ShareCoordinator::setShareStatus(const QString &status) {
    if (m_shareStatus == status) return;
    m_shareStatus = status;
    emit shareStatusChanged();
}
