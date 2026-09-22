#include "sharecoordinator.h"

#include "../documentsession.h"
#include "../engine.h"
#include "enginebridge.h"
#include "session.h"
#include "liveoperations.h"
#include <QJsonArray>
#include <QSettings>
#include <QSaveFile>
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
#include <QGuiApplication>

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
        ensureSession();
        if (m_live) { captureLiveChanges(); return; }
        const QByteArray bytes = m_engine->documentBytes();
        const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        if (hash == m_lastQueuedHash) return;
        ++m_deviceCounter;
        m_pendingHash = hash;
        m_pendingChanges = bytes;
        ensureSession();
        if (!m_session || !m_session->queueChange(m_deviceCounter, m_pendingHash, m_pendingChanges)) {
            setShareStatus("Could not save changes for synchronization");
            emit operationFailed(shareStatus());
            return;
        }
        m_lastQueuedHash = m_pendingHash;
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
        if (signedIn() && !m_mapId.isEmpty()) { if(m_live) captureLiveChanges(); else m_debounce.start(); }
    });
    connect(m_engine, &Engine::documentOpening, this, [this] {
        if (m_bridge->applyingRemote()) return;
        // Switching local documents leaves the room, but must not delete the
        // previous document's saved sharing attachment.
        m_debounce.stop();
        m_transport->leave();
        if (m_session) { delete m_session; m_session = nullptr; }
        m_mapId.clear(); m_role.clear(); m_invitationCode.clear(); m_presence.clear();
        m_live = false; m_epoch.clear(); m_revision = 0;
        m_baseline = {}; m_visible = {}; m_applied.clear(); m_deferredHello = {};
        m_captureFailed = false; m_legacyRecoveryPath.clear(); m_lastQueuedHash.clear();
        m_pendingChanges.clear(); m_pendingHash.clear();
        m_lastSubmitChanges.clear(); m_lastSubmitHash.clear();
        m_lastSubmitCounter = 0; m_submitAttempts = 0;
        emit mapChanged(); emit invitationCodeChanged(); emit presenceChanged();
        setShareStatus(signedIn() ? "Online" : "Offline");
    });
    connect(m_engine, &Engine::documentOpened, this, [this] {
        if (m_bridge->applyingRemote()) return;
        adoptAttachedMap();
        if (m_mapId.isEmpty()) return;
        if (signedIn()) {
            ensureSession();
            m_transport->setBaseUrl(serverUrl());
            m_transport->join(m_mapId);
            m_client->fetchMapState(m_mapId);
        } else {
            setShareStatus("Shared · offline");
        }
    });
    connect(m_engine, &Engine::documentSaved, this, [this] {
        if (!m_mapId.isEmpty() && !attachMapId(m_mapId))
            emit operationFailed("The map was saved, but its sharing link could not be saved beside it. You can reopen the shared map from Shared with me.");
    });
    wireCookie();
    connect(m_client, &ShareClient::rememberedLoginChanged, this, &ShareCoordinator::rememberedLoginChanged);
    connect(m_client, &ShareClient::reconnectingChanged, this, &ShareCoordinator::reconnectingChanged);
    connect(m_client, &ShareClient::sessionMessage, this, [this](const QString &message) {
        // Background authentication has no pending dialog action. Keep its
        // progress in the live status, which successful sign-in replaces.
        setShareStatus(message);
    });
    connect(m_client, &ShareClient::sessionRenewed, this, [this] {
        if (!m_mapId.isEmpty()) {
            m_transport->leave();
            m_transport->join(m_mapId);
        } else setShareStatus("Online");
    });
    connect(m_client, &ShareClient::signedInChanged, this, &ShareCoordinator::handleSignedIn);
    connect(m_client, &ShareClient::loginFailed, this, [this] {
        setShareStatus("Sign in failed");
        emit operationFailed("Could not sign in. Check your account and password.");
        emit signedInChanged();
    });
    connect(m_client, &ShareClient::invitationReady, this, [this](const QString &code) {
        m_invitationCode = code;
        emit invitationCodeChanged();
    });
    connect(m_client, &ShareClient::inviteSent, this, [this] { emit operationSucceeded("invite"); });
    connect(m_client, &ShareClient::mapCreated, this, &ShareCoordinator::handleMapCreated);
    connect(m_client, &ShareClient::inviteAccepted, this, &ShareCoordinator::handleInviteAccepted);
    connect(m_client, &ShareClient::mapStateReady, this, &ShareCoordinator::handleMapStateReady);
    connect(m_client, &ShareClient::mapsReady, this, &ShareCoordinator::handleMapsReady);
    connect(m_client, &ShareClient::error, this, [this](const QString &message) {
        setShareStatus(message);
        emit operationFailed(message);
    });
    connect(m_transport, &ShareTransport::connectedChanged, this, &ShareCoordinator::handleTransportConnected);
    connect(m_transport, &ShareTransport::joined, this, &ShareCoordinator::handleJoined);
    connect(m_transport, &ShareTransport::liveMessage, this, &ShareCoordinator::handleLiveMessage);
    connect(m_transport, &ShareTransport::committed, this, &ShareCoordinator::handleCommitted);
    connect(m_transport, &ShareTransport::presence, this, &ShareCoordinator::handlePresence);
    connect(m_transport, &ShareTransport::rejected, this, &ShareCoordinator::handleRejected);

    // Headless rendering/tests never access a user's credential vault.
    if (!client && QGuiApplication::platformName() != "offscreen") {
        m_client->enableRememberedLogin();
        m_client->setBaseUrl(m_settings->serverUrl());
        QTimer::singleShot(0, m_client, [client = m_client] { client->restoreLogin(); });
    }
    adoptAttachedMap();
    setShareStatus(m_client->signedIn() ? "Signed in" : "Offline");
    if (!m_mapId.isEmpty() && m_client->signedIn()) {
        m_transport->setBaseUrl(m_settings->serverUrl());
        ensureSession();
        m_transport->join(m_mapId);
        m_client->fetchMapState(m_mapId);
    }
}

