#!/usr/bin/env bash
set -euo pipefail

###############################################################################
# web.sh — Bitcoin transaction web visualizer
#
# Starts the web visualizer server.
#
# Behavior:
#   - Reads PORT env var (default: 3000)
#   - Prints the URL (e.g., http://127.0.0.1:3000) to stdout
#   - Keeps running until terminated (CTRL+C / SIGTERM)
#   - Must serve GET /api/health -> 200 { "ok": true }
#
###############################################################################

PORT="${PORT:-3000}"

cd "$(dirname "$0")/webapp"

echo "http://127.0.0.1:${PORT}"

export PORT
exec node index.js