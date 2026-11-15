#!/usr/bin/env python3
"""Local read-only bridge for a continuously overwritten gyro log.

Usage:
  python3 -I realtime_bridge.py [--log-dir PATH] [--file serial_com9.log]

The bridge snapshots every file change into archive/ and serves the latest parsed
samples at http://127.0.0.1:8765/state. It uses only the Python standard library.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

NUMBER = re.compile(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?")

class State:
    def __init__(self, source: Path, archive: Path, interval: float):
        self.source = source
        self.archive = archive
        self.interval = interval
        self.lock = threading.Lock()
        self.samples: list[dict[str, float]] = []
        self.version = ""
        self.bytes = 0
        self.updated_at = None
        self.status = "WAITING_FOR_LOG"
        self.last_error = None
        self.total_updates = 0
        self.archive.mkdir(parents=True, exist_ok=True)

    def parse(self, text: str) -> list[dict[str, float]]:
        rows = []
        for line_no, line in enumerate(text.splitlines()):
            values = [float(x) for x in NUMBER.findall(line)]
            if len(values) < 3:
                continue
            # Supported forms:
            #   ax, ay, az
            #   timestamp, ax, ay, az
            #   firmware lines with a leading sequence/timestamp and 6 values
            if len(values) >= 7:
                t, x, y, z = values[0], values[1], values[2], values[3]
            elif len(values) >= 4 and values[0] > 1000:
                t, x, y, z = values[0], values[1], values[2], values[3]
            else:
                t, x, y, z = line_no * 150.0, values[0], values[1], values[2]
            rows.append({"t": t, "x": x, "y": y, "z": z})
        return rows

    def snapshot(self) -> None:
        try:
            if not self.source.exists():
                with self.lock:
                    self.status = "WAITING_FOR_LOG"
                return
            raw = self.source.read_bytes()
            digest = hashlib.sha1(raw).hexdigest()
            with self.lock:
                if digest == self.version:
                    return
            # Keep every observed version locally because the device overwrites the source.
            stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
            copy_path = self.archive / f"{self.source.stem}_{stamp}{self.source.suffix}"
            copy_path.write_bytes(raw)
            text = raw.decode("utf-8", errors="replace")
            rows = self.parse(text)
            with self.lock:
                self.samples = rows
                self.version = digest
                self.bytes = len(raw)
                self.updated_at = datetime.now(timezone.utc).isoformat()
                self.status = "LIVE" if rows else "NO_SAMPLES"
                self.last_error = None
                self.total_updates += 1
        except Exception as exc:  # keep the bridge alive while the file is being rewritten
            with self.lock:
                self.status = "READ_RETRY"
                self.last_error = str(exc)

    def watch(self) -> None:
        while True:
            self.snapshot()
            time.sleep(self.interval)

    def payload(self) -> dict:
        with self.lock:
            return {
                "source": str(self.source),
                "status": self.status,
                "updatedAt": self.updated_at,
                "bytes": self.bytes,
                "updates": self.total_updates,
                "lastError": self.last_error,
                "samples": list(self.samples),
            }

class Handler(BaseHTTPRequestHandler):
    state: State
    def do_GET(self):
        if self.path == "/state":
            body = json.dumps(self.state.payload(), ensure_ascii=False).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/health":
            body = b'{"ok":true}'
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_error(404)
    def log_message(self, *_):
        return

def main() -> None:
    p = argparse.ArgumentParser()
    default_dir = Path(__file__).resolve().parent
    p.add_argument(
        "--log-dir",
        default=str(default_dir),
        help="Folder containing the overwritten log (default: this script's folder)",
    )
    p.add_argument("--file", default="serial_com9.log", help="Log filename")
    p.add_argument(
        "--archive-dir",
        default=str(default_dir / "archive"),
        help="Folder for local snapshots (default: ./archive)",
    )
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8765)
    p.add_argument("--interval", type=float, default=0.35)
    args = p.parse_args()
    source = Path(args.log_dir).expanduser().resolve() / args.file
    archive = Path(args.archive_dir).expanduser().resolve()
    state = State(source, archive, args.interval)
    State.snapshot(state)
    threading.Thread(target=state.watch, daemon=True).start()
    Handler.state = state
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"Watching: {source}")
    print(f"Snapshots: {archive}")
    print(f"State API: http://{args.host}:{args.port}/state")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped")
        server.server_close()

if __name__ == "__main__":
    main()
