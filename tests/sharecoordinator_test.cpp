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
#include <QNetworkCookie>
#include <QScopedPointer>
#include <QCryptographicHash>
#include "../src/collaboration/sharecoordinator.h"
#include "../src/collaboration/liveoperations.h"
#include "../src/collaboration/localstore.h"
#include "../src/documentsession.h"
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QScopeGuard>
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
    void createMap(const QByteArray &, const QString &) override {
        createdMapId = "m-" + QUuid::createUuid().toString(QUuid::WithoutBraces).mid(1, 8);
        emit mapCreated(createdMapId);
    }
    void maps() override { emit mapsReady(); }
    QVariantList mapSummaries() const override { return fakeMaps; }
    void acceptInvite(const QString &token) override {
        acceptedTokens.append(token);
        emit inviteAccepted(acceptedMapId, acceptedRole);
    }
    void fetchMapState(const QString &mapId) override { fetchedMaps.append(mapId); }
    bool signedIn() const override { return fakeSignedIn; }
    QString accountName() const override { return fakeAccount; }

    bool fakeSignedIn = false;
    QString fakeAccount = "test-account";
    QString createdMapId;
    QVariantList fakeMaps;
    QString acceptedMapId = "m-shared";
    QString acceptedRole = "editor";
    QStringList acceptedTokens;
    QStringList fetchedMaps;
};

class FakeTransport : public ShareTransport {
public:
    using ShareTransport::ShareTransport;
    QList<QByteArray> edits;
    void submitEdit(const QByteArray &payload,const QString &) override { edits.append(payload); }
    void setSessionCookie(const QString &value) override { sessionCookie = value; }
    void join(const QString &mapId) override {
        joins.append(mapId);
        lastJoined = mapId;
        cookieAtJoin = sessionCookie;
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
        emit committed(seq, "peer-device", counter, "peer-hash", state, "acct_peer");
    }
    void ownCommit(quint64 counter) {
        emit committed(counter, clientUuid, counter, "own-hash", QByteArray(), "acct_abc");
    }
    void reject(const QString &code) { emit rejected(code); }
    void emitJoined() { emit joined(lastJoined, "owner"); }

