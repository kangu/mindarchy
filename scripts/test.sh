#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# Quiet fast tests by default. Opt into UI/native checks with dev.py ui/full.
exec python3 "$project_dir/scripts/dev.py" check "$@"
