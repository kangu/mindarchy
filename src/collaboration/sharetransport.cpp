#include "sharetransport.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkRequest>
#include <QUrl>

namespace {
constexpr int kBackoffBaseMs = 1000;
constexpr int kBackoffCapMs = 30000;
constexpr int kPingIntervalMs = 15000;
constexpr int kPresenceIntervalMs = 5000;

QJsonObject changesObject(const QString &mapId, const QString &deviceId, quint64 counter,
                          const QString &hash, const QByteArray &changes) {
    return QJsonObject{{"version", 1},
                       {"mapId", mapId},
                       {"deviceId", deviceId},
                       {"counter", static_cast<qint64>(counter)},
                       {"hash", hash},
                       {"changes", QString::fromLatin1(changes.toBase64())}};
}
}

ShareTransport::ShareTransport(QObject *parent)
    : QObject(parent),
      m_deviceId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_reconnectTimer(this),
      m_pingTimer(this),
      m_presenceTimer(this) {
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (!m_mapId.isEmpty() && !m_connected) openSocket(m_mapId);
    });
    m_pingTimer.setInterval(kPingIntervalMs);
    connect(&m_pingTimer, &QTimer::timeout, this, [this] {
        if (m_connected) m_socket.ping();
    });
    m_presenceTimer.setInterval(kPresenceIntervalMs);
    connect(&m_presenceTimer, &QTimer::timeout, this, [this] {
        if (m_connected && !m_mapId.isEmpty()) sendPresence();
    });
    connect(&m_socket, &QWebSocket::connected, this, &ShareTransport::handleConnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &ShareTransport::handleMessage);
    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        const bool wasConnected = m_connected;
        m_connected = false;
        m_pingTimer.stop();
        m_presenceTimer.stop();
        emit connectedChanged();
        if (wasConnected && !m_mapId.isEmpty()) scheduleReconnect();
    });
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!m_connected) {
            m_errorCode = m_socket.errorString();
            emit connectedChanged();
            if (!m_mapId.isEmpty()) scheduleReconnect();
        }
    });
}

void ShareTransport::setBaseUrl(const QString &url) {
    m_baseUrl = url;
    if (m_baseUrl.startsWith(QStringLiteral("https://"))) {
        m_baseUrl.replace(0, 8, QStringLiteral("wss://"));
    } else if (m_baseUrl.startsWith(QStringLiteral("http://"))) {
        m_baseUrl.replace(0, 7, QStringLiteral("ws://"));
    }
    if (m_baseUrl.endsWith('/')) m_baseUrl.chop(1);
}

void ShareTransport::setSessionCookie(const QString &value) {
    m_sessionCookie = value;
}

void ShareTransport::join(const QString &mapId) {
    if (m_connected && mapId == m_mapId) return;
    if (m_connected || m_mapId != mapId) m_socket.abort();
    m_mapId = mapId;
    m_errorCode.clear();
    m_accounts.clear();
    m_reconnectTimer.stop();
    openSocket(mapId);
}

void ShareTransport::leave() {
    m_reconnectTimer.stop();
    m_mapId.clear();
    m_accounts.clear();
    m_presenceTimer.stop();
    m_socket.abort();
}

void ShareTransport::setDeviceId(const QString &id) {
    if (!id.isEmpty()) m_deviceId = id;
}

QString ShareTransport::deviceId() const { return m_deviceId; }

void ShareTransport::submit(quint64 counter, const QString &hash, const QByteArray &changes) {
    if (!m_connected) return;
    const QJsonObject envelope{{"type", "submit"},
                               {"changes", changesObject(m_mapId, m_deviceId, counter, hash, changes)}};
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
}

bool ShareTransport::connected() const {
    return m_connected;
}

void ShareTransport::sendPresence() {
    const QJsonObject envelope{{"type", "presence"}};
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
}

QString ShareTransport::errorCode() const {
    return m_errorCode;
}

QByteArray ShareTransport::encodeSubmit(const QString &mapId, const QString &deviceId, quint64 counter,
                                        const QString &hash, const QByteArray &changes) {
    const QJsonObject envelope{{"type", "submit"},
                               {"changes", changesObject(mapId, deviceId, counter,
                                                         QString::fromLatin1(
                                                             QCryptographicHash::hash(changes, QCryptographicHash::Sha256).toHex()),
                                                         changes)}};
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

QUrl ShareTransport::liveUrl(const QString &mapId) const {
    return QUrl(m_baseUrl + QStringLiteral("/v1/maps/%1/live").arg(mapId));
}

void ShareTransport::openSocket(const QString &mapId) {
    QNetworkRequest request{liveUrl(mapId)};
    if (!m_sessionCookie.isEmpty()) {
        const QNetworkCookie cookie(QByteArrayLiteral("AuthSession"), m_sessionCookie.toUtf8());
        request.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(QList<QNetworkCookie>{cookie}));
    }
    m_socket.open(request);
}

void ShareTransport::handleConnected() {
    resetBackoff();
    m_connected = true;
    m_errorCode.clear();
    m_pingTimer.start();
    m_presenceTimer.start();
    emit connectedChanged();
}

void ShareTransport::handleMessage(const QString &message) {
    const QJsonObject object = QJsonDocument::fromJson(message.toUtf8()).object();
    const QString type = object.value("type").toString();
    if (type == QStringLiteral("hello")) {
        emit joined(object.value("mapId").toString(), object.value("role").toString());
    } else if (type == QStringLiteral("committed")) {
        const QJsonObject receipt = object.value("receipt").toObject();
        const QByteArray state = QByteArray::fromBase64(object.value("state").toString().toUtf8());
        emit committed(static_cast<quint64>(receipt.value("seq").toInteger(0)),
                       receipt.value("deviceId").toString(),
                       static_cast<quint64>(receipt.value("counter").toInteger(0)),
                       receipt.value("hash").toString(),
                       state,
                       object.value("sender").toString());
    } else if (type == QStringLiteral("presence")) {
        handlePresence(object);
    } else if (type == QStringLiteral("rejected")) {
        emit rejected(object.value("code").toString());
    }
}

void ShareTransport::handlePresence(const QJsonObject &object) {
    if (object.contains("accounts")) {
        m_accounts.clear();
        for (const QJsonValue &value : object.value("accounts").toArray()) {
            const QString account = value.toString();
            if (!account.isEmpty()) m_accounts.append(account);
        }
    } else {
        const QString account = object.value("accountId").toString();
        if (account.isEmpty() || m_accounts.contains(account)) return;
        m_accounts.append(account);
    }
    emit presence(m_accounts);
}

void ShareTransport::scheduleReconnect() {
    m_reconnectTimer.start(m_backoffMs);
    m_backoffMs = qMin(m_backoffMs * 2, kBackoffCapMs);
}

void ShareTransport::resetBackoff() {
    m_backoffMs = kBackoffBaseMs;
}
