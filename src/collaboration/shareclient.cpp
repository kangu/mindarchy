#include "shareclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
constexpr int kTransferTimeoutMs = 5000;

QString jsonString(const QString &value) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson().mid(1).chopped(2));
}
}

ShareClient::ShareClient(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)) {
    m_network->setCookieJar(new QNetworkCookieJar(m_network));
    m_network->setTransferTimeout(kTransferTimeoutMs);
}

void ShareClient::setBaseUrl(const QString &url) {
    m_baseUrl = url;
    if (!m_baseUrl.endsWith('/')) m_baseUrl += '/';
}

void ShareClient::login(const QString &username, const QString &password) {
    QNetworkRequest request = makeRequest(QStringLiteral("v1/auth/session"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QByteArray body = QStringLiteral("{\"username\":%1,\"password\":%2}")
                                .arg(jsonString(username), jsonString(password))
                                .toUtf8();
    QNetworkReply *reply = m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (handleHttpError(reply, QStringLiteral("login"))) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QString accountId = doc.object().value("accountId").toString();
        if (accountId.isEmpty()) {
            emit loginFailed(QStringLiteral("invalid_message"));
            return;
        }
        m_accountName = accountId;
        m_signedIn = true;
        emit signedInChanged();
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
        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();
            const QString mapId = object.value("ID").toString();
            if (mapId.isEmpty()) continue;
            const QByteArray snapshot = QByteArray::fromBase64(object.value("Snapshot").toString().toUtf8());
            const quint64 seq = static_cast<quint64>(object.value("Seq").toInteger(0));
            m_mapStates.insert(mapId, {snapshot, seq});
        }
        emit mapsReady();
    });
}

void ShareClient::createMap(const QByteArray &snapshot) {
    QNetworkRequest request = makeRequest(QStringLiteral("v1/maps"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    QNetworkReply *reply = m_network->post(request, snapshot);
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
