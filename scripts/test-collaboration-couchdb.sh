#!/usr/bin/env bash
set -euo pipefail

: "${MINDARCHY_TEST_COUCHDB_URL:?Set MINDARCHY_TEST_COUCHDB_URL}"
: "${MINDARCHY_TEST_COUCHDB_USER:?Set MINDARCHY_TEST_COUCHDB_USER}"
: "${MINDARCHY_TEST_COUCHDB_PASSWORD:?Set MINDARCHY_TEST_COUCHDB_PASSWORD}"
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
go_cmd="${GO:-go}"
if ! command -v "$go_cmd" >/dev/null 2>&1 && [[ -x "$HOME/.local/opt/go/bin/go" ]]; then go_cmd="$HOME/.local/opt/go/bin/go"; fi
cd "$project_dir/backend"
"$go_cmd" test -race ./integration -run '^TestRealCouchDBRestartAndDurableWebSocketSubmit$' -count=1 -v
