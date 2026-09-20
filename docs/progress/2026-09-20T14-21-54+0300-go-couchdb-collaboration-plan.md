# Initial Prompt

Make an implementation plan for a performant thin Go server coordinating live mindmap sharing in front of central CouchDB. Follow-up confirmed full offline shared editing with automatic reconciliation.

# Plan

Read the repository architecture and planning instructions; verify primary documentation for CouchDB commit semantics, Go transport and native collaboration bindings; write the architecture and an ordered implementation plan; review consistency, dependencies, performance targets and failure cases.

# Next Steps

Review the design and plan. Begin execution with the protocol and native interoperability gate after implementation is requested. Decide an execution method then. Verify Automerge Go/C compatibility, rich-text support and offline undo before implementing the rest of the feature.

# Implementation Summary

Created `docs/superpowers/specs/2026-09-20-go-couchdb-collaboration-design.md` and `docs/superpowers/plans/2026-09-20-go-couchdb-collaboration.md`. The plan has 13 tasks across protocol/merge validation, Go backend, desktop offline integration and release verification. Go owns room coordination, authorization, presence, batching and persistence; private CouchDB stores committed heads, immutable changes, snapshots and assets. Full offline editing uses a shared native CRDT adapter and deterministic tree projection. The proposed Automerge binding uses cgo and has an explicit compatibility gate; it is not claimed to be validated already.

Documented commit-before-acknowledgment, CAS/timeout recovery, bounded queues, transient presence, serialized revocation, invite replay protection, offline draft preservation, platform packaging and measurable performance targets. Initial deployment uses one active Go process; horizontal room ownership is intentionally outside v1. No application code or runtime behavior changed, no deployment performed and no runtime tests claimed. Performed document structure, local link, placeholder and whitespace checks.
