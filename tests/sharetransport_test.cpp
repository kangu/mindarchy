#include <QtTest>
#include <QtNetwork>
#include <QtWebSockets>
#include <QSignalSpy>
#include "../src/collaboration/sharetransport.h"

class StubLiveServer : public QObject {
    Q_OBJECT
public:
    explicit StubLiveServer(QObject *parent = nullptr) : QObject(parent) {
        connect(&m_server, &QWebSocketServer::newConnection, this, [this] {
            m_socket = m_server.nextPendingConnection();
            connect(m_socket, &QWebSocket::textMessageReceived, this, [this](const QString &message) {
                m_messages.append(message);
            });
        });
        m_server.listen(QHostAddress::LocalHost);
    }
    void sendJson(const QJsonObject &object) {
        if (m_socket) m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    }
    QUrl urlFor(const QString &mapId) const {
        return QUrl(QString("ws://127.0.0.1:%1/v1/maps/%2/live?protocol=2").arg(m_server.serverPort()).arg(mapId));
    }
    QString baseUrl() const { return QString("ws://127.0.0.1:%1").arg(m_server.serverPort()); }
    int port() const { return m_server.serverPort(); }
    QWebSocket *m_socket = nullptr;
    QStringList m_messages;
private:
    QWebSocketServer m_server{QStringLiteral("stub"), QWebSocketServer::NonSecureMode, this};
};

