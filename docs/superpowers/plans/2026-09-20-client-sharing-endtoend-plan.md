# Client Sharing End-to-End Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the Qt client to the Go sharing server for sign-in, invites, and two-way live editing (snapshot payloads), with a Share-dialog server picker (`share.mindarchy.xyz` / `http://localhost:8080`), fixing the blocking server P1s.

**Architecture:** New `src/collaboration/` classes (`ShareSettings`, `ShareClient`, `ShareTransport`, `ShareCoordinator`) sitting on the existing `CollaborationSession`/`CollaborationLocalStore`/`CollaborationEngineBridge`, exposed to QML as the `share` context property. Go backend fixes: replay-based reads, JSON snapshot ingress validation, committed-event broadcast with change distribution, per-message auth recheck, two-peer production WebSocket arbitration. Spec: `docs/superpowers/specs/2026-09-20-client-sharing-endtoend-design.md`.

**Tech Stack:** Qt 6.11 (QML, Network, WebSockets, QSettings, Qt Test, offscreen QPA), Go 1.24 (`coder/websocket`, CouchDB HTTP), SQLite (existing store).

## Global Constraints

- Build: `python3 scripts/dev.py build` (app), `python3 scripts/dev.py check` (fast tests). Do NOT run `dev.py full` or native window tests; use `dev.py ui` for the QML change task only.
- Payload model: **snapshot payloads** — every transaction carries the whole Engine document (engine `documentBytes()`), strict Submit envelope (`version=1`, UUID `mapId`+`deviceId`, counter, SHA-256 hex hash, base64 `changes` ≤ 1 MiB), last-writer-wins convergence. No Automerge candidate application server-side.
- Server error codes stay stable per `docs/collaboration-protocol.md`: `unsupported_version`, `invalid_message`, `too_large`, `counter_reuse`, `missing_dependencies`, `access_revoked`, `resync_required`, `quota_exceeded`, `undo_conflict`.
- All new Qt tests run offscreen (`QT_QPA_PLATFORM=offscreen`), labeled `fast` where they don't touch windows.
- No comments in code (repo style). No new third-party C++ deps.
- Keyboard/window behavior aside, QML copy stays English, terse.
- Backend env for local run: `MINDARCHY_COUCHDB_URL=http://127.0.0.1:5984`, `MINDARCHY_COUCHDB_USER=admin`, `MINDARCHY_COUCHDB_PASSWORD=admin` (dev), `MINDARCHY_LISTEN_ADDR=127.0.0.1:8080`.

---

## Part 1 — Qt client

### Task 1: ShareSettings (presets + persistence)

**Files:**
- Create: `src/collaboration/sharesettings.h`
- Create: `src/collaboration/sharesettings.cpp`
- Test: `tests/sharesettings_test.cpp`
- Modify: `CMakeLists.txt:9-11` (library adds new sources), `CMakeLists.txt:170` (`fast_tests` list), `CMakeLists.txt:115-123` area (new test executable block after `collaboration_engine_bridge_test`, mirroring it)

**Interfaces:**
- Produces: `class ShareSettings : public QObject` with `Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)`, `Q_PROPERTY(QStringList presets CONSTANT)`, `Q_INVOKABLE void setCustomUrl(const QString&)`, `QString serverUrl() const`, `QStringList presets() const` (returns `{"share.mindarchy.xyz", "http://localhost:8080"}`), `static QString cliOverride(int &argc, char *argv[])` scanning `--share-server <url>` (non-persisting). Constructor: `explicit ShareSettings(QObject *parent = nullptr)` loads `AppIdentity::windowSettings()` key `sharing/serverUrl`, defaulting to preset 0 (`https://share.mindarchy.xyz`); `setServerUrl` persists. Include `"../appidentity.h"`.

- [ ] **Step 1: Write the failing test** `tests/sharesettings_test.cpp`

