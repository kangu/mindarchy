# Collaboration Protocol v1

The desktop sends durable CRDT changes to the Go service over HTTPS or the
authenticated WebSocket session. The server assigns the authenticated account;
an account identity must never be accepted from a client envelope.

## Submit envelope

`Submit` is JSON with exactly these fields:

| Field | Type | Requirement |
|---|---|---|
| `version` | integer | Must be `1` |
| `mapId` | UUID string | Target map |
| `deviceId` | UUID string | Stable per account/device/session actor |
| `counter` | positive integer | Monotonic per account/map/device |
| `hash` | lowercase SHA-256 hex | Digest of decoded `changes` |
| `changes` | base64 string | Non-empty CRDT update, at most 1 MiB decoded |

Unknown or duplicate fields, trailing JSON, malformed IDs, empty updates and
hash mismatches are rejected before native collaboration decoding. Base64 is
standard padded encoding. The encoded payload is bounded before decoding.

## Outcomes

Errors use a stable machine-readable code: `unsupported_version`,
`invalid_message`, `too_large`, `counter_reuse`, `missing_dependencies`,
`access_revoked`, `resync_required`, `quota_exceeded`, or `undo_conflict`.
The authenticated account, authorization decision and map head are server-side
state and are not decoded from `Submit`.

An accepted update is not considered committed until its immutable batch is
reachable from the map head in CouchDB. A receipt contains the device ID,
counter, hash and committed sequence. Retrying the same device/counter/hash is
idempotent; reusing a counter with different bytes is a `counter_reuse` error.

The JSON envelope is separate from causal sync messages. Receiving a sync
message never acknowledges or persists an edit.

## Live WebSocket messages

On the production `/v1/maps/{mapId}/live` socket, the server sends `hello`
followed by the current roster, then broadcasts to every peer in the room,
including the sender.

`committed` carries the receipt plus the committed change bytes so peers apply
the state without an extra fetch:

| Field | Type | Meaning |
|---|---|---|
| `type` | string | `committed` |
| `receipt` | object | Device ID, counter, hash and committed sequence |
| `state` | base64 string | Committed change bytes (snapshot payload) |
| `sender` | string | Authenticated account that submitted the change |

`presence` carries the room roster. Any incoming message from a peer refreshes
its presence TTL (10 seconds); a background sweep broadcasts the updated
roster whenever membership changes, and the joining peer receives it right
after `hello`:

| Field | Type | Meaning |
|---|---|---|
| `type` | string | `presence` |
| `accounts` | string array | Sorted live account IDs in the room |

Identity and role are re-verified on every received message; a failed check
answers `rejected` with code `access_revoked` and closes the socket.

## Two-peer network milestone

The first executable milestone uses a loopback HTTP transport under
`/v1/test/maps/{mapId}/changes`. It is a test harness, not the production
sharing API: it accepts an authenticated test-account header, commits opaque
change payloads in sequence, and lets two peers poll committed events. The
integration test proves that two independent clients on a network connection
receive each other's committed changes exactly once and can resume from a
sequence cursor.

It intentionally does not merge opaque payloads. Native Automerge
interoperability and deterministic map projection must pass before this
harness becomes the production WebSocket/CRDT path.