class ShareTransportTest : public QObject {
    Q_OBJECT
private slots:
    void liveV2RoundTrip() {
        StubLiveServer server; ShareTransport transport; transport.setBaseUrl(server.baseUrl());transport.join("map-1");
        QTRY_VERIFY(server.m_socket);QSignalSpy live(&transport,&ShareTransport::liveMessage);
        server.sendJson({{"type","hello"},{"protocol",2},{"mapId","map-1"},{"epoch","epoch"},{"revision",0}});
        QTRY_COMPARE(live.count(),1);QCOMPARE(transport.protocol(),2);
        QByteArray payload="{\"version\":2,\"id\":\"stable\",\"ops\":[]}";transport.submitEdit(payload,"hash");
        QTRY_COMPARE(server.m_messages.size(),1);auto frame=QJsonDocument::fromJson(server.m_messages.first().toUtf8()).object();
        QCOMPARE(frame["type"].toString(),QString("edit"));QCOMPARE(QByteArray::fromBase64(frame["operation"].toString().toLatin1()),payload);
        transport.submit(1,"hash",payload);QTest::qWait(20);QCOMPARE(server.m_messages.size(),1);
        server.sendJson({{"type","applied"},{"epoch","epoch"},{"revision",1}});QTRY_COMPARE(live.count(),2);
        server.sendJson({{"type","durable"},{"epoch","epoch"},{"revision",1}});QTRY_COMPARE(live.count(),3);
    }
    void encodeSubmitEnvelope() {
        const QByteArray changes = "hello";
        const QString hash = QString::fromLatin1(QCryptographicHash::hash(changes, QCryptographicHash::Sha256).toHex());
        const QByteArray envelope = ShareTransport::encodeSubmit(
            "a6e7db7b-81a6-43e2-a1cf-25421f4f81e1", "1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65", 7, hash, changes);
        const QJsonObject object = QJsonDocument::fromJson(envelope).object();
        QCOMPARE(object.value("type").toString(), QString("submit"));
        const QJsonObject changesObject = object.value("changes").toObject();
        QCOMPARE(changesObject.value("version").toInt(), 1);
        QCOMPARE(changesObject.value("mapId").toString(), QString("a6e7db7b-81a6-43e2-a1cf-25421f4f81e1"));
        QCOMPARE(changesObject.value("deviceId").toString(), QString("1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65"));
        QCOMPARE(changesObject.value("counter").toInteger(), qint64(7));
        QCOMPARE(changesObject.value("hash").toString(), QString("2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"));
        QCOMPARE(QByteArray::fromBase64(changesObject.value("changes").toString().toUtf8()), changes);
    }
    void joinOpensLiveEndpointAndHandlesHello() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setBaseUrl(server.baseUrl());
        transport.setSessionCookie("tok-ada");
        QSignalSpy joinedSpy(&transport, &ShareTransport::joined);
        QSignalSpy connectedSpy(&transport, &ShareTransport::connectedChanged);
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        QCOMPARE(server.m_socket->requestUrl(), server.urlFor("map-1"));
        const QByteArray cookieHeader = server.m_socket->request().rawHeader(QByteArrayLiteral("Cookie"));
        QVERIFY(cookieHeader.contains("AuthSession=tok-ada"));        server.sendJson({{"type", "hello"}, {"mapId", "map-1"}, {"role", "editor"}});
        QTRY_COMPARE(joinedSpy.count(), 1);
        QCOMPARE(joinedSpy.at(0).at(0).toString(), QString("map-1"));
        QCOMPARE(joinedSpy.at(0).at(1).toString(), QString("editor"));
        QCOMPARE(transport.connected(), true);
        QCOMPARE(connectedSpy.count(), 1);
        QCOMPARE(transport.errorCode(), QString());
    }
    void submitSendsEnvelopeOverWire() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setBaseUrl(server.baseUrl());
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        server.sendJson({{"type", "hello"}, {"mapId", "map-1"}, {"role", "editor"}});
        QTRY_COMPARE(transport.protocol(), 1);
        const QByteArray changes = "hello";
        transport.submit(3, "abc", changes);
        QTRY_COMPARE(server.m_messages.count(), 1);
        const QJsonObject object = QJsonDocument::fromJson(server.m_messages.first().toUtf8()).object();
        QCOMPARE(object.value("type").toString(), QString("submit"));
        const QJsonObject changesObject = object.value("changes").toObject();
        QCOMPARE(changesObject.value("mapId").toString(), QString("map-1"));
        QCOMPARE(changesObject.value("counter").toInteger(), qint64(3));
        QCOMPARE(changesObject.value("hash").toString(), QString("abc"));
        QVERIFY(!changesObject.value("deviceId").toString().isEmpty());
        QCOMPARE(QByteArray::fromBase64(changesObject.value("changes").toString().toUtf8()), changes);
    }
    void setDeviceIdOverridesSubmitDeviceId() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setDeviceId("coordinator-device");
        transport.setBaseUrl(server.baseUrl());
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        transport.submit(1, "h", QByteArray("x"));
        QTest::qWait(20);QVERIFY(server.m_messages.isEmpty());
        server.sendJson({{"type","hello"},{"mapId","map-1"}});
        QTRY_COMPARE(transport.protocol(),1);
        QCOMPARE(transport.deviceId(), QString("coordinator-device"));
        transport.submit(1, "h", QByteArray("x"));
        QTRY_COMPARE(server.m_messages.count(), 1);
        const QJsonObject object = QJsonDocument::fromJson(server.m_messages.first().toUtf8()).object();
        QCOMPARE(object.value("changes").toObject().value("deviceId").toString(), QString("coordinator-device"));
    }
    void committedDecodesState() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setBaseUrl(server.baseUrl());
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        QSignalSpy committedSpy(&transport, &ShareTransport::committed);
        server.sendJson({{"type", "committed"},
                         {"receipt", QJsonObject{{"deviceId", "dev-1"}, {"counter", 5}, {"hash", "abc"}, {"seq", 12}}},
                         {"state", "aGVsbG8="},
                         {"sender", "bo"}});
        QTRY_COMPARE(committedSpy.count(), 1);
        QCOMPARE(committedSpy.at(0).at(0).toULongLong(), quint64(12));
        QCOMPARE(committedSpy.at(0).at(1).toString(), QString("dev-1"));
        QCOMPARE(committedSpy.at(0).at(2).toULongLong(), quint64(5));
        QCOMPARE(committedSpy.at(0).at(3).toString(), QString("abc"));
        QCOMPARE(committedSpy.at(0).at(4).toByteArray(), QByteArray("hello"));
        QCOMPARE(committedSpy.at(0).at(5).toString(), QString("bo"));
    }
    void rejectedPassthrough() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setBaseUrl(server.baseUrl());
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        QSignalSpy rejectedSpy(&transport, &ShareTransport::rejected);
        server.sendJson({{"type", "rejected"}, {"code", "counter_reuse"}});
        QTRY_COMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(), QString("counter_reuse"));
    }
    void presenceRosterAndSingleEntryMerge() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setBaseUrl(server.baseUrl());
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        QSignalSpy presenceSpy(&transport, &ShareTransport::presence);
        server.sendJson({{"type", "presence"}, {"accounts", QJsonArray{"ada", "bo"}}});
        QTRY_COMPARE(presenceSpy.count(), 1);
        QCOMPARE(presenceSpy.at(0).at(0).toStringList(), (QStringList{"ada", "bo"}));
        server.sendJson({{"type", "presence"}, {"accountId", "cy"}});
        QTRY_COMPARE(presenceSpy.count(), 2);
        QCOMPARE(presenceSpy.at(1).at(0).toStringList(), (QStringList{"ada", "bo", "cy"}));
    }
    void presenceHeartbeatEveryFiveSeconds() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setBaseUrl(server.baseUrl());
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        server.sendJson({{"type", "hello"}, {"mapId", "map-1"}, {"role", "editor"}});
        server.m_messages.clear();
        const qint64 start = QDateTime::currentMSecsSinceEpoch();
        bool sawPresence = false;
        while (QDateTime::currentMSecsSinceEpoch() - start < 6500) {
            for (const QString &message : server.m_messages) {
                if (QJsonDocument::fromJson(message.toUtf8()).object().value("type").toString() == QString("presence"))
                    sawPresence = true;
            }
            if (sawPresence) break;
            QTest::qWait(100);
        }
        QVERIFY(sawPresence);
        qint64 sawAt = -1;
        for (const QString &message : server.m_messages) {
            if (QJsonDocument::fromJson(message.toUtf8()).object().value("type").toString() == QString("presence"))
                sawAt = QDateTime::currentMSecsSinceEpoch();
        }
        const int leaveMarker = server.m_messages.count();
        QVERIFY(sawAt > 0);
        transport.leave();
        QTest::qWait(5500);
        QVERIFY(server.m_messages.count() <= leaveMarker + 1);
        for (int i = leaveMarker + 1; i < server.m_messages.count(); ++i) {
            QCOMPARE(QJsonDocument::fromJson(server.m_messages.at(i).toUtf8()).object().value("type").toString(),
                     QString("presence"));
        }
    }
    void bareHostBaseUrlJoinsOverWs() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setBaseUrl(QString("localhost:%1").arg(server.port()));
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        QCOMPARE(server.m_socket->requestUrl().scheme(), QString("ws"));
        QCOMPARE(server.m_socket->requestUrl().host(), QString("localhost"));
    }
    void joinDifferentMapLeavesFirst() {
        StubLiveServer server;
        ShareTransport transport;
        transport.setBaseUrl(server.baseUrl());
        transport.join("map-1");
        QTRY_VERIFY(server.m_socket);
        QWebSocket *first = server.m_socket;
        transport.join("map-2");
        QTRY_VERIFY(server.m_socket != first);
        QCOMPARE(server.m_socket->requestUrl(), server.urlFor("map-2"));
    }
};

QTEST_MAIN(ShareTransportTest)
#include "sharetransport_test.moc"