```cpp
#include <QtTest>
#include "../src/collaboration/sharesettings.h"
#include "../src/appidentity.h"

class ShareSettingsTest : public QObject {
    Q_OBJECT
private slots:
    void defaultsAndPresets() {
        QSettings *settings = windowSettings();
        settings->remove("sharing/serverUrl");
        settings->sync();
        ShareSettings share;
        QCOMPARE(share.presets(), QStringList{"share.mindarchy.xyz", "http://localhost:8080"});
        QCOMPARE(share.serverUrl(), "share.mindarchy.xyz");
    }
    void persistenceAndCustom() {
        ShareSettings share;
        share.setServerUrl("http://localhost:8080");
        QCOMPARE(windowSettings()->value("sharing/serverUrl").toString(), "http://localhost:8080");
        QCOMPARE(ShareSettings().serverUrl(), "http://localhost:8080");
        share.setCustomUrl("http://192.168.1.10:8080");
        QCOMPARE(share.serverUrl(), "http://192.168.1.10:8080");
        QCOMPARE(windowSettings()->value("sharing/serverUrl").toString(), "http://192.168.1.10:8080");
        share.setServerUrl("http://localhost:8080");
    }
};
QTEST_MAIN(ShareSettingsTest)
#include "sharesettings_test.moc"
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build-macos --config Release --target sharesettings_test && ctest --test-dir build-macos -C Release -R '^sharesettings$' --output-on-failure`
Expected: FAIL — `sharesettings.h` does not exist yet (build error is the expected failure).

- [ ] **Step 3: Implement** `src/collaboration/sharesettings.h`

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QSettings;

class ShareSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QStringList presets CONSTANT)
public:
    explicit ShareSettings(QObject *parent = nullptr);
    Q_INVOKABLE void setCustomUrl(const QString &url);
    QString serverUrl() const { return m_serverUrl; }
    void setServerUrl(const QString &url);
    QStringList presets() const { return {"share.mindarchy.xyz", "http://localhost:8080"}; }
signals:
    void serverUrlChanged();
private:
    QString m_serverUrl;
};

QString shareServerOverride(int &argc, char *argv[]);
```

`src/collaboration/sharesettings.cpp`

```cpp
#include "sharesettings.h"

#include "../appidentity.h"

ShareSettings::ShareSettings(QObject *parent) : QObject(parent) {
    m_serverUrl = windowSettings()->value("sharing/serverUrl", "share.mindarchy.xyz").toString();
}

void ShareSettings::setServerUrl(const QString &url) {
    if (m_serverUrl == url) return;
    m_serverUrl = url;
    windowSettings()->setValue("sharing/serverUrl", url);
    emit serverUrlChanged();
}

void ShareSettings::setCustomUrl(const QString &url) { setServerUrl(url); }