QString ShareCoordinator::serverUrl() const { return m_settings->serverUrl(); }

QStringList ShareCoordinator::presets() const { return m_settings->presets(); }

bool ShareCoordinator::rememberedLogin() const { return m_client->rememberedLogin(); }

bool ShareCoordinator::reconnecting() const { return m_client->reconnecting(); }

bool ShareCoordinator::signedIn() const { return m_client->signedIn(); }

QString ShareCoordinator::accountName() const { return m_client->accountName(); }

QString ShareCoordinator::shareStatus() const { return m_shareStatus; }

QStringList ShareCoordinator::presence() const { return m_presence; }

QString ShareCoordinator::mapId() const { return m_mapId; }

QVariantList ShareCoordinator::sharedMaps() const { return m_client->mapSummaries(); }

void ShareCoordinator::chooseServer(const QString &url) {
    if (url != serverUrl()) { signOut(); disconnectSharing(); }
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
    m_lastQueuedHash.clear();
    m_debounce.stop();
    m_transport->leave();
    m_client->signOut();
    m_invitationCode.clear();
    emit invitationCodeChanged();
    setShareStatus("Offline");
}

void ShareCoordinator::shareCurrentMap() {
    if (!m_mapId.isEmpty()) { emit operationSucceeded("share"); return; }
    m_client->createMap(m_engine->documentBytes(), m_engine->documentName());
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
    ensureSession();
    m_transport->setBaseUrl(m_settings->serverUrl());
    m_transport->join(mapId);
    m_client->fetchMapState(mapId);
}

void ShareCoordinator::disconnectSharing() {
    m_debounce.stop();
    if (m_session) { m_session->deleteLater(); m_session = nullptr; }
    m_presence.clear();
    emit presenceChanged();
    setShareStatus(signedIn() ? "Signed in" : "Offline");
    m_invitationCode.clear();
    emit invitationCodeChanged();
    m_transport->leave();
    attachMapId(QString());
}

