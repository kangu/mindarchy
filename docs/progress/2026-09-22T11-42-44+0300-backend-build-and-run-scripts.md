# Backend build and run scripts

## Initial Prompt
Rebuild the GO server. Provide quick shell scripts under /backend so i can manually do that

## Plan
1. Inspect Go entry points and configuration.
2. Add portable build/run helpers and local configuration example.
3. Rebuild binaries and verify script behavior.

## Next Steps
Configure backend/.env or export existing MINDARCHY variables; stop the old server and run backend/run.sh. Service managers should launch backend/mindarchy-server rather than the older platform-suffixed binary.

## Implementation Summary
Rebuilt the Go server and maintenance tool successfully as native macOS arm64 binaries. Added executable backend/build.sh and backend/run.sh, ignored local configuration and build outputs, and documented setup in backend/README.md with backend/.env.example. Build uses temporary output followed by rename; run uses foreground exec for graceful signal handling. Verified shell syntax, successful builds/binary architecture, configuration error handling from another working directory, and git diff --check. Existing server and desktop app were not restarted; no CouchDB data was changed.
