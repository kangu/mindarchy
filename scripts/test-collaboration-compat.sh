#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
go_cmd="${GO:-go}"
cmake_cmd="${CMAKE:-cmake}"
ctest_cmd="${CTEST:-ctest}"
if ! command -v "$go_cmd" >/dev/null 2>&1 && [[ -x "$HOME/.local/opt/go/bin/go" ]]; then go_cmd="$HOME/.local/opt/go/bin/go"; fi
if ! command -v "$cmake_cmd" >/dev/null 2>&1 && [[ -x "$HOME/.local/opt/cmake-4.4.3-linux-x86_64/bin/cmake" ]]; then cmake_cmd="$HOME/.local/opt/cmake-4.4.3-linux-x86_64/bin/cmake"; fi
if ! command -v "$ctest_cmd" >/dev/null 2>&1 && [[ -x "$HOME/.local/opt/cmake-4.4.3-linux-x86_64/bin/ctest" ]]; then ctest_cmd="$HOME/.local/opt/cmake-4.4.3-linux-x86_64/bin/ctest"; fi

backend="$project_dir/backend"
automerge_root="$($go_cmd env GOMODCACHE)/github.com/automerge/automerge-go@v0.0.0-20241030180337-6fb4f2d08244"
build_dir="${TMPDIR:-/tmp}/mindarchy-collab-compat-build"
fixture_dir="$(mktemp -d)"
trap 'rm -rf "$fixture_dir"' EXIT

cd "$backend"
"$go_cmd" run ./cmd/collab-fixture -path "$fixture_dir/go.bin"
"$cmake_cmd" -S "$project_dir" -B "$build_dir" \
    -DMINDARCHY_BUILD_COLLABORATION_COMPAT=ON -DAUTOMERGE_ROOT="$automerge_root"
"$cmake_cmd" --build "$build_dir" --target collaboration_compat -j2
AUTOMERGE_GO_FIXTURE="$fixture_dir/go.bin" AUTOMERGE_CPP_FIXTURE="$fixture_dir/cpp.bin" \
    "$ctest_cmd" --test-dir "$build_dir" -R collaboration_compat --output-on-failure
"$go_cmd" run ./cmd/collab-fixture -path "$fixture_dir/cpp.bin" -expect cpp-created