    bool online = false;
    QString sessionCookie;
    QString cookieAtJoin;
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
    void failedOutboxWritePreservesVisibleEdits_data() {
        QTest::addColumn<bool>("firstHello");
        QTest::newRow("first hello") << true;
        QTest::newRow("already live") << false;
    }
    void failedOutboxWritePreservesVisibleEdits() {
        QFETCH(bool, firstHello);
        Harness h; QVERIFY(h.setup()); QVERIFY(h.loadDocument("full-outbox.omm"));
        h.coordinator->signIn("test-account", "password"); h.coordinator->shareCurrentMap();
        const auto baseline=LiveOperations::normalize(h.engine.documentBytes());
        QJsonObject hello{{"type","hello"},{"protocol",2},{"epoch","e"},{"revision",0},
                          {"state",QString::fromLatin1(QJsonDocument(baseline).toJson(QJsonDocument::Compact).toBase64())}};
        if(!firstHello) emit h.transport->liveMessage(hello);
        const QString scope=sha256Hex((h.coordinator->serverUrl()+"\n"+h.client->accountName()).toUtf8());
        const QString connection="full-outbox-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto db=QSqlDatabase::addDatabase("QSQLITE",connection);
        db.setDatabaseName(QDir(DocumentSession::defaultDirectory()).filePath(scope+"/collaboration.sqlite"));
        QVERIFY(db.open());
        auto cleanup=qScopeGuard([&]{QSqlQuery(db).exec("DROP TRIGGER IF EXISTS fail_test_outbox");db.close();db={};QSqlDatabase::removeDatabase(connection);h.restore();});
        QVERIFY(QSqlQuery(db).exec("CREATE TRIGGER fail_test_outbox BEFORE INSERT ON pending BEGIN SELECT RAISE(ABORT, 'simulated disk full'); END"));
        QVERIFY(h.engine.setText(1,"must survive disk failure"));
        emit h.transport->liveMessage(hello);
        h.transport->emitJoined();
        QCOMPARE(h.engine.selectedText(),QString("must survive disk failure"));
        QCOMPARE(h.coordinator->shareStatus(),QString("Could not save changes for synchronization"));
        QVERIFY(h.transport->edits.isEmpty());
        auto applied=hello;applied["type"]="applied";applied["revision"]=1;
        emit h.transport->liveMessage(applied);applied["type"]="durable";emit h.transport->liveMessage(applied);
        QCOMPARE(h.engine.selectedText(),QString("must survive disk failure"));
        QCOMPARE(h.coordinator->shareStatus(),QString("Could not save changes for synchronization"));
        QVERIFY(QSqlQuery(db).exec("DROP TRIGGER fail_test_outbox"));
        auto remote=baseline;auto nodes=remote["nodes"].toObject();auto node=nodes["legacy:1"].toObject();
        node["notes"]="peer edit after disk recovery";nodes["legacy:1"]=node;remote["nodes"]=nodes;
        applied["type"]="applied";applied["revision"]=2;applied["accountId"]="peer";
        applied["state"]=QString::fromLatin1(QJsonDocument(remote).toJson(QJsonDocument::Compact).toBase64());
        emit h.transport->liveMessage(applied); // No second server hello.
        QCOMPARE(h.engine.selectedText(),QString("must survive disk failure"));
        QCOMPARE(h.engine.selectedNotes(),QString("peer edit after disk recovery"));
        QCOMPARE(h.transport->edits.size(),1);
        QCOMPARE(h.coordinator->shareStatus(),QString("Live · saving"));
        const auto payload=QJsonDocument::fromJson(h.transport->edits.first()).object();
        QVERIFY(LiveOperations::apply(remote,payload["ops"].toArray()));
        applied["type"]="durable";applied["revision"]=3;
        applied["state"]=QString::fromLatin1(QJsonDocument(remote).toJson(QJsonDocument::Compact).toBase64());
        applied["receipts"]=QJsonArray{QJsonObject{{"id",payload["id"]},{"account","test-account"},{"hash",sha256Hex(h.transport->edits.first())}}};
        emit h.transport->liveMessage(applied);
        QCOMPARE(h.coordinator->shareStatus(),QString("Saved"));
        QCOMPARE(h.engine.selectedText(),QString("must survive disk failure"));
    }
    void upgradeExportsLegacyOutboxForRecovery() {
        Harness h;QVERIFY(h.setup());QVERIFY(h.loadDocument("upgrade.omm"));h.coordinator->signIn("test-account","password");h.coordinator->shareCurrentMap();
        const auto document=h.engine.documentBytes();CollaborationLocalStore old("test-account",DocumentSession::defaultDirectory());QVERIFY(old.open());QVERIFY(old.enqueue(h.coordinator->mapId(),1,sha256Hex(document),document));
        const auto baseline=LiveOperations::normalize(document);
        emit h.transport->liveMessage(QJsonObject{{"type","hello"},{"protocol",2},{"epoch","e"},{"revision",0},{"state",QString::fromLatin1(QJsonDocument(baseline).toJson(QJsonDocument::Compact).toBase64())}});
        QVERIFY(h.coordinator->shareStatus().startsWith("Older offline edits need recovery:"));QVERIFY(h.transport->edits.isEmpty());
        const QString path=QDir(DocumentSession::defaultDirectory()).filePath("Recovered offline maps/offline-"+sha256Hex(document)+".omm");QFile recovered(path);QVERIFY(recovered.open(QIODevice::ReadOnly));QCOMPARE(recovered.readAll(),document);
        QCOMPARE(old.pending(h.coordinator->mapId()).size(),1);old.removePending(old.pending(h.coordinator->mapId()).first().id);h.restore();
    }
    void firstHelloCapturesDebouncedEdit() {
        Harness h; QVERIFY(h.setup()); QVERIFY(h.loadDocument("first-hello.omm"));
        h.coordinator->signIn("test-account","password"); h.coordinator->shareCurrentMap();
        const auto baseline=LiveOperations::normalize(h.engine.documentBytes());
        QVERIFY(h.engine.setText(1,"typed before hello"));
        h.coordinator->handleMapStateReadyForTest(h.coordinator->mapId(), LiveOperations::project(baseline), 0);
        QCOMPARE(h.engine.selectedText(),QString("typed before hello"));
        emit h.transport->liveMessage(QJsonObject{{"type","hello"},{"protocol",2},{"epoch","e"},{"revision",0},{"state",QString::fromLatin1(QJsonDocument(baseline).toJson(QJsonDocument::Compact).toBase64())}});
        QCOMPARE(h.transport->edits.size(),1); QCOMPARE(h.engine.selectedText(),QString("typed before hello"));
        const auto ops=QJsonDocument::fromJson(h.transport->edits.first()).object()["ops"].toArray();
        QCOMPARE(ops.first().toObject()["path"].toArray(),QJsonArray({"nodes","legacy:1","text"})); h.restore();
    }
    void livePendingRebaseAndDurability() {
        Harness h;QVERIFY(h.setup());QVERIFY(h.loadDocument("live.omm"));h.coordinator->signIn("test-account","password");h.coordinator->shareCurrentMap();
        auto baseline=LiveOperations::normalize(h.engine.documentBytes());
        auto message=[&](QString type,int revision,QJsonObject state){return QJsonObject{{"type",type},{"protocol",2},{"epoch","e"},{"revision",revision},{"state",QString::fromLatin1(QJsonDocument(state).toJson(QJsonDocument::Compact).toBase64())}};};
        emit h.transport->liveMessage(message("hello",0,baseline));QVERIFY(h.engine.setText(1,"local"));QCOMPARE(h.transport->edits.size(),1);
        auto payload=QJsonDocument::fromJson(h.transport->edits[0]).object();auto remote=baseline;auto nodes=remote["nodes"].toObject();auto root=nodes["legacy:1"].toObject();root["notes"]="remote";nodes["legacy:1"]=root;remote["nodes"]=nodes;
        emit h.transport->liveMessage(message("applied",1,remote));QCOMPARE(h.engine.selectedText(),QString("local"));
        QCOMPARE(LiveOperations::normalize(h.engine.documentBytes())["nodes"].toObject()["legacy:1"].toObject()["notes"].toString(),QString("remote"));
        QVERIFY(LiveOperations::apply(remote,payload["ops"].toArray()));auto applied=message("applied",2,remote);applied["opId"]=payload["id"];applied["accountId"]="test-account";emit h.transport->liveMessage(applied);QCOMPARE(h.coordinator->shareStatus(),QString("Live · saving"));
        auto hello=message("hello",2,remote);hello["appliedIds"]=QJsonArray{payload["id"]};emit h.transport->liveMessage(hello);h.transport->emitJoined();QCOMPARE(h.transport->edits.last(),h.transport->edits.first());
        auto durable=message("durable",2,remote);durable["receipts"]=QJsonArray{QJsonObject{{"id",payload["id"]},{"account","test-account"},{"hash","wrong"}}};emit h.transport->liveMessage(durable);QCOMPARE(h.coordinator->shareStatus(),QString("Live · saving"));
        durable["receipts"]=QJsonArray{QJsonObject{{"id",payload["id"]},{"account","test-account"},{"hash",sha256Hex(h.transport->edits.first())}}};emit h.transport->liveMessage(durable);QCOMPARE(h.coordinator->shareStatus(),QString("Saved"));
        h.engine.undo();QCOMPARE(h.engine.selectedText(),QString(""));
        QCOMPARE(LiveOperations::normalize(h.engine.documentBytes())["nodes"].toObject()["legacy:1"].toObject()["notes"].toString(),QString("remote"));h.restore();
    }
    void unchangedNotificationsDoNotCreateSyncBatches() {
        Harness h; QVERIFY(h.setup()); QVERIFY(h.loadDocument("dedupe.omm"));
        h.coordinator->signIn("test-account", "password");
        h.coordinator->shareCurrentMap();
        QVERIFY(h.engine.setText(1, "one edit"));
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 1);
        // Engine notifications may come from UI refreshes without changing bytes.
        h.engine.changed();
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 1);
        QVERIFY(h.engine.setText(1, "another edit"));
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 2);
        h.restore();
    }

    void remoteUpdateDoesNotSuppressReturningToEarlierLocalState() {
        Harness h; QVERIFY(h.setup()); QVERIFY(h.loadDocument("dedupe-remote.omm"));
        h.coordinator->signIn("test-account", "password");
        h.coordinator->shareCurrentMap();
        QVERIFY(h.engine.setText(1, "A")); h.pump(400);
        QCOMPARE(h.transport->submitCount, 1);
        Engine peer; QVERIFY(peer.loadDocumentBytes(h.engine.documentBytes(), {}));
        QVERIFY(peer.setText(1, "B"));
        h.transport->peerCommit(2, 1, peer.documentBytes());
        QVERIFY(h.engine.setText(1, "A")); h.pump(400);
        QCOMPARE(h.transport->submitCount, 2);
        h.restore();
    }

    void automaticLoginProgressDoesNotBecomeAnOperationError() {
        Harness h; QVERIFY(h.setup());
        QSignalSpy failures(h.coordinator.data(), &ShareCoordinator::operationFailed);
        h.client->sessionMessage("Connecting to your sharing server…");
        QCOMPARE(h.coordinator->shareStatus(), QString("Connecting to your sharing server…"));
        QCOMPARE(failures.count(), 0);
        // Restore completes without any manual pending sign-in in the dialog.
        h.client->fakeSignedIn = true;
        h.client->signedInChanged();
        QVERIFY(h.coordinator->signedIn());
        QCOMPARE(h.coordinator->shareStatus(), QString("Online"));
        QCOMPARE(failures.count(), 0);
        h.client->sessionMessage("Offline · will reconnect automatically");
        QCOMPARE(failures.count(), 0);
        h.client->sessionRenewed();
        QCOMPARE(h.coordinator->shareStatus(), QString("Online"));
        // Real user-action failures still reach the feedback panel.
        h.client->error("Invitation could not be created");
        QCOMPARE(failures.count(), 1);
        h.restore();
    }
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

    void signInPropagatesAuthSessionCookieToTransport() {
        Harness h;
        h.setup();

        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        QNetworkCookie cookie(QByteArrayLiteral("AuthSession"), "tok-ada");
        cookie.setDomain("localhost");
        cookie.setPath("/");
        h.client->cookieJar()->insertCookie(cookie);
        h.coordinator->signIn("ada", "secret");
        QVERIFY(h.coordinator->signedIn());
        QCOMPARE(h.transport->sessionCookie, QString("tok-ada"));
        h.restore();
    }

    void reopenedMapRestoresSharingBeforeAndAfterLogin() {
        Harness first; QVERIFY(first.setup()); QVERIFY(first.loadDocument("persisted.omm"));
        QVERIFY(first.engine.save(first.engine.documentPath()));
        first.coordinator->signIn("ada", "secret"); first.coordinator->shareCurrentMap();
        const auto mapId = first.coordinator->mapId();
        const auto path = first.engine.documentPath();
        for (bool loginFirst : {false, true}) {
            Harness reopened; QVERIFY(reopened.setup());
            reopened.coordinator->chooseServer("http://127.0.0.1:19999");
            if (loginFirst) reopened.coordinator->signIn("ada", "secret");
            QVERIFY(reopened.engine.open(path));
            QCOMPARE(reopened.coordinator->mapId(), mapId);
            QCOMPARE(reopened.coordinator->serverUrl(), first.coordinator->serverUrl());
            if (!loginFirst) reopened.coordinator->signIn("ada", "secret");
            QCOMPARE(reopened.transport->lastJoined, mapId);
            QVERIFY(reopened.transport->online);
            QCOMPARE(reopened.client->fetchedMaps.last(), mapId);
            const auto privatePath = reopened.dir.filePath("private.omm");
            QFile file(privatePath); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(Harness::fixtureBytes()); file.close();
            QVERIFY(reopened.engine.open(privatePath));
            QVERIFY(reopened.coordinator->mapId().isEmpty());
            QVERIFY(!reopened.transport->online);
            QVERIFY(QFile::exists(path + ".share"));
            QVERIFY(reopened.engine.open(path));
            QCOMPARE(reopened.transport->lastJoined, mapId);
            QVERIFY(reopened.transport->online);
            reopened.restore();
        }
        first.restore();
    }

    void sharingSurvivesUnwritableSidecar() {
        Harness h; QVERIFY(h.setup()); QVERIFY(h.loadDocument("blocked.omm"));
        QVERIFY(QDir().mkdir(h.engine.documentPath() + ".share"));
        h.coordinator->signIn("ada", "secret");
        QSignalSpy warnings(h.coordinator.data(), &ShareCoordinator::operationFailed);
        h.coordinator->shareCurrentMap();
        QCOMPARE(h.coordinator->mapId(), h.client->createdMapId);
        QVERIFY(h.coordinator->canInvite());
        QCOMPARE(h.transport->joins.last(), h.client->createdMapId);
        QVERIFY(!warnings.isEmpty());
        const auto original = h.client->createdMapId;
        h.coordinator->shareCurrentMap();
        QCOMPARE(h.client->createdMapId, original);
        h.restore();
    }

    void unsavedMapSharesWithoutWorkingDirectorySidecar() {
        Harness h; QVERIFY(h.setup());
        const auto cwd = QDir::currentPath();
        auto restore = qScopeGuard([&] { QDir::setCurrent(cwd); });
        QVERIFY(QDir::setCurrent(h.dir.path()));
        h.coordinator->signIn("ada", "secret");
        h.coordinator->shareCurrentMap();
        QVERIFY(!h.coordinator->mapId().isEmpty());
        QVERIFY(h.coordinator->canInvite());
        QVERIFY(!QFile::exists(".share"));
        QVERIFY(h.engine.save(h.dir.filePath("saved.omm")));
        QVERIFY(QFile::exists(h.engine.documentPath() + ".share"));
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

    void signInJoinsAttachedMapWithCookieAlreadySet() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        QVERIFY(h.coordinator->attachMapId("m-attached"));
        QNetworkCookie cookie(QByteArrayLiteral("AuthSession"), "tok-attached");
        cookie.setDomain("localhost");
        cookie.setPath("/");
        h.client->cookieJar()->insertCookie(cookie);
        h.coordinator->signIn("ada", "secret");

        QCOMPARE(h.transport->lastJoined, QString("m-attached"));
        QCOMPARE(h.transport->cookieAtJoin, QString("tok-attached"));
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

    void ownCommitSkipsApplyClearsPending() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");
        h.coordinator->shareCurrentMap();

        QVERIFY(h.engine.setText(1, "own echo"));
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 1);
        QCOMPARE(h.transport->lastCounter, quint64(1));

        h.transport->clientUuid = h.coordinator->deviceId();
        h.transport->ownCommit(1);
        QCOMPARE(h.coordinator->shareStatus(), QString("Live"));
        const QJsonObject node = QJsonDocument::fromJson(h.engine.documentBytes())
                                     .object().value("nodes").toArray().at(0).toObject();
        QCOMPARE(node.value("text").toString(), QString("own echo"));

        h.transport->online = false;
        h.transport->emitJoined();
        QCOMPARE(h.transport->submitCount, 1);
        h.restore();
    }

    void counterReuseGivesUpAfterRepeatedRejections() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");
        h.coordinator->shareCurrentMap();
        QVERIFY(h.engine.setText(1, "first"));
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 1);
        QCOMPARE(h.transport->lastCounter, quint64(1));

        h.transport->reject("counter_reuse");
        QCOMPARE(h.transport->submitCount, 2);
        QCOMPARE(h.transport->lastCounter, quint64(2));

        h.transport->reject("counter_reuse");
        QCOMPARE(h.transport->submitCount, 2);
        QCOMPARE(h.coordinator->shareStatus(), QString("Sync error (counter_reuse)"));

        h.transport->emitJoined();
        QCOMPARE(h.transport->submitCount, 2);
        h.restore();
    }

    void acceptInviteAttachesJoinsAndHydrates() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");

        h.coordinator->acceptInvite("tok-9");
        QCOMPARE(h.client->acceptedTokens, (QStringList{"tok-9"}));
        QCOMPARE(h.coordinator->mapId(), QString("m-shared"));
        QCOMPARE(h.transport->joins.last(), QString("m-shared"));
        QCOMPARE(h.client->fetchedMaps, (QStringList{"m-shared"}));
        QFile shareFile(h.engine.documentPath() + ".share");
        QVERIFY(shareFile.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(shareFile.readAll()).object().value("mapId").toString(), QString("m-shared"));
        h.restore();
    }

    void joinSharedMapAttachesJoinsAndHydrates() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");

        h.coordinator->joinSharedMap("m-picked");
        QCOMPARE(h.coordinator->mapId(), QString("m-picked"));
        QCOMPARE(h.transport->joins.last(), QString("m-picked"));
        QCOMPARE(h.client->fetchedMaps, (QStringList{"m-picked"}));
        QFile shareFile(h.engine.documentPath() + ".share");
        QVERIFY(shareFile.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(shareFile.readAll()).object().value("mapId").toString(), QString("m-picked"));
        h.restore();
    }

    void joinedMapHydratesThenLocalEditsQueue() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");

        h.coordinator->joinSharedMap("m-picked");
        QCOMPARE(h.client->fetchedMaps, (QStringList{"m-picked"}));

        Engine peerEngine;
        QVERIFY(peerEngine.loadDocumentBytes(h.fixtureBytes(), {}));
        peerEngine.setText(1, "server text");
        h.coordinator->handleMapStateReadyForTest("m-picked", peerEngine.documentBytes(), 5);
        const QJsonObject node = QJsonDocument::fromJson(h.engine.documentBytes())
                                     .object().value("nodes").toArray().at(0).toObject();
        QCOMPARE(node.value("text").toString(), QString("server text"));
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 0);

        QVERIFY(h.engine.setText(1, "local after join"));
        h.pump(400);
        QCOMPARE(h.transport->submitCount, 1);
        QVERIFY(h.transport->lastCounter >= quint64(1)); // Allocation survives earlier sessions.
        h.restore();
    }

    void refreshSharedMapsPopulatesList() {
        Harness h;
        h.setup();
        QVERIFY(h.loadDocument("doc.omm"));
        h.coordinator->chooseServer("http://localhost:8080");
        h.coordinator->signIn("ada", "secret");
        QSignalSpy changedSpy(h.coordinator.data(), &ShareCoordinator::sharedMapsChanged);
        QCOMPARE(h.coordinator->sharedMaps().size(), 0);

        h.client->fakeMaps = QVariantList{QVariantMap{{"id", "m-1"}, {"owner", "bo"}, {"role", "editor"}},
                                          QVariantMap{{"id", "m-2"}, {"owner", "cy"}, {"role", "viewer"}}};
        h.coordinator->refreshSharedMaps();
        QTRY_COMPARE(changedSpy.count(), 1);
        const QVariantList maps = h.coordinator->sharedMaps();
        QCOMPARE(maps.size(), 2);
        QCOMPARE(maps.first().toMap().value("id").toString(), QString("m-1"));
        QCOMPARE(maps.first().toMap().value("owner").toString(), QString("bo"));
        QCOMPARE(maps.first().toMap().value("role").toString(), QString("editor"));
        QCOMPARE(maps.last().toMap().value("role").toString(), QString("viewer"));
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