QString ShareCoordinator::deviceId() const { return m_deviceId; }

bool ShareCoordinator::attachMapId(const QString &mapId) {
    if (mapId != m_mapId) {
        if(m_live)captureLiveChanges();
        m_captureFailed = false; m_deferredHello = {}; m_legacyRecoveryPath.clear(); m_lastQueuedHash.clear(); m_live=false; m_epoch.clear(); m_visible={}; m_baseline={}; m_applied.clear();
        if(m_session){delete m_session;m_session=nullptr;}
    }
    // The server attachment is valid even when a local sidecar cannot be saved.
    // Publish it first so invitations and the outbox are not gated on file permissions.
    m_mapId = mapId;
    emit mapChanged();
    if (m_engine->documentPath().isEmpty()) return true;
    const QString path = m_engine->documentPath() + QStringLiteral(".share");
    if (mapId.isEmpty()) {
        const bool removed = QFile::remove(path);
        m_mapId.clear();
        emit mapChanged();
        return removed;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray payload = QJsonDocument(QJsonObject{{"serverUrl", m_settings->serverUrl()},
                                                         {"mapId", mapId},
                                                         {"role", m_role},
                                                         {"account", m_client->accountName()}})
                                   .toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size()) return false;
    return file.commit();
}

void ShareCoordinator::adoptAttachedMap() {
    if (m_engine->documentPath().isEmpty()) return;
    const QString path = m_engine->documentPath() + QStringLiteral(".share");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    if (object.value("mapId").toString().isEmpty()) return;
    const QString savedServer = object.value("serverUrl").toString();
    if (!savedServer.isEmpty() && savedServer != serverUrl()) {
        // Select the saved server without erasing credentials for the old one.
        m_settings->setServerUrl(savedServer);
        m_client->setBaseUrl(savedServer);
        m_transport->setBaseUrl(savedServer);
        emit serverChanged();
        QTimer::singleShot(0, m_client, [client = m_client] { client->restoreLogin(); });
    }
    m_mapId = object.value("mapId").toString();
    m_role = object.value("role").toString();
    emit mapChanged();
}

void ShareCoordinator::ensureSession() {
    if (m_session || m_mapId.isEmpty()) return;
    m_session = new CollaborationSession(this);
    const QString scope=QString::fromLatin1(QCryptographicHash::hash((serverUrl()+"\n"+m_client->accountName()).toUtf8(),QCryptographicHash::Sha256).toHex());
    if(!m_session->openOffline(scope, DocumentSession::defaultDirectory(), m_mapId, m_role)) {delete m_session;m_session=nullptr;return;}
    m_deviceCounter=m_session->nextCounter()-1;
    QSettings settings;const QString key="collaboration/devices/"+scope+"/"+m_mapId;
    m_deviceId=settings.value(key).toString();if(m_deviceId.isEmpty()){m_deviceId=QUuid::createUuid().toString(QUuid::WithoutBraces);settings.setValue(key,m_deviceId);settings.sync();}
    m_transport->setDeviceId(m_deviceId);
    const auto saved=QJsonDocument::fromJson(m_session->accepted()).object();
    if(saved["liveProtocol"].toInt()==2){m_live=true;m_baseline=saved["canonical"].toObject();}
    m_visible=LiveOperations::normalize(m_engine->documentBytes());
    recoverLegacySnapshots();
}

void ShareCoordinator::wireCookie() {
    const auto updateCookie = [this] {
        const QUrl url(m_settings->serverUrl().startsWith("http") ? m_settings->serverUrl()
                                                                  : QStringLiteral("https://") + m_settings->serverUrl());
        const auto cookies = m_client->cookieJar()->cookiesForUrl(url);
        for (const QNetworkCookie &cookie : cookies) {
            if (cookie.name() != QByteArrayLiteral("AuthSession")) continue;
            m_transport->setSessionCookie(QString::fromUtf8(cookie.value()));
            return;
        }
        m_transport->setSessionCookie(QString());
    };
    connect(m_client, &ShareClient::signedInChanged, this, updateCookie);
    connect(m_client, &ShareClient::sessionRenewed, this, updateCookie);
}

