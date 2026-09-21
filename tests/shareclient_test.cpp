#include <QtTest>
#include <QtNetwork>
#include <QSignalSpy>
#include "../src/collaboration/shareclient.h"

class StubHttpServer : public QObject {
    Q_OBJECT
public:
    explicit StubHttpServer(QObject *parent = nullptr) : QObject(parent) {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = m_server.nextPendingConnection();
            m_buffers.insert(socket, QByteArray());
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                m_buffers[socket] += socket->readAll();
                if (!m_buffers.value(socket).contains("\r\n\r\n")) return;
                const QString request = QString::fromUtf8(m_buffers.take(socket));
                m_requests.append(request);
                const QString path = request.section('\n', 0, 0).section(' ', 1, 1);
                const bool isPost = request.startsWith("POST");
                int status = 200;
                QByteArray body;
                if (path.endsWith("/v1/auth/session")) {
                    if (m_validLogin) body = "{\"accountId\":\"ada\"}";
                    else status = 401;
                } else if (path.endsWith("/v1/me")) {
                    body = "{\"accountId\":\"ada\"}";
                } else if (path == "/v1/maps" && !isPost) {
                    body = R"([{"ID":"a1b2c3","Owner":"ada"}])";
                } else if (path == "/v1/maps" && isPost) {
                    body = "{\"id\":\"m-123\",\"role\":\"owner\"}";
                } else if (path.contains("/invites") && path.contains("/accept")) {
                    body = "{\"id\":\"m-123\",\"role\":\"editor\"}";
                } else if (path.contains("/invites")) {
                    body = "{\"token\":\"tok-9\"}";
                } else if (path.contains("/v1/maps/")) {
                    body = "{\"ID\":\"m-123\",\"Owner\":\"ada\",\"Snapshot\":\"aGVsbG8=\"}";
                }
                const QByteArray statusText = status == 200 ? "200 OK" : "401 Unauthorized";
                const QByteArray setCookie = (path.endsWith("/v1/auth/session") && status == 200)
                    ? "Set-Cookie: AuthSession=tok-ada; Path=/; HttpOnly\r\n" : QByteArray();
                socket->write("HTTP/1.1 " + statusText + "\r\nContent-Type: application/json\r\n" + setCookie
                              + "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        });
    }
    void start() { m_server.listen(QHostAddress::LocalHost); }
    int port() const { return m_server.serverPort(); }
    bool m_validLogin = false;
    QStringList m_requests;
private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

class ShareClientTest : public QObject {
    Q_OBJECT
private slots:
    void loginStoresCookieAndAccount() {
        StubHttpServer server;
        server.start();
        server.m_validLogin = true;
        ShareClient client;
        client.setBaseUrl(QString("http://127.0.0.1:%1").arg(server.port()));
        QSignalSpy signedSpy(&client, &ShareClient::signedInChanged);
        client.login("ada", "secret");
        QTRY_COMPARE(client.signedIn(), true);
        QCOMPARE(signedSpy.count(), 1);
        client.account();
        QTRY_COMPARE(client.accountName(), QString("ada"));
        QTRY_VERIFY(std::any_of(server.m_requests.cbegin(), server.m_requests.cend(),
                                [](const QString &r) { return r.startsWith("GET /v1/me"); }));
        const QString meRequest = *std::find_if(server.m_requests.cbegin(), server.m_requests.cend(),
                                                [](const QString &r) { return r.startsWith("GET /v1/me"); });
        QVERIFY(meRequest.contains("Cookie:"));
        QVERIFY(meRequest.contains("AuthSession="));
        QVERIFY(server.m_requests.first().contains("POST /v1/auth/session"));
    }
    void loginRejectsBadCredentials() {
        StubHttpServer server;
        server.start();
        ShareClient client;
        client.setBaseUrl(QString("http://127.0.0.1:%1").arg(server.port()));
        QSignalSpy failedSpy(&client, &ShareClient::loginFailed);
        QSignalSpy signedSpy(&client, &ShareClient::signedInChanged);
        client.login("ada", "wrong");
        QTRY_COMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.at(0).at(0).toString(), QString("401"));
        QCOMPARE(client.signedIn(), false);
        QCOMPARE(signedSpy.count(), 0);
    }
    void listAndCreateMaps() {
        StubHttpServer server;
        server.start();
        ShareClient client;
        client.setBaseUrl(QString("http://127.0.0.1:%1").arg(server.port()));
        QSignalSpy readySpy(&client, &ShareClient::mapsReady);
        client.maps();
        QTRY_COMPARE(readySpy.count(), 1);
        QCOMPARE(client.mapState("a1b2c3"), QByteArray());
        QSignalSpy createdSpy(&client, &ShareClient::mapCreated);
        client.createMap(QByteArray("{\"nodes\":[]}"));
        QTRY_COMPARE(createdSpy.count(), 1);
        QCOMPARE(createdSpy.at(0).at(0).toString(), QString("m-123"));
        QVERIFY(server.m_requests.last().startsWith("POST /v1/maps"));
    }
    void inviteAndAccept() {
        StubHttpServer server;
        server.start();
        ShareClient client;
        client.setBaseUrl(QString("http://127.0.0.1:%1").arg(server.port()));
        QSignalSpy sentSpy(&client, &ShareClient::inviteSent);
        client.invite("m-123", "bo", "editor");
        QTRY_COMPARE(sentSpy.count(), 1);
        QSignalSpy acceptedSpy(&client, &ShareClient::mapCreated);
        client.acceptInvite("tok-9");
        QTRY_COMPARE(acceptedSpy.count(), 1);
        QCOMPARE(acceptedSpy.at(0).at(0).toString(), QString("m-123"));
    }
    void fetchMapState() {
        StubHttpServer server;
        server.start();
        ShareClient client;
        client.setBaseUrl(QString("http://127.0.0.1:%1").arg(server.port()));
        QSignalSpy stateSpy(&client, &ShareClient::mapStateReady);
        client.fetchMapState("m-123");
        QTRY_COMPARE(stateSpy.count(), 1);
        QCOMPARE(stateSpy.at(0).at(0).toString(), QString("m-123"));
        QCOMPARE(stateSpy.at(0).at(1).toByteArray(), QByteArray("hello"));
        QCOMPARE(stateSpy.at(0).at(2).toULongLong(), quint64(0));
        QCOMPARE(client.mapState("m-123"), QByteArray("hello"));
    }
};

QTEST_MAIN(ShareClientTest)
#include "shareclient_test.moc"
