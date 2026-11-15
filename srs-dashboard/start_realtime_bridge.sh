#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="${1:-serial_com9.log}"
PYTHON_BIN="${PYTHON_BIN:-python3}"

exec "$PYTHON_BIN" -I "$SCRIPT_DIR/realtime_bridge.py" \
  --log-dir "$SCRIPT_DIR" \
  --file "$LOG_FILE" \
  --archive-dir "$SCRIPT_DIR/archive"
