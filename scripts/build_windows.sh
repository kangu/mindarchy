#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
version=${VERSION:-0.1.0}
: "${WINDOWS_QT_DIR:?Set WINDOWS_QT_DIR, e.g. C:\\Qt\\6.11.2\\msvc2022_64}"
case $(uname -s) in
    MINGW*|MSYS*|CYGWIN*)
        exec powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$here/release-windows.ps1")" -QtDir "$WINDOWS_QT_DIR" -Version "$version" "$@"
        ;;
    *)
        : "${WINDOWS_HOST:?Set WINDOWS_HOST to a Windows SSH build machine, or run this script in Git Bash on Windows}"
        : "${WINDOWS_SOURCE:?Set WINDOWS_SOURCE to its current qt-prototype checkout (Windows path)}"
        # EncodedCommand avoids cmd.exe/OpenSSH quoting differences. Values are
        # PowerShell single-quoted literals, not executable shell fragments.
        command=$(python3 - "$WINDOWS_SOURCE" "$WINDOWS_QT_DIR" "$version" <<'PY'
import base64, sys
quote=lambda s: "'"+s.replace("'", "''")+"'"
source, qt, version=sys.argv[1:]
script="& "+quote(source.rstrip('\\/')+'\\scripts\\release-windows.ps1')+' -QtDir '+quote(qt)+' -Version '+quote(version)
script="$ErrorActionPreference='Stop'; try { "+script+"; exit 0 } catch { Write-Error $_; exit 1 }"
print(base64.b64encode(script.encode('utf-16le')).decode())
PY
)
        exec ssh "$WINDOWS_HOST" "powershell.exe -NoProfile -ExecutionPolicy Bypass -EncodedCommand $command"
        ;;
esac
