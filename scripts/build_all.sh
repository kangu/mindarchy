#!/usr/bin/env bash
set -uo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# Continue after a failed platform so one missing host does not hide other results.
failed=0
for platform in macos omarchy windows; do
    printf '\nBuilding %s installer (version %s)\n' "$platform" "${VERSION:-0.1.0}"
    if bash "$here/build_$platform.sh"; then
        printf '%s: SUCCESS\n' "$platform"
    else
        printf '%s: FAILED (see output above)\n' "$platform" >&2
        failed=1
    fi
done
exit "$failed"
