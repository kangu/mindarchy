# Initial Prompt

Evaluate completeness and correctness of the current implementation against `docs/superpowers/plans/2026-09-20-go-couchdb-collaboration.md`.

# Plan

Inspect tracked and untracked implementation and the referenced design. Review backend persistence/auth separately from protocol/native integration. Run existing tests fresh, reproduce critical defects in an isolated temporary source copy, and map implementation evidence to all 13 plan tasks.

# Next Steps

Fix production map authorization and ingress validation; complete the native compatibility gate; implement durable receipts/permissions, authoritative replay and production fanout. Continue with Qt offline storage and editing only after backend and merge gates pass.

# Implementation Summary

Created `docs/collaboration-implementation-review-2026-09-20.md`. The implementation is an incomplete internal prototype with a partial Go backend and Linux Automerge compatibility experiment; it does not satisfy live sharing or offline desktop requirements. Identified nine correctness findings, including cross-map unauthorized writes, malformed-update acceptance, restart idempotency failure, lost sharing grants, missing production peer delivery and stale snapshots.

Fresh Go race tests passed after allowing local loopback test sockets; real CouchDB testing skipped because test connection settings were absent. Native Linux C/C++ compatibility passed. Isolated temporary repros demonstrate the critical authorization/persistence defects. No application/backend implementation changes or deployments were made. No performance or full desktop/platform validation is claimed.