void ShareCoordinator::handleSignedIn() {
    if (signedIn()) emit operationSucceeded("signin");
    setShareStatus(m_client->signedIn() ? "Online" : "Offline");
    if (!signedIn()) {
        m_debounce.stop();
        m_transport->leave();
        if (m_session) { m_session->deleteLater(); m_session = nullptr; }
        m_invitationCode.clear();
        emit invitationCodeChanged();
        m_presence.clear();
        emit presenceChanged();
    } else {
        ensureSession();
        m_client->maps();
    }
    if (m_client->signedIn() && !m_mapId.isEmpty()) {
        m_transport->setBaseUrl(m_settings->serverUrl());
        m_transport->join(m_mapId);
        m_client->fetchMapState(m_mapId);
    }
    emit signedInChanged();
}

void ShareCoordinator::handleMapCreated(const QString &mapId) {
    if (mapId.isEmpty()) {
        emit operationFailed("The server did not return a shared map ID. Please try again.");
        return;
    }
    m_role = "owner";
    const bool persisted = attachMapId(mapId);
    ensureSession();
    m_transport->setBaseUrl(m_settings->serverUrl());
    m_transport->join(mapId);
    emit operationSucceeded("share");
    if (!persisted)
        emit operationFailed("Your map is shared and you can invite people. Its local sharing link could not be saved; reopen it from Shared with me if needed.");
}

void ShareCoordinator::handleInviteAccepted(const QString &mapId, const QString &role) {
    emit operationSucceeded("join");
    m_role = role;
    attachMapId(mapId);
    ensureSession();
    m_transport->setBaseUrl(m_settings->serverUrl());
    m_transport->join(mapId);
    m_client->fetchMapState(mapId);
}

void ShareCoordinator::handleMapsReady() {
    emit sharedMapsChanged();
}

void ShareCoordinator::handleMapStateReady(const QString &mapId, const QByteArray &state, quint64 seq) {
    if (mapId != m_mapId || m_live) return;
    if (m_transport->protocol() == 0 && (m_debounce.isActive() || (!m_visible.isEmpty() && m_visible != LiveOperations::normalize(m_engine->documentBytes())))) return;
    applyRemote(state, seq, m_deviceCounter + 1);
}

void ShareCoordinator::applyRemote(const QByteArray &state, quint64 seq, quint64 nextCounter) {
    m_bridge->beginRemoteApply();
    m_engine->loadDocumentBytes(state, m_engine->documentPath());
    m_bridge->endRemoteApply();
    m_visible = LiveOperations::normalize(m_engine->documentBytes());
    m_lastQueuedHash = QString::fromLatin1(QCryptographicHash::hash(m_engine->documentBytes(), QCryptographicHash::Sha256).toHex());
    if (m_session) m_session->saveLocal(state, seq, nextCounter);
}

void ShareCoordinator::handleJoined(const QString &mapId, const QString &role) {
    if (mapId != m_mapId && !m_mapId.isEmpty()) return;
    m_role = role;
    emit mapChanged();
    ensureSession();
    if(m_live && m_transport->protocol()==1){setShareStatus("This map requires a Live v2 server");return;}
    drainOutbox();
    setShareStatus(m_live ? liveStatus() : "Live");
}

