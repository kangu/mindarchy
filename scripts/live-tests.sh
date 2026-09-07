#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$project_dir/build-tests/ui"
cd "$project_dir/build-tests/ui"
qmake6 "$project_dir/tests/ui_test.pro"
make -j2
export MINDMAP_LIVE_TEST_MS="${MINDMAP_LIVE_TEST_MS:-1600}"
exec ./ui_test
