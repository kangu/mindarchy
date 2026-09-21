#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QScopedPointer>
#include <QCryptographicHash>
#include "../src/collaboration/sharecoordinator.h"
#include "../src/collaboration/shareclient.h"
#include "../src/collaboration/sharetransport.h"
#include "../src/collaboration/sharesettings.h"
#include "../src/appidentity.h"
#include "../src/engine.h"

class FakeClient : public ShareClient {
public:
    using ShareClient::ShareClient;
    void login(const QString &, const QString &) override {
        fakeSignedIn = true;
        fakeAccount = "test-account";
        emit signedInChanged();
    }
    void createMap(const QByteArray &) override {
        createdMapId = "m-" + QUuid::createUuid().toString(QUuid::WithoutBraces).mid(1, 8);
        emit mapCreated(createdMapId);
    }
    bool signedIn() const override { return fakeSignedIn; }
    QString accountName() const override { return fakeAccount; }

    bool fakeSignedIn = false;
    QString fakeAccount = "test-account";
    QString createdMapId;
};

class FakeTransport : public ShareTransport {
public:
    using ShareTransport::ShareTransport;
    void join(const QString &mapId) override {
        joins.append(mapId);
        lastJoined = mapId;
        online = true;
    }
    void leave() override { online = false; }
    void submit(quint64 counter, const QString &hash, const QByteArray &changes) override {
        counters.append(counter);
        hashes.append(hash);
        ++submitCount;
        lastCounter = counter;
        lastHash = hash;
        lastChanges = changes;
    }
    bool connected() const override { return online; }

    void peerCommit(quint64 seq, quint64 counter, const QByteArray &state) {
        emit committed(seq, "peer-device", counter, "peer-hash", state, "peer-device");
    }
    void ownCommit(quint64 counter) {
        emit committed(counter, clientUuid, counter, "own-hash", QByteArray(), clientUuid);
    }
    void reject(const QString &code) { emit rejected(code); }
    void emitJoined() { emit joined(lastJoined, "owner"); }

    bool online = false;
    int submitCount = 0;
    quint64 lastCounter = 0;
    QString lastHash;
    QByteArray lastChanges;
    QString lastJoined;
    QStringList joins;
    QList<quint64> counters;
    QStringList hashes;
    QString clientUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
};

class ShareCoordinatorTest : public QObject {
    Q_OBJECT
private:
    struct Harness {
        QString serverUrlBackup;
        QTemporaryDir dir;
        Engine engine;
        FakeClient *client = nullptr;
        FakeTransport *transport = nullptr;
        QScopedPointer<ShareCoordinator> coordinator;

        bool setup() {
            serverUrlBackup = AppIdentity::windowSettings()->value("sharing/serverUrl", "share.mindarchy.xyz").toString();
            client = new FakeClient;
            transport = new FakeTransport;
            coordinator.reset(new ShareCoordinator(&engine, client, transport, new ShareSettings));
            return true;
        }

        bool loadDocument(const QString &name) {
            return engine.loadDocumentBytes(fixtureBytes(), dir.filePath(name));
        }

        static QByteArray fixtureBytes() {
            QJsonObject node{{"id", 1},         {"parent", -1},     {"children", QJsonArray{}},
                             {"text", ""},      {"notes", ""},      {"folded", false},
                             {"task", false},   {"checked", false}, {"x", 0},
                             {"y", 0}};
            QJsonObject doc{{"format", "mindarchy"},
                            {"version", 1},
                            {"layout", "Horizontal"},
                            {"spacing", "Standard"},
                            {"branchStyle", "Rounded"},
                            {"themeId", "lab"},
                            {"manual", false},
                            {"nodes", QJsonArray{node}},
                            {"connections", QJsonArray{}}};
            return QJsonDocument(doc).toJson();
        }

        void pump(int ms) {
            QEventLoop loop;
            QTimer::singleShot(ms, &loop, &QEventLoop::quit);
            loop.exec();
        }

        void restore() { AppIdentity::windowSettings()->setValue("sharing/serverUrl", serverUrlBackup); }
    };

    static QString sha256Hex(const QByteArray &bytes) {
        return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    }

private slots:
    void signInMarksCoordinatorSignedIn() {
        Harness h;
        h.setup();

        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        QVERIFY(!h.coordinator->signedIn());
        h.coordinator->signIn("ada", "secret");
        QVERIFY(h.coordinator->signedIn());
        QCOMPARE(h.coordinator->accountName(), QString("test-account"));
        h.restore();
    }

    void shareCurrentMapAttachesAndJoins() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");
        h.coordinator->shareCurrentMap();

        QVERIFY(QFile::exists(h.engine.documentPath() + ".share"));
        QCOMPARE(h.coordinator->mapId(), h.client->createdMapId);
        QCOMPARE(h.transport->joins.last(), h.client->createdMapId);
        h.restore();
    }

    void engineChangeUploadsDebouncedSnapshot() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");
        h.coordinator->shareCurrentMap();

        QVERIFY(h.engine.setText(1, "hello"));
        h.pump(140);
        QCOMPARE(h.transport->submitCount, 0);
        h.pump(300);
        QCOMPARE(h.transport->submitCount, 1);
        QCOMPARE(sha256Hex(h.engine.documentBytes()), h.transport->lastHash);
        QCOMPARE(h.transport->lastCounter, quint64(1));
        QVERIFY(h.transport->lastChanges == h.engine.documentBytes());
        h.restore();
    }

    void peerCommitLoadsAndDoesNotSubmit() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");
        h.coordinator->shareCurrentMap();

        Engine peerEngine;
        QVERIFY(peerEngine.loadDocumentBytes(h.fixtureBytes(), {}));
        const QByteArray peerState = peerEngine.documentBytes();
        h.transport->peerCommit(7, 3, peerState);
        QCOMPARE(QJsonDocument::fromJson(h.engine.documentBytes()).object(),
                 QJsonDocument::fromJson(peerState).object());
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 0);
        h.restore();
    }

    void counterReuseRequeuesWithNewCounter() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");
        h.coordinator->shareCurrentMap();
        QVERIFY(h.engine.setText(1, "first"));
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 1);
        QCOMPARE(h.transport->counters.first(), quint64(1));

        h.transport->reject("counter_reuse");
        QCOMPARE(h.transport->counters.last(), quint64(2));
        QCOMPARE(h.transport->lastCounter, quint64(2));
        h.restore();
    }

    void outboxDrainsOnJoin() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");
        h.coordinator->shareCurrentMap();

        h.transport->online = false;
        QVERIFY(h.engine.setText(1, "queued edit"));
        h.pump(450);
        QCOMPARE(h.transport->submitCount, 0);
        QCOMPARE(h.coordinator->shareStatus(), QString("Offline · 1 queued"));

        h.transport->emitJoined();
        QCOMPARE(h.transport->submitCount, 1);
        QCOMPARE(h.transport->lastCounter, quint64(1));
        QCOMPARE(QJsonDocument::fromJson(h.transport->lastChanges).object(),
                 QJsonDocument::fromJson(h.engine.documentBytes()).object());
        h.restore();
    }
};

QTEST_MAIN(ShareCoordinatorTest)
#include "sharecoordinator_test.moc"
