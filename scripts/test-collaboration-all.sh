#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
go_cmd="${GO:-go}"
if ! command -v "$go_cmd" >/dev/null 2>&1 && [[ -x "$HOME/.local/opt/go/bin/go" ]]; then go_cmd="$HOME/.local/opt/go/bin/go"; fi

cd "$project_dir/backend"
"$go_cmd" test -race ./...
cd "$project_dir"
./scripts/test-collaboration-compat.sh
