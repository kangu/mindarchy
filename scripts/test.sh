#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# Keep Omarchy's routine pass focused on the model and end-to-end UI smoke
# coverage. The broader platform and component suites remain in CTest/macOS.
for suite in engine ui; do
  build_dir="$project_dir/build-tests/$suite"
  mkdir -p "$build_dir"
  cd "$build_dir"
  qmake6 "$project_dir/tests/${suite}_test.pro"
  make -j2
  QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "./${suite}_test"
done