void ShareCoordinator::drainOutbox() {
    if (!m_session) return;
    const auto pending = m_session->pendingForSubmit();
    for (const auto &row : pending) {
        const auto payload=QJsonDocument::fromJson(row.changes).object();
        if(payload["version"].toInt()==2 && payload.contains("ops")) {if(m_live)m_transport->submitEdit(row.changes,row.hash);continue;}
        if(m_live)continue; // Preserve legacy snapshots without interpreting them as operations.
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
    if(m_live)return;
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
    if (!m_mapId.isEmpty()) setShareStatus("Connecting");
}

void ShareCoordinator::handleRejected(const QString &code) {
    if(m_live) {
        if(code=="access_revoked") {if(m_session)m_session->markAccessRemoved();m_transport->leave();setShareStatus("Access removed · local changes retained");return;}
        if(code=="storage_unavailable" || code=="quota_exceeded" || code=="invalid_message" || code=="upgrade_required") {setShareStatus("Changes saved locally · "+code);return;}
    }
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
        if (!m_session || !m_session->queueChange(m_deviceCounter, m_pendingHash, m_pendingChanges)) {
            setShareStatus("Could not save changes for synchronization");
            emit operationFailed(shareStatus());
            return;
        }
        m_lastQueuedHash = m_pendingHash;
        submitPayload(m_deviceCounter, m_pendingHash, m_pendingChanges);
        return;
    }
    if (code == QStringLiteral("access_revoked")) {
        if (m_session) m_session->markAccessRemoved();
        m_transport->leave();
        return;
    }
    if (code == QStringLiteral("resync_required")) {
        if(m_live){m_transport->leave();m_transport->join(m_mapId);return;}
        m_client->fetchMapState(m_mapId);
    }
}

void ShareCoordinator::setShareStatus(const QString &status) {
    if (m_shareStatus == status) return;
    m_shareStatus = status;
    emit shareStatusChanged();
}

