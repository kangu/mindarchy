#include "shareclient.h"

#include "sharesettings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QSet>

namespace {
constexpr int kTransferTimeoutMs = 5000;
QSet<ShareClient *> clients;

QString jsonString(const QString &value) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson().mid(1).chopped(2));
}
}

ShareClient::ShareClient(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)) {
    m_network->setCookieJar(new QNetworkCookieJar(m_network));
    m_network->setTransferTimeout(kTransferTimeoutMs);
    clients.insert(this);
    m_loginTimer.setSingleShot(true);
    connect(&m_loginTimer, &QTimer::timeout, this, [this] { restoreLogin(); });
}

ShareClient::~ShareClient() { clients.remove(this); }

void ShareClient::enableRememberedLogin(std::shared_ptr<ShareCredentialStore> store) {
    m_credentialStore = std::move(store);
}

bool ShareClient::restoreLogin() {
    if (!m_credentialStore || m_baseUrl.isEmpty() || m_loginInFlight) return false;
    QString error;
    const auto saved = m_credentialStore->read(m_baseUrl, &error);
    if (!error.isEmpty()) { emit sessionMessage(error); return false; }
    m_rememberedLogin = !saved.isEmpty();
    emit rememberedLoginChanged();
    if (saved.isEmpty()) return false;
    const auto object = QJsonDocument::fromJson(saved).object();
    const QString username = object.value("username").toString();
    const QString password = object.value("password").toString();
    if (username.isEmpty() || password.isEmpty()) {
        emit sessionMessage("The saved login is incomplete. Please sign in again.");
        return false;
    }
    performLogin(username, password, true);
    return true;
}

void ShareClient::setBaseUrl(const QString &url) {
    QString normalized = normalizeShareServerUrl(url);
    if (!normalized.endsWith('/')) normalized += '/';
    if (m_baseUrl == normalized) return;
    clearLocalSession();
    m_baseUrl = normalized;
    m_rememberedLogin = false;
    emit rememberedLoginChanged();
}

void ShareClient::signOut() {
    QString error;
    if (m_credentialStore && !m_baseUrl.isEmpty()) m_credentialStore->remove(m_baseUrl, &error);
    // Signing out on this device also stops other tabs from renewing this login.
    const auto peers = clients.values();
    for (auto *peer : peers) {
        if (peer == this || (m_credentialStore && peer->m_credentialStore == m_credentialStore && peer->m_baseUrl == m_baseUrl))
        {
            peer->clearLocalSession();
            peer->m_rememberedLogin = !error.isEmpty();
            emit peer->rememberedLoginChanged();
        }
    }
    if (!error.isEmpty()) emit sessionMessage(error);
}

void ShareClient::clearLocalSession() {
    m_loginTimer.stop();
    m_loginInFlight = false;
    emit reconnectingChanged();
    // Do not let an in-flight response restore credentials after signing out.
    for (auto *reply : m_network->findChildren<QNetworkReply *>()) {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    m_network->setCookieJar(new QNetworkCookieJar(m_network));
    m_network->clearAccessCache();
    m_signedIn = false;
    m_accountName.clear();
    m_mapSummaries.clear();
    m_mapStates.clear();
    emit signedInChanged();
    emit mapsReady();
}

void ShareClient::login(const QString &username, const QString &password) {
    if (m_loginInFlight) return;
    performLogin(username, password, false);
}

void ShareClient::performLogin(const QString &username, const QString &password, bool automatic) {
    m_loginTimer.stop();
    m_loginInFlight = true;
    emit reconnectingChanged();
    if (automatic && !m_signedIn) emit sessionMessage("Connecting to your sharing server…");
    QNetworkRequest request = makeRequest(QStringLiteral("v1/auth/session"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QByteArray body = QJsonDocument(QJsonObject{{"username", username}, {"password", password}}).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, username, password, automatic] {
        reply->deleteLater();
        m_loginInFlight = false;
        emit reconnectingChanged();
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (automatic && (status == 401 || status == 403)) {
                signOut();
                emit sessionMessage("Your saved login is no longer valid. Please sign in again.");
            } else if (automatic) {
                m_loginTimer.start(30000);
                emit sessionMessage("Offline · will reconnect automatically");
            } else handleHttpError(reply, QStringLiteral("login"));
            return;
        }
        const QString accountId = QJsonDocument::fromJson(reply->readAll()).object().value("accountId").toString();
        if (accountId.isEmpty()) { emit loginFailed(QStringLiteral("invalid_message")); return; }
        const bool wasSignedIn = m_signedIn;
        m_accountName = accountId;
        m_signedIn = true;
        QString storageError;
        if (m_credentialStore) {
            if (!automatic) m_credentialStore->write(m_baseUrl,
                QJsonDocument(QJsonObject{{"username", username}, {"password", password}}).toJson(QJsonDocument::Compact), &storageError);
            if (!automatic && storageError.isEmpty()) {
                m_rememberedLogin = true;
                emit rememberedLoginChanged();
            }
            // Renew the short-lived server session while this device is online.
            m_loginTimer.start(5 * 60 * 1000);
        }
        if (wasSignedIn) emit sessionRenewed();
        else emit signedInChanged();
        if (!storageError.isEmpty()) emit sessionMessage(storageError);
        if (!automatic && m_credentialStore && storageError.isEmpty()) {
            for (auto *peer : clients) {
                if (peer != this && peer->m_credentialStore == m_credentialStore && peer->m_baseUrl == m_baseUrl && !peer->m_signedIn)
                    peer->restoreLogin();
            }
        }
    });
}