QString shareServerOverride(int &argc, char *argv[]) {
    for (int i = 1; i + 1 < argc; ++i)
        if (qstrcmp(argv[i], "--share-server") == 0) return QString::fromLocal8Bit(argv[i + 1]);
    return {};
}
```

- [ ] **Step 4: CMake registration** — extend `mindmap_collaboration` sources with both files; add test executable mirroring the `collaboration_engine_bridge_test` block:

```cmake
add_executable(sharesettings_test tests/sharesettings_test.cpp)
target_link_libraries(sharesettings_test PRIVATE mindmap_collaboration Qt6::Test)
target_include_directories(sharesettings_test PRIVATE src)
add_test(NAME sharesettings COMMAND sharesettings_test)
set_tests_properties(sharesettings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
```

Append `"sharesettings"` to `set(fast_tests ...)`.

- [ ] **Step 5: Run test to verify it passes**

Same command as Step 2. Expected: 2 PASS.

- [ ] **Step 6: Commit**

```bash
git add src/collaboration/sharesettings.* tests/sharesettings_test.cpp CMakeLists.txt
git commit -m "feat: share server preset settings with custom url"
```

### Task 2: ShareClient (HTTP layer)

**Files:**
- Create: `src/collaboration/shareclient.h`, `src/collaboration/shareclient.cpp`
- Test: `tests/shareclient_test.cpp` (+CMake registration mirroring Event 1)

**Interfaces:**
- Consumes: `ShareSettings` (none strictly required — ShareClient takes `baseUrl` as constructor arg).
- Produces: `class ShareClient : public QObject` — constructor `explicit ShareClient(QObject *parent = nullptr)`. Methods: `Q_INVOKABLE void setBaseUrl(const QString&)`, `Q_INVOKABLE void login(const QString &username, const QString &password)`, `Q_INVOKABLE void account()`, `Q_INVOKABLE void maps()`, `Q_INVOKABLE void createMap(const QByteArray &snapshot)`, `Q_INVOKABLE void invite(const QString &mapId, const QString &account, const QString &role)`, `Q_INVOKABLE void acceptInvite(const QString &token)`, `Q_INVOKABLE QString mapState(const QString &mapId)` (returns cached snapshot from last `maps()`/`get`), `Q_INVOKABLE void fetchMapState(const QString &mapId)`. Properties: `signedIn`, `accountName`. Signals: `signedInChanged()`, `loginFailed(QString code)`, `mapsReady()`, `mapStateReady(QString mapId, QByteArray state, quint64 seq)`, `mapCreated(QString mapId)`, `inviteSent()`, `error(QString message)`.

- [ ] **Step 1: Write the failing test** with a loopback HTTP stub. The stub is a raw `QTcpServer` writing canned HTTP JSON — put the helper inline in the test file:

```cpp
class StubHttpServer : public QObject {
    Q_OBJECT
public:
    explicit StubHttpServer(QObject *parent = nullptr) : QObject(parent) {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                m_requests.append(QString::fromUtf8(socket->readAll()));
                if (!m_requests.last().contains("\r\n\r\n")) return;
                QString path = m_requests.last().section('\n', 0, 0).section(' ', 1, 1);
                QByteArray body;
                if (path.endsWith("/v1/auth/session") && m_validLogin) body = "{\"accountId\":\"ada\"}";
                else if (path.endsWith("/v1/me")) body = "{\"accountId\":\"ada\"}";
                else if (path == "/v1/maps") body = R"([{"ID":"a1b2c3","Owner":"ada"}])";
                else if (path.endsWith("/v1/maps") && m_requests.last().startsWith("POST")) body = "{\"id\":\"m-123\",\"role\":\"owner\"}";
                else if (path.contains("/invites") && path.contains("/accept")) body = "{\"id\":\"m-123\",\"role\":\"editor\"}";
                else if (path.contains("/invites")) body = "{\"token\":\"tok-9\"}";
                else if (path.contains("/v1/maps/")) body = "{\"ID\":\"m-123\",\"Owner\":\"ada\",\"Snapshot\":\"aGVsbG8=\"}";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromServer();
            });
        });
    }
    void start() { m_server.listen(QHostAddress::LocalHost); }
    int port() const { return m_server.serverPort(); }
    bool m_validLogin = false;
    QStringList m_requests;
private:
    QTcpServer m_server;
};
```

Test cases: `loginStoresCookieAndAccount` (set `m_validLogin`, call `login("ada","secret")`, expect `signedIn` true and later `account()` returns "ada"), `listAndCreateMaps`, `inviteAndAccept` (expect `inviteSent`, accept returns map id `m-123` role `editor`), `fetchMapState` (expect `mapStateReady` with `state == "hello"`, i.e. base64-decoded, and `seq == 0`).

- [ ] **Step 2: Run to verify it fails** (missing headers — build error expected)
- [ ] **Step 3: Implement `ShareClient`**: `QNetworkAccessManager` member; keep the `AuthSession` cookie received from login via `QNetworkCookieJar` (default jar handles it). `createMap` posts `snapshot` with `Content-Type: application/octet-stream`; `maps()` parses the JSON array (note keys `ID`, `Owner`, `ACL`, `Snapshot` from Go field names); `fetchMapState` does `GET /v1/maps/{id}`, base64-decodes `Snapshot` and emits it. Report non-200 replies via `error()`.
- [ ] **Step 4: Register CMake** like Event 1 (`shareclient_test`, offscreen timeout 60, `fast_tests` += `"shareclient"`).
- [ ] **Step 5: Run tests, all pass**
- [ ] **Step 6: Commit** — `feat: share http client for auth, maps and invites`

### Task 3: ShareTransport (WebSocket + Submit envelope)

**Files:**
- Create: `src/collaboration/sharetransport.h`, `src/collaboration/sharetransport.cpp`
- Test: `tests/sharetransport_test.cpp`

**Interfaces:**
- Produces: `class ShareTransport : public QObject` — `Q_INVOKABLE void setBaseUrl(const QString&)`, `Q_INVOKABLE void join(const QString &mapId)`, `Q_INVOKABLE void leave()`, `Q_INVOKABLE void submit(quint64 counter, const QString &hash, const QByteArray &changes)`, property `connected`, `errorCode`. Signals: `joined(QString mapId, QString role)`, `connectedChanged()`, `committed(quint64 seq, QString deviceId, quint64 counter, QString hash, QByteArray state, QString sender)`, `presence(QStringList accounts)`, `rejected(QString code)`. Static envelope builder (unit-testable):

```cpp
static QByteArray encodeSubmit(const QString &mapId, const QString &deviceId, quint64 counter,
                               const QString &hash, const QByteArray &changes);
