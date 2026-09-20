# Initial Prompt
Evaluate how we could implement sharing mindmaps between different user accounts using central CouchDB infrastructure.
Clarification: sharing must support simultaneous live editing with presence.

# Plan
Inspect application persistence, identity, undo and resource handling. Verify CouchDB authorization and conflict semantics against official documentation. Compare architectures and recommend a staged implementation without changing application behavior.

# Next Steps
Design and validate a live collaboration model with two native clients before implementing full sharing. Evaluate Automerge native integration versus a server-ordered operation model; test concurrent text editing, moves, deletions, undo, reconnection and persistence recovery. Then design accounts, invitations and owner/editor/viewer permissions around the validated collaboration model.

# Implementation Summary
Assessment only; no application code changed or runtime tests performed. Current Mindarchy is a Qt/C++ local-file application with versioned JSON serialization in src/engine.cpp, incrementing integer node IDs, snapshot undo, embedded base64 images and local-file resource references. There is no existing account or synchronization subsystem.

Recommended architecture after clarification: Qt clients connect over HTTPS/WebSocket to an authenticated sharing and live collaboration service, which persists data in private CouchDB. The service enforces owner/editor/viewer permissions for reads, writes, assets, room joins and ongoing room activity. Permission revocation and accepted edits must be serialized per map; stale permission caches and existing sockets must not retain access. Clients cannot write ACL fields through content endpoints. Do not expose the shared database or its raw changes feed to clients.

Use a metadata/ACL document per map, uniquely identified immutable collaboration updates, periodic collaboration-state snapshots and separate image assets. Persist accepted updates before acknowledging durable saves; client outboxes retain unacknowledged updates and retries are deduplicated. Snapshots record the exact covered frontier; retain updates until snapshot recovery is verified. Serialize room ownership or provide fencing when scaling across service instances. Retain local .omm import/export; cloud identity, collaboration state and pending changes require separate account-isolated local persistence. Presence is transient and expires by heartbeat, not CouchDB writes on every movement. Do not treat CouchDB revisions as durable user-facing history.

Whole-map version sharing was considered before clarification but does not satisfy live editing. The recommended direction is CRDT-backed incremental edits plus a WebSocket room service, with Automerge native bindings as a candidate requiring a cross-platform integration probe. Yjs provides documented update and awareness protocols but its native Qt integration also needs evaluation. A server-ordered operation model is an alternative if shared editing is online-only; disconnected editing then needs an explicit rebase/conflict policy. Neither generic CRDT maps nor CouchDB enforce tree invariants automatically.

Alternative: database per map or sharing group with CouchDB _security and write validation; appropriate when direct native replication is a core requirement, but adds provisioning and database lifecycle overhead. A single directly accessible database with per-document ACL fields or filtered replication cannot provide map-level read isolation. Per-user replicated copies complicate bidirectional ownership, reconciliation and revocation.

Live node-level synchronization requires globally unique node identities, defined ordering and delete/move conflict behavior, tree invariant validation and collaboration-aware undo. Existing parent and children fields should project from a single canonical relationship model. Concurrent moves must not introduce cycles; orphan and delete-versus-edit behavior need deterministic rules. Shared text needs character-level editing semantics, including a deliberate rich-text strategy. Existing snapshot undo must not undo another user's changes. Selection, viewport and generally folding stay local; presence can expose selections separately. CouchDB transports data and detects conflicts; it does not implement semantic merging or presence. Revoke future server access promptly while recognizing that previously downloaded files cannot be recalled. Handle local file links explicitly; store shared binary assets separately and preserve portable exports.

Verification for implementation should cover cross-account access denial, viewer writes, unauthorized membership changes, revoked offline writers, concurrent saves, interrupted uploads/retries, account switching, recovery, tree validity, undo and large images. Deployment requires TLS, private database access, secret handling, quotas, compaction and tested backups.

Sources:
- https://docs.couchdb.org/en/stable/api/database/security.html
- https://docs.couchdb.org/en/stable/replication/conflicts.html
- https://docs.couchdb.org/en/stable/api/database/changes.html
- https://docs.couchdb.org/en/stable/api/document/common.html
- https://automerge.org/docs/reference/api/
- https://automerge.org/automerge/automerge/
- https://docs.yjs.dev/api/document-updates
- https://docs.yjs.dev/getting-started/adding-awareness
