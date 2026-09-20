# Mindarchy Sharing on a VPS

This guide starts the current Go sharing server with CouchDB on a Linux VPS.
It is intended for a private test run with one or two accounts.

The current server supports:

- CouchDB `_session` login and secure HTTP session cookies.
- OIDC bearer authentication when configured.
- Map creation and ACLs for owner, editor, and viewer roles.
- Invitations and invitation acceptance.
- Authenticated WebSocket connections.
- Durable map snapshots, map heads, immutable batches, and restart hydration.

The desktop Qt client is not yet wired to these production endpoints. Use the
HTTP/WebSocket integration test or a small API client for the first VPS test.
Invitation records and some room metadata still need durable persistence before
this should be treated as a public production service.

## 1. VPS Requirements

Use a current Ubuntu or Debian VPS with:

- 2 vCPUs and 4 GB RAM for a small test deployment.
- A DNS name such as `share.mindarchy.xyz`.
- Ports 22, 80, and 443 open. Do not expose CouchDB publicly.
- A non-root deployment user with Docker access or permission to run systemd services.

Point the DNS A/AAAA record at the VPS before enabling TLS.

## Local CouchDB Debugging

For local development, run CouchDB on `localhost` with a disposable Docker
volume. This keeps debugging independent from the VPS database:

```sh
docker volume create mindarchy-couchdb-debug
docker run -d \
  --name mindarchy-couchdb-debug \
  -p 127.0.0.1:5984:5984 \
  -e COUCHDB_USER=debug_admin \
  -e COUCHDB_PASSWORD=debug_password \
  -e COUCHDB_SECRET=debug_cookie_secret \
  -e COUCHDB_SINGLE_NODE=true \
  -v mindarchy-couchdb-debug:/opt/couchdb/data \
  couchdb:3
```

Wait for CouchDB and create a local content database:

```sh
until curl --silent --fail --user debug_admin:debug_password \
  http://127.0.0.1:5984/; do sleep 1; done

curl --fail --user debug_admin:debug_password \
  -X PUT http://127.0.0.1:5984/mindarchy-content
```

Run the Go server from the repository with a local environment file:

```sh
cd backend
export MINDARCHY_LISTEN_ADDR=127.0.0.1:8080
export MINDARCHY_COUCHDB_URL=http://127.0.0.1:5984
export MINDARCHY_COUCHDB_DATABASE=mindarchy-content
export MINDARCHY_COUCHDB_USER=debug_admin
export MINDARCHY_COUCHDB_PASSWORD=debug_password
go run ./cmd/mindarchy-server
```

For CouchDB `_session` debugging, create separate users through the CouchDB
Fauxton UI at `http://127.0.0.1:5984/_utils/`, then exercise the server:

```sh
curl --fail --cookie-jar /tmp/mindarchy-debug-cookie.txt \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","password":"ALICE_PASSWORD"}' \
  http://127.0.0.1:8080/v1/auth/session

curl --fail --cookie /tmp/mindarchy-debug-cookie.txt \
  http://127.0.0.1:8080/v1/me
```

Run the real-CouchDB integration test against this local instance:

```sh
export MINDARCHY_TEST_COUCHDB_URL=http://127.0.0.1:5984
export MINDARCHY_TEST_COUCHDB_USER=debug_admin
export MINDARCHY_TEST_COUCHDB_PASSWORD=debug_password
./scripts/test-collaboration-couchdb.sh
```

Inspect or reset the disposable instance:

```sh
docker logs mindarchy-couchdb-debug
docker stop mindarchy-couchdb-debug
docker rm mindarchy-couchdb-debug
docker volume rm mindarchy-couchdb-debug
```

Do not reuse the debug password or publish port `5984` outside localhost.

## 2. Install Dependencies

Install Git, Go, Docker, and a reverse proxy. Example for Ubuntu:

```sh
sudo apt update
sudo apt install -y git curl ca-certificates docker.io caddy
sudo systemctl enable --now docker caddy
```

Install Go 1.25 or newer, or use the project-supported Go toolchain. Verify:

```sh
go version
docker version
```

## 3. Start Private CouchDB

Create a private Docker network and a persistent volume:

```sh
sudo docker network create mindarchy-private || true
sudo docker volume create mindarchy-couchdb-data
```

Start CouchDB. Replace the password with a long random value:

```sh
sudo docker run -d \
  --name mindarchy-couchdb \
  --restart unless-stopped \
  --network mindarchy-private \
  -e COUCHDB_USER=server_admin \
  -e COUCHDB_PASSWORD='REPLACE_WITH_RANDOM_PASSWORD' \
  -e COUCHDB_SECRET='REPLACE_WITH_ANOTHER_RANDOM_SECRET' \
  -e NODENAME=couchdb@127.0.0.1 \
  -e COUCHDB_SINGLE_NODE=true \
  -v mindarchy-couchdb-data:/opt/couchdb/data \
  couchdb:3
```

Create the content database from the VPS host:

```sh
curl --fail --user server_admin:REPLACE_WITH_RANDOM_PASSWORD \
  -X PUT http://127.0.0.1:5984/mindarchy-content
```

If CouchDB is reachable only inside Docker, use the container name from the
server container instead of `127.0.0.1`.

Do not publish port `5984` with `-p`. The Go server should be the only service
that can reach CouchDB from outside the private Docker network.

## 4. Obtain and Build Mindarchy

```sh
sudo mkdir -p /opt/mindarchy
sudo chown "$USER":"$USER" /opt/mindarchy
git clone https://github.com/OWNER/mindarchy.git /opt/mindarchy
cd /opt/mindarchy/backend
go mod download
mkdir -p /opt/mindarchy/bin
go build -o /opt/mindarchy/bin/mindarchy-server ./cmd/mindarchy-server
```