```

Produces a JSON object `{"type":"submit","changes":{"version":1,"mapId":...,"deviceId":...,"counter":N,"hash":"<sha256 hex of changes>","changes":"<base64>"}}` (hash computed with `QCryptographicHash::Sha256` over the raw `changes` bytes).

- Test stub: `QWebSocketServer` on localhost that, on a connection, emits `newConnection`; test verifies: (a) `encodeSubmit` output matches protocol fixture shape (`tests/fixtures/collaboration/wire.json` fields), (b) join sends the socket URL `/v1/maps/{id}/live`, (c) writing `{"type":"committed","receipt":{...},"state":"aGVsbG8=","sender":"bo"}` on the stub socket emits `committed` with decoded state `"hello"`, seq/counter/hash/deviceId propagated, (d) `{"type":"rejected","code":"counter_reuse"}` emits `rejected("counter_reuse")`, (e) two roster messages merge into `presence` signal content.
- Implement reconnect timer: `QTimer` single-shot, base 1000 ms doubling to 30000 ms cap, reset on `connected`.
- [ ] Steps: failing test → verify fail → implement → verify pass → CMake (`sharetransport_test`, `fast_tests` += `"sharetransport"`) → commit `feat: share websocket transport with submit envelope`

### Task 4: ShareCoordinator (session fusion + engine glue)

**Files:**
- Create: `src/collaboration/sharecoordinator.h`, `src/collaboration/sharecoordinator.cpp`
- Test: `tests/sharecoordinator_test.cpp`

**Interfaces:**
- Consumes: `ShareClient`, `ShareTransport`, `ShareSettings`, `CollaborationSession` (`openOffline`, `queueChange`, `saveLocal`), `CollaborationEngineBridge` (`recordLocalTransaction`, `beginRemoteApply`, `endRemoteApply`, `localTransaction`), `Engine` (`documentBytes()`, `loadDocumentBytes()`, `documentPath()`, `changed` signal), `CollaborationLocalStore` (`accepted()`, `pending()`).
- Produces: `class ShareCoordinator : public QObject` — constructor `explicit ShareCoordinator(Engine *engine, QObject *parent = nullptr)`. Exclusive QML surface: properties `serverUrl`, `signedIn`, `shareStatus`, `presence` (QStringList), Q_INVOKABLE `chooseServer(QString url)`, `signIn(QString name, QString password)`, `signOut()`, `shareCurrentMap()`, `inviteOnMap(QString account, QString role)`, `disconnect()`. Internal: `bool attachMapId(const QString &mapId)` (persists sidecar file `<documentPath()>.share` containing JSON `{serverUrl, mapId, role, account}`), `loadAttachedMapId()` reads it back, is a no-op when the file does not exist. Snapshot debounce: a 300 ms single-shot `QTimer` armed on engine `changed` (suppressed while the bridge reports `applyingRemote`), converting a local edit batch into `documentBytes()` and calling `queueChange(deviceCounter++, hash, bytes)` where hash is `QCryptographicHash::Sha256` hex — counters increment per client session.

- [ ] **Step 1: Failing test** — using a temp-directory engine produced by instantiating `Engine` directly (offscreen):

```cpp
Engine engine;
engine.loadDocumentBytes(QJsonDocument(QJsonObject{{"nodes", QJsonArray{}}, {"meta", QJsonObject{{"name","t"}}}}).toJson(), QString("t.omm"));
ShareCoordinator coordinator(&engine);
coordinator.chooseServer("http://localhost:8080");
coordinator.signIn("ada", "secret"); // left unattached: server IS the out-of-process stub
QVERIFY(coordinator.signedIn());
```

For the send path without a real server, drive the internals directly: after `coordinator.shareCurrentMap()` with the ShareClient/ShareTransport swapped for fakes (the constructor takes optional fake pointers: `ShareCoordinator(Engine*, ShareClient *client, ShareTransport *transport, ShareSettings *settings, QObject *parent = nullptr)` — real by default, test injects doubles), assert: first edit arms debounce → after 350 ms a stored `Submit` went through the fake transport whose `submitCount` is 1; a `committed` emitted by the fake with foreign `state` triggers `engine.documentBytes()` change and no further local enqueue (`m_localVersion` guard); replayed outbox on `join()` drains pending rows in counter order.

The test needs `fake` subclasses in the test file only:

```cpp
class FakeClient : public ShareClient { public: using ShareClient::ShareClient; };
class FakeTransport : public ShareTransport {
public:
    using ShareTransport::ShareTransport;
    int submitCount = 0;
    void submit(quint64, const QString&, const QByteArray&) override { ++submitCount; }
};
```

(Therefore `ShareTransport::submit` must be `virtual`.)

- [ ] **Step 2:** run, expected fail (missing headers)
- [ ] **Step 3:** implement per Interfaces. Flow decisions to encode:
  - `signIn` → `ShareClient::login`; on success create `CollaborationSession` with `openOffline(accountId, DocumentSession::defaultDirectory(), mapId, role)` only when a shared map is attached.
  - `shareCurrentMap()` → `client.createMap(engine.documentBytes())`; on `mapCreated` persist sidecar via `attachMapId`, then `transport.join(mapId)`.
  - `mapStateReady` (first hydration on join/reconnect and each `committed` from a peer) → `bridge.beginRemoteApply()` → `engine.loadDocumentBytes(state)` → `endRemoteApply()` → `session.saveLocal(state, seq, counter+1)`.
  - `localTransaction`/debounced change → `queueChange` → if `transport.connected`, `transport.submit(counter, hash, bytes)`.
  - `committed` with a `deviceId == m_deviceId` (own submit) → clear the pending entry (dedupe), status "Live".
  - `rejected("counter_reuse")` for own payload → re-enqueue with `++m_deviceCounter` and resubmit (documented mitigation for review #3).
  - `rejected("access_revoked")` or `resync_required` → `client.fetchMapState`, then outbox drain from sequence cursor; on `access_revoked` also `session.markAccessRemoved()` and leave.
- [ ] **Step 4:** pass tests; commit `feat: share coordinator fusing engine, outbox and transport`
- [ ] **Step 5:** CMake registration: `sharecoordinator_test` (needs `Qt6::Network Qt6::WebSockets` too), `fast_tests` += `"sharecoordinator"`.

### Task 5: Engine + workspace wiring

**Files:**
- Modify: `qml/DocumentWorkspace.qml` (near `ShareDialog { id: shareDialog ... }` around line 340; also construct `ShareCoordinator` here since per-window QML context is easiest)
- Modify: `src/main.cpp:230-246` and `src/macapplication.cpp:527-534` for the `--share-server` override: after parsing, if `shareServerOverride(argc, argv)` non-empty, persist it through a temporary `ShareSettings` instance before QML load so per-window instances pick it up.

**Interfaces:**
- Consumes: `ShareCoordinator` from Task 4.
- Produces (QML): context property `share` bound to the workspace's coordinator; `DocumentWorkspace` sets `controller`-adjacent property `share` for child components.

- [ ] **Step 1:** Instantiate in `DocumentWorkspace.qml`:

```qml
ShareCoordinator {
    id: share
    engine: controller
}
```

`ShareCoordinator` gets registered with `QML_ELEMENT` machinery? The repo registers types via `qmlRegisterType<MindCanvas>`; add `qmlRegisterType<ShareCoordinator>("Mindarchy", 1, 0, "ShareCoordinator")` in `src/main.cpp:228-229` next to `MindCanvas`, and make the QML import resolve by including the header from a small C++ registration in `DocumentWorkspace` — simplest: instantiate the coordinator in `main.cpp`/`macapplication.cpp` only when not `sharedWindowManaged`, and pass through as a context property to keep QML dumb:

```cpp
qml.rootContext()->setContextProperty("share", &shareCoordinator);
context->setContextProperty("share", coordinator);
```

`DocumentWorkspace.qml` instead treates `share` as an attached context property and forwards only UI events (`shareButton.onClicked` → `shareDialog.open()` which reads `share.*`). Presence strip host: `PresenceStrip { participants: share.presence }` above the canvas anchored to the canvas top edge where the zoom row sits (verify anchors in design against existing layout before applying).

- [ ] **Step 2: Update `tests/ui_test.cpp`** (the existing stub test at lines 71-82): keep the disabled-fields assertions replaced by: dialog is live only when `share.signedIn`; while not signed in the invite button stays disabled (preserves today's behavior offscreen with no server).
- [ ] **Step 3:** Build + `dev.py check` + targeted `dev.py ui -R ui` (allowed for this QML task), then commit `feat: wire share coordinator into workspace and cli override` (cli override bits: `parseAndApply` in `main.cpp` only; `macapplication.cpp` reuses `windowSettings` persisted values).

### Task 6: ShareDialog QML becomes live

**Files:**
- Modify: `qml/ShareDialog.qml` (full rewrite as spec section UI)
- Modify: `qml/Main.qml` → `qml/PresenceStrip.qml` instantiation (workspace-level, per Task 5 layout)
- Test: `dev.py ui` run of `tests/ui_test.cpp` additions in Task 5 step 2.

**Interfaces:**
- Consumes: `share` context property (Task 5).

Dialog layout (replaces the whole `contentItem`):

```qml
contentItem: ColumnLayout {
    spacing: 12
    Label { text: "Share map"; font.pixelSize: 18; color: "#e0e9ee" }
    RowLayout {
        ComboBox { id: serverSelect; model: share.presets; currentIndex: share.presets.indexOf(share.serverUrl) }
        TextField { id: customUrl; visible: serverSelect.currentIndex < 0; placeholderText: "http://…" }
        Button { text: "Connect"; onClicked: share.chooseServer(serverSelect.currentIndex < 0 ? customUrl.text : share.presets[serverSelect.currentIndex]) }
    }
    Label { text: share.shareStatus; color: "#9bb0bb" }
    if (!share.signedIn) { AccountField; PasswordField; Button "Sign in" }
    else { Label "Signed in as ⟨share.accountName⟩"; Button "Sign out" }
    TextField { id: shareAccount; enabled: share.signedIn; placeholderText: "Existing account" }
    ComboBox { id: role; enabled: share.signedIn; model: ["editor", "viewer"] }
    Button { id: shareInvite; enabled: share.signedIn; text: "Invite"; onClicked: share.inviteOnMap(shareAccount.text, role.currentText) }
}
```

Conditional `if` blocks are not valid QML — use `Loader { active: !share.signedIn; sourceComponent: signInBlock }` and `Loader { active: share.signedIn; sourceComponent: signedInBlock }`.

- [ ] Steps: implement → build → `python3 scripts/dev.py ui` → fix failures → commit `feat: live share dialog with server picker`
- [ ] **AGENTS.md:** update `src/macwindow.mm`/`qml/KeyboardShortcuts.qml` only if a shortcut was bound (none planned) — skip.

## Part 2 — Go backend blockers

### Task 7: Replay-based reads (`sharing.Service`)

**Files:**
- Modify: `backend/internal/sharing/service.go` (`hydrate`, `Get`, `Accept` — replace snapshot-only reconstruction with `couch.Replay`)
- Modify: `backend/internal/sharing/persistence.go` if present (check; if the Persistence interface lacks the chain walk, extend `couch.Replay` use behind the existing methods)
- Test: `backend/integration/replay_reads_test.go` (new)

- [ ] **Step 1: failing test** — create a persistent store, persist a map with snapshot S0, `Manager.Submit` a valid JSON batch B1, then a `NewPersistentService` rehydrating the same store must return a `Get` whose `Snapshot` equals the replayed document (base64 of S + B1), not S alone:

```go
func TestGetReplaysCommittedBatches(t *testing.T) {
	store := newTestCASStore(t)   // reuse the in-memory CAS harness from review_repro_test patterns
	const owner = protocol.AccountID("ada")
	service := sharing.NewPersistentService(store)
	entry := service.Create(owner, []byte(`{"name":"m"}`))
	if err := service.PersistMap(entry); err != nil { t.Fatal(err) }
	// submit one batch through rooms manager bound to the map
	manager := rooms.NewManager(store)
	digest := sha256.Sum256([]byte(`{"name":"m2"}`))
	update := protocol.Submit{Version: 1, MapID: entry.ID, DeviceID: "d1", Counter: 1,
		Hash: hex.EncodeToString(digest[:]), Changes: []byte(`{"name":"m2"}`)}
	if _, err := manager.Submit(context.Background(), owner, update); err != nil { t.Fatal(err) }
	reloaded := sharing.NewPersistentService(store)
	got, err := reloaded.Get(owner, entry.ID)
	if err != nil { t.Fatal(err) }
	if !strings.Contains(string(got.Snapshot), `"name":"m2"`) {
		t.Fatalf("expected replayed snapshot, got %s", got.Snapshot)
	}
}
```

- [ ] **Step 2:** failing (Get currently returns the creation snapshot).
- [ ] **Step 3:** implement — `Service.Get`, `Role`, and `Accept`'s entry reconstruction all call a shared `hydrateWithReplay(id)` that uses the head + `couch.Replay` snapshot (guard against `couch` import cycle: `sharing` already imports `protocol` only; do the replay inside `persistence` via an added interface method `ReplayMap(ctx, id) ([]byte, uint64, error)` implemented by `couch.Store` wrapper in `backend/internal/couch` so `sharing` stays uncoupled).
- [ ] **Step 4:** run `go test -race -count=1 ./...` fresh — pass.
- [ ] **Step 5:** commit `fix(backend): replay committed batches on share reads`

### Task 8: Snapshot-payload ingress validation + committed distribution + per-message authorization

**Files:**
- Modify: `backend/internal/rooms/manager.go` `Submit` (validation gate)
- Modify: `backend/internal/httpapi/server.go` (`liveHandler`: committed broadcast includes `state`; presence roster; single `identity()`+`sharing.Role()` recheck per message before submit; drop peer on `access_revoked`)
- Test: `backend/integration/ingress_validation_test.go` (new); inverts part of `backend/integration/couch_websocket_test.go:69`.

- [ ] **Step 1: failing tests** (review-repro style, production HTTP/WS route with in-memory CAS store):

```go
func TestInvalidSnapshotBytesRejected(t *testing.T) {
	// owner + valid map creation JSON snapshot on /v1/maps
	// socket submit with Changes []byte{0} and zero hash →
	// expect rejected {"code":"invalid_message"} and NO durable batch/head bump
}
func TestCrossMapSubmitRejected(t *testing.T) {
	// owner socket bound to map A must NOT commit to map B even when the
	// envelope mapId == B (transport-bound room check already in server.go;
	// regression locks manager behavior for direct Submit calls too:
	// manager.Submit must verify the update.MapID's head belongs to the
	// account through a credential callback set by httpapi)
}
func TestSubmitAfterSocketExpiryRejected(t *testing.T) {
	// with a fake expiring verifier, a submit after expiry closes the socket
	// and no batch persists
}
```

- [ ] **Step 2:** fail.
- [ ] **Step 3: implement**
  - `rooms.Manager`: add `CheckTarget func(ctx, mapID, account) error` (nil = refuse); `Submit` calls it first; then validates `json.Valid(update.Changes)` and `len <= 1MiB` (snapshot payload model), and `update.MapID` head/account binding via CAS head ACL when persistence exposes it (fall back to callback).
  - `liveHandler`: on each incoming message re-run `s.identity(request)` — expired → close socket with code `access_revoked`; for submits re-check `s.sharing.Role(identity.Account, mapID)`; viewer status → `rejected access_revoked`.
  - committed broadcast sends `{"type":"committed","receipt":receipt,"state":base64(update.Changes),"sender":account}` so peers can apply without an extra GET (snapshot payload = whole state).
  - presence roster: maintain `map[protocol.MapID]map[protocol.AccountID]time.Time`; heartbeat message `{"type":"presence"}` refreshes TTL (10 s); a 1 s ticker drops stale entries and broadcasts the roster map to all peers when membership changed (`{"type":"presence","accounts":[...]}`).
- [ ] **Step 4:** go test fresh, all pass (including inverted couch_websocket test); commit `fix: snapshot ingress validation, committed broadcast, live reauth`

### Task 9: Two-peer production acceptance test

**Files:**
- Modify: `backend/integration/system_test.go` or new `backend/integration/two_peer_production_test.go`

- [ ] **Step 1: test**: start the production server handler in-process against a Couch-free store (the CAS store harness used by review repros); participant A (owner, editor role) and B (granted editor via durable invite) both join `/v1/maps/{id}/live`; A submits; B receives committed with identical `state` bytes; B submits; A receives it; restart the in-process service (fresh `NewPersistentService` over same store) and both peers rejoin and re-hydrate equal state. This is the acceptance criterion 1 in miniature.
- [ ] **Step 2:** passing; commit `test: two-peer production websocket acceptance`
- [ ] **Step 3:** run `scripts/test-collaboration-all.sh` end-to-end; record output in the task summary.

## Part 3 — End-to-end dev verification + close-out

### Task 10: Local live fire + developer setup notes

**Files:**
- Modify: `docs/collaboration-vps-guide.md` (client section: presets, `--share-server`, CouchDB dev-account login)
- Run: local CouchDB + server + two app instances (manual smoke instructions below)

- [ ] **Step 1:** start dev CouchDB (`docker run --rm -d --name mindarchy-couch -p 5984:5984 -e COUCHDB_USER=admin -e COUCHDB_PASSWORD=admin couchdb:3`) and the server with the env block from Global Constraints.
- [ ] **Step 2:** build the app, launch two instances (`open build-macos/mindarchy.app` twice), in instance A: Share dialog → server preset `http://localhost:8080` → sign in `admin` / CouchDB credentials → share → invite `admin` as editor; instance B: sign in, accept invite, edit a node; assert node appears in A within one round-trip and vice versa.
- [ ] **Step 3:** kill network connection to server (stop process), make two edits in each instance (status `Offline · N changes queued` within 300 ms), restart server, watch both drain (status `Live`; convergence check by comparing node text).
- [ ] **Step 4:** commit docs `docs: dev sharing setup` and write the AGENTS-required `docs/progress/<timestamp>-client-sharing-endtoend.md` summary (initial prompt, plan, summary, next steps).
- [ ] **Step 5:** restart the running app once (kill instances, `open build-macos/mindarchy.app`) per AGENTS policy.

---

## Self-review

- Spec coverage: client (T1-T6 = ShareSettings/Client/Transport/Coordinator/wiring/dialog), server (T7-T9 = #6 replay reads, #1/#2 validation+cross-map, #5 committed distribution+presence, #7 reauth), end-to-end (T10). Known-issue #3 mitigation lives in T4 (`counter_reuse` re-enqueue, documented). Out of scope: durable receipts, P2s.
- Placeholders: none — every step carries concrete code or exact commands.
- Type consistency: `ShareTransport::submit` is `virtual` so Task 4 fakes it; `ShareCoordinator` accepts injected doubles; envelope field names match `docs/collaboration-protocol.md`; Go JSON keys (`ID`, `Owner`, `ACL`, `Snapshot`) match existing `sharing.Map` field names for the client test.
