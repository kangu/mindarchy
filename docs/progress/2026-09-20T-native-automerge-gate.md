# Native Automerge Compatibility Gate

The Go adapter uses the pinned `github.com/automerge/automerge-go`
`v0.0.0-20241030180337-6fb4f2d08244` cgo binding. Linux verification passed
for full save/load, incremental changes, duplicate delivery, missing-change
exchange, and causal-head convergence.

The bundled C API header is not directly C++-clean: its C enum typedefs
conflict under a C++ compiler. A C translation-unit shim now provides the
ownership boundary, and the C API compiles, links, creates, saves and reloads
a document through the C++ compatibility executable on Linux/amd64.

The binary vector check now runs in both directions: Go-created Automerge
bytes are loaded and inspected by C++, and C++-created bytes are loaded and
inspected by Go. Run `./scripts/test-collaboration-compat.sh` to repeat it.

The Go binding also passes concurrent Unicode text convergence for emoji,
combining marks, and multibyte insertions. Change actor IDs, actor sequence
numbers, dependencies, hashes, and raw serialized bytes are available for an
actor-scoped compensation layer.

This is not yet the full desktop gate. The pinned Go binding has no public
rich-text mark create/query API and no semantic undo or inverse-operation API;
the C header exposes marks, but adding a second unverified Go cgo surface would
violate the dependency gate. Rich-text marks, undo, macOS, and Windows remain
unverified. The shim must become part of the
native adapter and those vectors must pass before desktop integration or
production WebSocket sync is enabled.