bool ShareCoordinator::captureLiveChanges() {
    if (!m_live || !m_session)
        return false;
    const auto current = LiveOperations::normalize(m_engine->documentBytes());
    const auto operations = LiveOperations::diff(m_visible, current);
    if (operations.isEmpty()) {
        m_captureFailed = false;
        return true;
    }
    const QByteArray payload = QJsonDocument(QJsonObject{{"version", 2},
                                                         {"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                                         {"ops", operations}})
                                   .toJson(QJsonDocument::Compact);
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
    if (!m_session->queueChange(++m_deviceCounter, hash, payload)) {
        m_captureFailed = true;
        setShareStatus("Could not save changes for synchronization");
        return false;
    }
    m_captureFailed = false;
    m_visible = current;
    m_transport->submitEdit(payload, hash);
    setShareStatus(m_transport->connected() ? liveStatus() : "Offline · changes saved locally");
    return true;
}
void ShareCoordinator::handleLiveMessage(const QJsonObject &message) {
    const QString type = message["type"].toString();
    const QString epoch = message["epoch"].toString();
    const quint64 revision = message["revision"].toInteger();
    ensureSession();
    if (!m_session)
        return;
    m_live = true;
    // A live state may only replace the editor after every local edit is durable
    // in the outbox. Leave the baseline and revision untouched on storage failure.
    if (!captureLiveChanges()) {
        if (type == "hello") m_deferredHello = message;
        return;
    }
    // Storage may recover on a later applied/durable frame. Retain the original
    // handshake so that recovery does not depend on the server sending it twice.
    // Adopt only its negotiation metadata; the current frame supplies newer state.
    if (type != "hello" && !m_deferredHello.isEmpty()) {
        m_epoch = m_deferredHello["epoch"].toString();
        m_revision = m_deferredHello["revision"].toInteger();
        m_applied.clear();
        for (auto id : m_deferredHello["appliedIds"].toArray())
            m_applied.insert(id.toString());
    }
    m_deferredHello = {};
    if (type != "hello" && (epoch != m_epoch || revision < m_revision)) {
        // Historical retry acknowledgements still prove durability, but never replace current state.
        if (type == "durable")
            for (auto v : message["receipts"].toArray()) {
                auto r = v.toObject();
                if (r["account"].toString() == accountName())
                    m_session->clearOperation(r["id"].toString(), r["hash"].toString());
            }
        setShareStatus(liveStatus());
        return;
    }
    m_debounce.stop();
    if (type == "hello") recoverLegacySnapshots();
    if (type == "hello") {
        m_epoch = epoch;
        m_applied.clear();
        for (auto id : message["appliedIds"].toArray())
            m_applied.insert(id.toString());
    }
    if (type == "applied" && message["accountId"].toString() == accountName())
        m_applied.insert(message["opId"].toString());
    if (type == "durable")
        for (auto v : message["receipts"].toArray()) {
            auto r = v.toObject();
            if (r["account"].toString() == accountName())
                m_session->clearOperation(r["id"].toString(), r["hash"].toString());
        }
    const auto state = QJsonDocument::fromJson(QByteArray::fromBase64(message["state"].toString().toLatin1())).object();
    if (!state.contains("nodes") || !state.contains("props"))
        return;
    m_baseline = state;
    m_revision = revision;
    auto overlay = m_baseline;
    for (const auto &row : m_session->pendingForSubmit()) {
        auto p = QJsonDocument::fromJson(row.changes).object();
        if (p["version"].toInt() != 2 || !p.contains("ops") || m_applied.contains(p["id"].toString()))
            continue;
        QString error;
        if (!LiveOperations::apply(overlay, p["ops"].toArray(), &error)) {
            setShareStatus("Synchronization needs attention: " + error);
            return;
        }
    }
    const auto bytes = LiveOperations::project(overlay);
    m_bridge->beginRemoteApply();
    const auto remoteOperations = LiveOperations::diff(m_visible, overlay);
    bool loaded = m_engine->applyRemoteDocumentBytes(bytes, [remoteOperations](const QByteArray &history) {
        auto canonical = LiveOperations::normalize(history);
        if (!LiveOperations::apply(canonical, remoteOperations))
            return QByteArray();
        return LiveOperations::project(canonical);
    });
    m_bridge->endRemoteApply();
    if (!loaded) {
        setShareStatus("Invalid shared document");
        return;
    }
    m_visible = LiveOperations::normalize(m_engine->documentBytes());
    m_session->saveLocal(
        QJsonDocument(QJsonObject{{"liveProtocol", 2}, {"canonical", m_baseline}}).toJson(QJsonDocument::Compact),
        message["seq"].toInteger(), m_deviceCounter + 1);
    setShareStatus(liveStatus());
}

QString ShareCoordinator::liveStatus() const {
    if (m_captureFailed) return "Could not save changes for synchronization";
    if (!m_legacyRecoveryPath.isEmpty())
        return "Older offline edits need recovery: " + m_legacyRecoveryPath;
    return m_session && !m_session->pendingForSubmit().isEmpty() ? "Live · saving" : "Saved";
}

void ShareCoordinator::recoverLegacySnapshots() {
    if (!m_session)
        return;
    auto rows = m_session->pendingForSubmit();
    // The previous client used account-only storage. Never submit those snapshots
    // into a new server scope: export them for explicit recovery and leave originals.
    const QString oldDatabase =
        QDir(DocumentSession::defaultDirectory()).filePath(accountName() + "/collaboration.sqlite");
    if (QFile::exists(oldDatabase)) {
        CollaborationLocalStore oldStore(accountName(), DocumentSession::defaultDirectory());
        if (oldStore.open())
            rows.append(oldStore.pending(m_mapId));
    }
    for (const auto &row : rows) {
        const auto document = QJsonDocument::fromJson(row.changes).object();
        if (!document.value("nodes").isArray())
            continue;
        const QString digest =
            QString::fromLatin1(QCryptographicHash::hash(row.changes, QCryptographicHash::Sha256).toHex());
        const QString directory = QDir(DocumentSession::defaultDirectory()).filePath("Recovered offline maps");
        if (!QDir().mkpath(directory))
            continue;
        const QString path = QDir(directory).filePath("offline-" + digest + ".omm");
        if (!QFile::exists(path)) {
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(row.changes) != row.changes.size() || !file.commit())
                continue;
        }
        m_legacyRecoveryPath = directory;
    }
}
