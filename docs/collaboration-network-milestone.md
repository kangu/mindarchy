# Collaboration Network Milestone

## Goal

Run two independent Mindarchy test peers against the same map over a network
connection. Each peer must submit a local change, receive the other peer's
committed change exactly once, and resume from a sequence cursor after polling
again.

## Current checkpoint

`backend/integration/network_test.go` uses two independent HTTP clients and a
real loopback TCP listener. It verifies concurrent submissions, committed
sequence assignment, delivery to both peers, duplicate-poll suppression, and
the existing protocol hash/size validation.

Run it with:

```sh
./scripts/test-collaboration.sh
```

This is a transport and commit-order checkpoint, not the finished sharing
feature. Payloads remain opaque until the native Automerge compatibility gate
passes. The next checkpoint replaces the test-only HTTP route with the
authenticated WebSocket session and applies compatible CRDT changes to the
desktop document projection.
