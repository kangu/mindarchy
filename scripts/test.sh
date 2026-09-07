#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
for suite in engine canvas ui; do
  build_dir="$project_dir/build-tests/$suite"
  mkdir -p "$build_dir"
  cd "$build_dir"
  qmake6 "$project_dir/tests/${suite}_test.pro"
  make -j2
  QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "./${suite}_test"
done
