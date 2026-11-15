#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG_FILE="${1:-serial_com9.log}"

exec python3 -I "$SCRIPT_DIR/realtime_bridge.py" \
  --log-dir "$SCRIPT_DIR" \
  --file "$LOG_FILE" \
  --archive-dir "$SCRIPT_DIR/archive"