void ShareClient::account() {
    QNetworkReply *reply = m_network->get(makeRequest(QStringLiteral("v1/me")));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (handleHttpError(reply, QStringLiteral("account check"))) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QString accountId = doc.object().value("accountId").toString();
        const bool changed = accountId != m_accountName || !m_signedIn;
        m_accountName = accountId;
        m_signedIn = true;
        if (changed) emit signedInChanged();
    });
}

void ShareClient::maps() {
    QNetworkReply *reply = m_network->get(makeRequest(QStringLiteral("v1/maps")));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (handleHttpError(reply, QStringLiteral("listing maps"))) return;
        const QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();
        QVariantList summaries;
        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();
            const QString mapId = object.value("ID").toString();
            if (mapId.isEmpty()) continue;
            const QByteArray snapshot = QByteArray::fromBase64(object.value("Snapshot").toString().toUtf8());
            const quint64 seq = static_cast<quint64>(object.value("Seq").toInteger(0));
            m_mapStates.insert(mapId, {snapshot, seq});
            const QString name = object.value("Name").toString();
            QVariantMap summary{{"id", mapId},
                                {"name", name.isEmpty() ? mapId : name},
                                {"owner", object.value("Owner").toString()},
                                {"role", object.value("ACL").toObject().value(m_accountName).toString()}};
            summaries.append(summary);
        }
        m_mapSummaries = summaries;
        emit mapsReady();
    });
}

QVariantList ShareClient::mapSummaries() const {
    return m_mapSummaries;
}

void ShareClient::createMap(const QByteArray &snapshot, const QString &name) {
    const QString encoded = QString::fromLatin1(snapshot.toBase64());
    const QJsonObject envelope{{"name", name}, {"snapshot", encoded}};
    QNetworkRequest request = makeRequest(QStringLiteral("v1/maps"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_network->post(request, QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, snapshot] {
        reply->deleteLater();
        if (handleHttpError(reply, QStringLiteral("creating map"))) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QString mapId = doc.object().value("id").toString();
        m_mapStates.insert(mapId, {snapshot, 0});
        emit mapCreated(mapId);
    });
}

void ShareClient::invite(const QString &mapId, const QString &account, const QString &role) {
    QNetworkRequest request = makeRequest(QStringLiteral("v1/maps/%1/invites").arg(mapId));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QByteArray body = QStringLiteral("{\"account\":%1,\"role\":%2}")
                                .arg(jsonString(account), jsonString(role))
                                .toUtf8();
    QNetworkReply *reply = m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (handleHttpError(reply, QStringLiteral("sending invite"))) return;
        const QString code = QJsonDocument::fromJson(reply->readAll()).object().value("token").toString();
        if (code.isEmpty()) { emit error("The server did not return an invitation code. Please try again."); return; }
        emit invitationReady(code);
        emit inviteSent();
    });
}

void ShareClient::acceptInvite(const QString &token) {
    QNetworkReply *reply = m_network->post(makeRequest(QStringLiteral("v1/invites/%1/accept").arg(token)), QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (handleHttpError(reply, QStringLiteral("accepting invite"))) return;
        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
        const QString mapId = object.value("id").toString();
        if (mapId.isEmpty()) {
            emit error(QStringLiteral("accepting invite failed: invalid response"));
            return;
        }
        emit inviteAccepted(mapId, object.value("role").toString());
    });
}

void ShareClient::fetchMapState(const QString &mapId) {
    QNetworkReply *reply = m_network->get(makeRequest(QStringLiteral("v1/maps/%1").arg(mapId)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, mapId] {
        reply->deleteLater();
        if (handleHttpError(reply, QStringLiteral("fetching map"))) return;
        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
        const QByteArray state = QByteArray::fromBase64(object.value("Snapshot").toString().toUtf8());
        const quint64 seq = static_cast<quint64>(object.value("Seq").toInteger(0));
        m_mapStates.insert(mapId, {state, seq});
        emit mapStateReady(mapId, state, seq);
    });
}

QString ShareClient::mapState(const QString &mapId) const {
    return QString::fromUtf8(m_mapStates.value(mapId).state);
}

bool ShareClient::signedIn() const {
    return m_signedIn;
}

QString ShareClient::accountName() const {
    return m_accountName;
}

bool ShareClient::handleHttpError(QNetworkReply *reply, const QString &action) {
    if (reply->error() == QNetworkReply::NoError) return false;
    const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (reply->url().path() == QStringLiteral("/v1/auth/session")) {
        emit loginFailed(status.isValid() ? status.toString() : QString::number(reply->error()));
    } else {
        emit error(QStringLiteral("%1 failed: %2").arg(action, reply->errorString()));
    }
    return true;
}

QNetworkRequest ShareClient::makeRequest(const QString &path) const {
    QNetworkRequest request{QUrl(m_baseUrl + path)};
    request.setTransferTimeout(kTransferTimeoutMs);
    return request;
}
