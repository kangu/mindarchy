#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
version=${VERSION:-0.1.0}
if [[ $(uname -s) != Darwin ]]; then
    echo 'macOS build requires macOS. Run this script on your Mac.' >&2
    exit 1
fi
# Additional release-macos.py options can be passed through (e.g. --qt or --arch).
mode=--unsigned
[[ ${MACOS_SIGN:-0} == 1 ]] && mode=--sign
exec python3 "$here/release-macos.py" --version "$version" "$mode" "$@"
