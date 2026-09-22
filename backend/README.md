# Mindarchy collaboration server

Requires Go (version in `go.mod`), a C compiler for the Automerge dependency,
and an existing CouchDB installation/database. Docker is not required.

## Rebuild

```sh
cd qt-prototype/backend
./build.sh
```

Builds `mindarchy-server` and `mindarchy-maintenance` for your current platform,
reusing Go's build cache. No tests, packaging or restart runs automatically.
Scripts also work when invoked by absolute path from another directory.

## Configure and run

If you already export `MINDARCHY_COUCHDB_*`, use those settings directly.
Otherwise, create your local configuration once:

```sh
cp .env.example .env
chmod 600 .env
# Edit .env with your existing database, CouchDB credentials and listen address.
./run.sh
```

You can instead pass `./run.sh /absolute/path/to/server.env`. Configuration files
are trusted shell code; file values override exported variables. `.env` files are
ignored by Git. The CouchDB URL defaults to `http://127.0.0.1:5984`, the database
to `mindarchy-content`, and the listen address to `127.0.0.1:8080`. Keep your
existing database name; rebuilding does not migrate or copy data. The configured
CouchDB account must be allowed to install the server's design document.

The server runs in the foreground. Stop the existing server with Ctrl+C (or your
service manager), then use `./run.sh` to start the rebuilt binary. Rebuilding alone
does not update a running process. If using a service manager, point it to
`backend/mindarchy-server`; the older `mindarchy-server-darwin` filename is not
updated by this script. Run one Go server per database.

## Optional verification

```sh
go test -race ./...
go vet ./...
```

See [collaboration deployment and maintenance](../docs/collaboration-scaling.md)
for the live protocol, maintenance commands and upgrade requirements.