Replace the repository URL with the actual Mindarchy repository URL.

## 5. Configure the Server

Create a root-readable environment file:

```sh
sudo install -d -m 0750 /etc/mindarchy
sudo sh -c 'umask 077; touch /etc/mindarchy/server.env'
sudo chmod 0600 /etc/mindarchy/server.env
```

Set values similar to:

```ini
MINDARCHY_LISTEN_ADDR=127.0.0.1:8080
MINDARCHY_COUCHDB_URL=http://127.0.0.1:5984
MINDARCHY_COUCHDB_DATABASE=mindarchy-content
MINDARCHY_COUCHDB_USER=server_admin
MINDARCHY_COUCHDB_PASSWORD=REPLACE_WITH_RANDOM_PASSWORD

# Optional OIDC. Leave both unset when using CouchDB _session login.
# MINDARCHY_OIDC_ISSUER=https://identity.example.com
# MINDARCHY_OIDC_AUDIENCE=mindarchy

MINDARCHY_READ_TIMEOUT=15s
MINDARCHY_WRITE_TIMEOUT=15s
MINDARCHY_IDLE_TIMEOUT=60s
```

The current server requires CouchDB credentials. Keep this file out of Git and
do not pass secrets in command-line arguments.

## 6. Run with systemd

Create `/etc/systemd/system/mindarchy-server.service`:

```ini
[Unit]
Description=Mindarchy sharing server
After=network-online.target docker.service
Wants=network-online.target

[Service]
Type=simple
User=mindarchy
Group=mindarchy
WorkingDirectory=/opt/mindarchy/backend
EnvironmentFile=/etc/mindarchy/server.env
ExecStart=/opt/mindarchy/bin/mindarchy-server
Restart=on-failure
RestartSec=3
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/mindarchy

[Install]
WantedBy=multi-user.target
```

Create the service user and start it:

```sh
sudo useradd --system --home /var/lib/mindarchy --shell /usr/sbin/nologin mindarchy || true
sudo mkdir -p /var/lib/mindarchy
sudo chown -R mindarchy:mindarchy /var/lib/mindarchy /opt/mindarchy
sudo systemctl daemon-reload
sudo systemctl enable --now mindarchy-server
sudo journalctl -u mindarchy-server -f
```

For a Docker-only deployment, place the Go server and CouchDB on
`mindarchy-private` and set `MINDARCHY_COUCHDB_URL=http://mindarchy-couchdb:5984`.

## 7. Add HTTPS and WebSockets

Caddy can obtain and renew a certificate automatically. Create
`/etc/caddy/Caddyfile`:

```caddyfile
share.mindarchy.xyz {
    reverse_proxy 127.0.0.1:8080
}
```

Apply it:

```sh
sudo caddy validate --config /etc/caddy/Caddyfile
sudo systemctl reload caddy
curl --fail https://share.mindarchy.xyz/healthz
curl --fail https://share.mindarchy.xyz/readyz
```

The reverse proxy must support HTTP/1.1 WebSocket upgrades. Never expose the
server over plain HTTP outside localhost.

## 8. Create CouchDB Test Accounts

For the current `_session` login flow, create users in CouchDB's `_users`
database using CouchDB's admin interface or a controlled provisioning tool.
Do not use `server_admin` as an end-user account.

Verify a test login directly:

```sh
curl --fail --cookie-jar /tmp/mindarchy-cookie.txt \
  -H 'Content-Type: application/json' \
  -d '{"name":"alice","password":"REPLACE_WITH_ALICE_PASSWORD"}' \
  https://share.mindarchy.xyz/v1/auth/session
```

Then check the authenticated identity:

```sh
curl --fail --cookie /tmp/mindarchy-cookie.txt \
  https://share.mindarchy.xyz/v1/me
```

Create a second account, such as `bob`, for the sharing test. Use unique,
long passwords and delete test accounts after the trial.

## 9. Run the Real Integration Test

From a checkout with the Go toolchain installed:

```sh
export MINDARCHY_TEST_COUCHDB_URL=http://127.0.0.1:5984
export MINDARCHY_TEST_COUCHDB_USER=server_admin
export MINDARCHY_TEST_COUCHDB_PASSWORD='REPLACE_WITH_RANDOM_PASSWORD'
./scripts/test-collaboration-couchdb.sh
```

The test creates a uniquely named database, persists a map, replaces the
application service to simulate a restart, connects over WebSocket, submits a
change, verifies the durable head and batch, and deletes the test database.

Run the local checks as well:

```sh
./scripts/test-collaboration-all.sh
```

## 10. Backups and Logs

Back up the CouchDB data volume before every server upgrade. At minimum:

```sh
sudo docker exec mindarchy-couchdb couchbackup \
  --url http://server_admin:REPLACE_WITH_RANDOM_PASSWORD@127.0.0.1:5984 \
  --db mindarchy-content \
  --output /var/backups/mindarchy-content-$(date +%F).json
```

Use an encrypted off-host backup destination and test restoring into a fresh
CouchDB instance. Inspect logs without printing credentials:

```sh
sudo journalctl -u mindarchy-server --since today
sudo docker logs mindarchy-couchdb --since 1h
```

## Current Test-Run Limitations

- Map metadata and initial snapshots persist to CouchDB.
- Room submissions persist immutable batches and advance map heads.
- Invitation records are not yet durable across server restart.
- WebSocket fanout is not yet a complete multi-client committed event stream.
- Asset storage, snapshots/compaction, quotas, and recovery policies are incomplete.
- The Qt desktop client is not yet connected to these endpoints.
- Do not advertise this as a production public collaboration service until the
  remaining persistence, authorization, backup, and two-client tests pass.
