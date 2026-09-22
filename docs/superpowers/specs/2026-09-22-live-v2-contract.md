# Live v2 wire and merge contract

Approved purpose: implement immediate WebSocket editing through Go with fewer full-map CouchDB writes, durable local outboxes, crash recovery and mergeable node edits. Existing v1 maps remain readable. A map switches permanently to liveProtocol=2 on first v2 join; v1 writes are rejected thereafter. No production deployment or database deletion.

## Canonical document and operations
Normalize .omm JSON to {props:{all document top-level fields except nodes/connections},nodes:{stableUid:{all node fields except id/children/syncId; parent:stableUid or ""; order:number}},edges:{fromUid:{toUid:true}}}. Each .omm node has optional syncId string persisted by Qt; existing nodes missing it use "legacy:<integer-id>". New nodes use UUID syncId. Root parent is empty string. Node child order is its index in original parent's children. Project canonical document back to valid .omm with numeric IDs and syncId, parent/children rebuilt from parent references. Cross-links derived from edges. Root must remain singular, parent relationships acyclic. Independent fields merge; simultaneous same-field changes follow server order. Deleting a node removes its descendants and incident edges. Editing a missing node is ignored (deletion wins); arbitrary field changes cannot recreate it. Newly created node is one whole-node set operation; root removal/move and invalid parent/cycle changes are rejected atomically. Validate document and operation limits.

Operations: {op:"set"|"remove",path:["nodes",uid,...fieldPath] or ["props",...fieldPath] or ["edges",fromUid,toUid],value:<json for set>}. Diff recursively traverses objects (arrays treated as field values), omits no-op changes. Whole-node sets are creation only; if existing UID has different content reject identity collision, never overwrite. An edit payload is compact UTF-8 JSON {version:2,id:<UUID>,ops:[operations...]}; SHA256 covers the exact bytes. Qt persists this payload unchanged in local outbox before submitting. IDs survive reconnect/restart.

## Transport
Qt requests /v1/maps/<id>/live?protocol=2. Old server returning hello without protocol=2 is legacy v1 and uses legacy path; never send v2 payload as a v1 snapshot.
Client frame: {type:"edit",operation:<base64 edit payload>,hash:<hex SHA256>}.
Server hello: {type:"hello",protocol:2,mapId,role,epoch:<UUID/random>,revision:<room monotonically increasing revision>,seq:<last durable CouchDB sequence>,state:<base64 canonical JSON>,appliedIds:[<IDs accepted in memory but not durable>]}.
Server applied: {type:"applied",epoch,revision,opId,accountId,state:<base64 current canonical JSON>}. This does not remove local outbox entries.
Server durable: {type:"durable",epoch,revision,seq,receipts:[{id,account,hash,seq}],state:<base64 canonical JSON>}. Remove only matching local payload id+hash for own account. Duplicate durable retries are acknowledged only to sender, without rebroadcasting historical state.
Server rejected: {type:"rejected",code,opId?}; codes include invalid_message, access_revoked, quota_exceeded, resync_required, upgrade_required, storage_unavailable.
Presence retains existing v1 event format and stays ephemeral. Client sends presence unchanged.

## Qt reconciliation
Keep canonical server baseline, live epoch/revision and local optimistic overlay. Capture local changes as operations on Engine::changed, enqueue before transport. Incoming server state is rebased with locally pending operations not already applied in this epoch so remote edits do not erase local work. Also capture edits inside debounce before replacing the visible document. Ignore out-of-order revisions. On hello reset applied IDs to server list and resend unchanged outbox payload IDs; never treat live receipt as durable. Persist account/server/map-scoped identity/counter and pending payloads (existing counter allocation must resume safely). Different maps/accounts must not share pending state. UI labels distinguish Live · saving from Saved. Legacy pending full snapshots remain preserved; do not reinterpret them as operations after promotion.

## Durability and batching
Go room merges operations and broadcasts applied immediately. Flush after max500ms since first pending edit, or100 ops /128KiB of operation bytes; pending limit1000 operations/4MiB and full .omm max1MiB. One immutable batch contains projected full current state and all included operation receipts; CAS head is commit point. Receipt materialization for prior committed batch precedes advancement, as existing crash-safe receipts do. Persist small operation receipts for direct retry lookup; full-map writes coalesced, not literally all per-op document writes eliminated. Batch receipt indexing may use bulk API with individual result checking. Failure retains pending queue, reports storage_unavailable and retries with bounded delay; no false durable ack. Graceful shutdown attempts a bounded flush. Restart recovers through unchanged client outbox IDs. Current initial deployment remains a single Go process per database; no multi-server claim.

## Files and ownership
Go canonical merge: backend/internal/liveops (Normalize([]byte), Apply(canonical, operations), Project(canonical)).
Qt implementation: src/collaboration/liveoperations.{h,cpp}, engine stable IDs, ShareTransport and ShareCoordinator and local store/session tests/build wiring.
Root implementation: rooms live manager/flush, receipt extensions, HTTP v2 integration, real CouchDB tests and docs.

## Authorization and recovery details
Membership is checked at acceptance. Revocation blocks subsequent edits; already authorized accepted edits may finish committing, so a revoked author cannot permanently block the room queue. Hello applied IDs are scoped to the joining account. Remote projection preserves selection, file-save baseline and rebases undo/redo. Legacy snapshot outboxes are exported intact for manual recovery, never silently converted to operations.
