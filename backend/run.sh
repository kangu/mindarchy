#!/usr/bin/env bash
# Run in the foreground; Ctrl+C gives the server a graceful shutdown.
set -euo pipefail
backend_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ $# -gt 1 ]]; then
    echo 'Usage: ./run.sh [path/to/trusted.env]' >&2
    exit 2
fi
# Resolve an explicitly supplied relative path against the caller's directory.
env_file="${1:-$backend_dir/.env}"
if [[ $# -eq 1 && ! -f "$env_file" ]]; then
    echo 'The specified environment file does not exist.' >&2
    exit 1
fi
if [[ -f "$env_file" ]]; then
    set -a
    # This is a trusted, shell-compatible configuration file, not arbitrary input.
    source "$env_file"
    set +a
fi
export MINDARCHY_COUCHDB_URL="${MINDARCHY_COUCHDB_URL:-http://127.0.0.1:5984}"
: "${MINDARCHY_COUCHDB_USER:?Set MINDARCHY_COUCHDB_USER in your environment or backend/.env}"
: "${MINDARCHY_COUCHDB_PASSWORD:?Set MINDARCHY_COUCHDB_PASSWORD in your environment or backend/.env}"
if [[ ! -x "$backend_dir/mindarchy-server" ]]; then
    echo 'Server binary missing. Run backend/build.sh first.' >&2
    exit 1
fi
cd "$backend_dir"
exec "$backend_dir/mindarchy-server"
