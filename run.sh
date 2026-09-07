#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$(uname -s)" == "Darwin" ]]; then
  executable="$project_dir/build-macos/mindmap-lab.app/Contents/MacOS/mindmap-lab"
  if [[ ! -x "$executable" ]]; then
    qt_prefix="${QT_PREFIX_PATH:-$HOME/Qt/6.11.2/macos}"
    cmake -S "$project_dir" -B "$project_dir/build-macos" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$qt_prefix"
    cmake --build "$project_dir/build-macos" --parallel 4
  fi
else
  executable="$project_dir/build/mindmap-lab"
  if [[ ! -x "$executable" ]]; then
    mkdir -p "$project_dir/build"
    cd "$project_dir/build"
    qmake6 ../prototype.pro
    make -j2
  fi
fi
exec "$executable" "$@"
