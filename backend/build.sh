#!/usr/bin/env bash
# Build native binaries without stopping a running server or running tests.
set -euo pipefail
backend_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$backend_dir"
command -v go >/dev/null 2>&1 || { echo 'Go is required (see go.mod for the version).' >&2; exit 1; }

# Build to a sibling temporary directory, then rename complete binaries into place.
# Existing processes keep their original executable until manually restarted.
staging_dir="$(mktemp -d "$backend_dir/.build.XXXXXX")"
trap 'rm -rf -- "$staging_dir"' EXIT
printf 'Building Go server and maintenance tool…\n'
go build -o "$staging_dir/mindarchy-server" ./cmd/mindarchy-server
go build -o "$staging_dir/mindarchy-maintenance" ./cmd/mindarchy-maintenance
mv -f -- "$staging_dir/mindarchy-server" "$backend_dir/mindarchy-server"
mv -f -- "$staging_dir/mindarchy-maintenance" "$backend_dir/mindarchy-maintenance"
printf 'Built:\n  %s/mindarchy-server\n  %s/mindarchy-maintenance\n' "$backend_dir" "$backend_dir"
printf 'To use this build, stop the old server, then run ./run.sh.\n'
