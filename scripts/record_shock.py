#!/usr/bin/env python3
"""
Team-04 Continuous Serial Logger & IMU Telemetry Recorder
Target: STM32G474RE Nucleo (ST-LINK VCP UART)
Config: COM10 @ 230400 baud, 8-N-1

Features:
- Dual-Backend: Uses pyserial if installed, or native Windows Win32 API (ctypes) with ZERO dependencies!
- Auto-Wait on Port Busy: If another program (e.g. Serial Monitor) is holding COM10 open,
  it waits until you disconnect it and automatically attaches.
- Zero memory leakage (streams straight to disk).
- Periodic disk flush.
"""

import sys
import os
import time
import argparse
import datetime

# Try importing pyserial, otherwise fallback to native Win32 ctypes
USE_PYSERIAL = False
try:
    import serial
    import serial.tools.list_ports
    USE_PYSERIAL = True
except ImportError:
    USE_PYSERIAL = False

# Native Win32 ctypes Serial Driver (Zero external dependencies on Windows)
if not USE_PYSERIAL:
    import ctypes
    from ctypes import wintypes

    class DCB(ctypes.Structure):
        _fields_ = [
            ("DCBlength", wintypes.DWORD),
            ("BaudRate", wintypes.DWORD),
            ("flags", wintypes.DWORD),
            ("wReserved", wintypes.WORD),
            ("XonLim", wintypes.WORD),
            ("XoffLim", wintypes.WORD),
            ("ByteSize", wintypes.BYTE),
            ("Parity", wintypes.BYTE),
            ("StopBits", wintypes.BYTE),
            ("XonChar", ctypes.c_char),
            ("XoffChar", ctypes.c_char),
            ("ErrorChar", ctypes.c_char),
            ("EofChar", ctypes.c_char),
            ("EvtChar", ctypes.c_char),
            ("wReserved1", wintypes.WORD),
        ]

    class COMMTIMEOUTS(ctypes.Structure):
        _fields_ = [
            ("ReadIntervalTimeout", wintypes.DWORD),
            ("ReadTotalTimeoutMultiplier", wintypes.DWORD),
            ("ReadTotalTimeoutConstant", wintypes.DWORD),
            ("WriteTotalTimeoutMultiplier", wintypes.DWORD),
            ("WriteTotalTimeoutConstant", wintypes.DWORD),
        ]

    class NativeWin32Serial:
        def __init__(self, port: str, baudrate: int = 230400, timeout: float = 1.0):
            self.port = port
            self.baudrate = baudrate
            self.timeout = timeout
            self.port_path = f"\\\\.\\{port}" if not port.startswith("\\\\.\\") else port
            self.handle = None
            self._buffer = bytearray()
            self._open()

        def _open(self):
            # GENERIC_READ (0x80000000) | GENERIC_WRITE (0x40000000) = 0xC0000000
            self.handle = ctypes.windll.kernel32.CreateFileW(
                self.port_path,
                0xC0000000,
                0,     # Exclusive access
                None,
                3,     # OPEN_EXISTING
                0x80,  # FILE_ATTRIBUTE_NORMAL
                None
            )
            if self.handle == -1 or self.handle == 0xFFFFFFFFFFFFFFFF:
                err = ctypes.GetLastError()
                self.handle = None
                raise OSError(err, f"CreateFileW failed for {self.port} (WinError {err})")

            dcb = DCB()
            dcb.DCBlength = ctypes.sizeof(DCB)
            config_str = f"baud={self.baudrate} parity=N data=8 stop=1"
            if not ctypes.windll.kernel32.BuildCommDCBW(config_str, ctypes.byref(dcb)):
                err = ctypes.GetLastError()
                self.close()
                raise OSError(err, f"BuildCommDCBW failed: {err}")

            dcb.flags = 1  # fBinary = 1
            if not ctypes.windll.kernel32.SetCommState(self.handle, ctypes.byref(dcb)):
                err = ctypes.GetLastError()
                self.close()
                raise OSError(err, f"SetCommState failed: {err}")

            # Buffer 64KB RX
            ctypes.windll.kernel32.SetupComm(self.handle, 65536, 8192)

            timeouts = COMMTIMEOUTS()
            timeouts.ReadIntervalTimeout = 50
            timeouts.ReadTotalTimeoutMultiplier = 0
            timeouts.ReadTotalTimeoutConstant = int(self.timeout * 1000)
            timeouts.WriteTotalTimeoutMultiplier = 0
            timeouts.WriteTotalTimeoutConstant = 1000
            ctypes.windll.kernel32.SetCommTimeouts(self.handle, ctypes.byref(timeouts))
            ctypes.windll.kernel32.PurgeComm(self.handle, 0x0001 | 0x0002 | 0x0004 | 0x0008)

        def read(self, size: int = 4096) -> bytes:
            if not self.handle:
                return b""
            buf = ctypes.create_string_buffer(size)
            n_read = wintypes.DWORD()
            success = ctypes.windll.kernel32.ReadFile(
                self.handle, buf, size, ctypes.byref(n_read), None
            )
            if success and n_read.value > 0:
                return buf.raw[:n_read.value]
            return b""

        def readline(self) -> bytes:
            start_t = time.time()
            while b"\n" not in self._buffer:
                chunk = self.read(4096)
                if chunk:
                    self._buffer.extend(chunk)
                else:
                    if (time.time() - start_t) >= self.timeout:
                        break
                    time.sleep(0.002)

            if b"\n" in self._buffer:
                idx = self._buffer.index(b"\n")
                line = self._buffer[:idx + 1]
                self._buffer = self._buffer[idx + 1:]
                return bytes(line)
            elif self._buffer:
                line = bytes(self._buffer)
                self._buffer.clear()
                return line
            return b""

        def close(self):
            if self.handle is not None and self.handle != -1:
                ctypes.windll.kernel32.CloseHandle(self.handle)
                self.handle = None


def format_bytes(num_bytes: int) -> str:
    for unit in ["B", "KB", "MB", "GB"]:
        if num_bytes < 1024.0:
            return f"{num_bytes:6.1f} {unit}"
        num_bytes /= 1024.0
    return f"{num_bytes:6.1f} TB"


def open_serial_connection(port: str, baud: int, wait_for_port: bool = True):
    """
    Attempts to open the serial port.
    If the port is locked by another window (Access Denied / Error 5),
    it waits in a friendly loop until the user disconnects that window.
    """
    warned = False
    while True:
        try:
            if USE_PYSERIAL:
                ser = serial.Serial(
                    port=port,
                    baudrate=baud,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    timeout=1.0,
                    xonxoff=False,
                    rtscts=False,
                    dsrdtr=False
                )
                return ser
            else:
                return NativeWin32Serial(port=port, baudrate=baud, timeout=1.0)

        except Exception as e:
            err_msg = str(e)
            is_busy = "Access is denied" in err_msg or "WinError 5" in err_msg or "PermissionError" in err_msg
            if is_busy:
                if not warned:
                    print(f"\n" + "=" * 70)
                    print(f" [PORT BUSY] Cannot open {port}: Access is denied (WinError 5)!")
                    print(f" [CAUSE]     Another application has {port} open right now.")
                    print(f"             (VS Code Serial Monitor / PuTTY / Arduino IDE)")
                    print(f" [ACTION]    Please CLOSE or DISCONNECT that window now!")
                    print(f"             Waiting for {port} to be released... (Ctrl+C to cancel)")
                    print("=" * 70 + "\n")
                    warned = True
                if wait_for_port:
                    print(f"\r  [WAITING] Retrying {port}... ", end="", flush=True)
                    time.sleep(0.8)
                    continue
                else:
                    raise
            else:
                raise


def main():
    parser = argparse.ArgumentParser(
        description="Team-04 Serial Logger (COM10, 230400 8-N-1)"
    )
    parser.add_argument("--port", "-p", type=str, default="COM10", help="Serial port (default: COM10)")
    parser.add_argument("--baud", "-b", type=int, default=230400, help="Baud rate (default: 230400)")
    parser.add_argument("--output", "-o", type=str, default="serial_COM10.log", help="Output file (default: serial_COM10.log)")
    parser.add_argument("--append", "-a", action="store_true", help="Append to file")
    parser.add_argument("--timestamp", "-t", action="store_true", help="Add PC timestamp")
    parser.add_argument("--echo-all", action="store_true", help="Echo every line to terminal")
    parser.add_argument("--duration", "-d", type=float, default=0.0, help="Recording time in seconds (0 = infinite)")

    args = parser.parse_args()

    driver_name = "pyserial" if USE_PYSERIAL else "Native Win32 ctypes (Zero dependency)"
    print(f"[INIT] Driver       : {driver_name}")
    print(f"[INIT] Target Port  : {args.port} @ {args.baud} 8-N-1")
    print(f"[INIT] Output file  : '{args.output}'")
    if args.duration > 0:
        print(f"[INIT] Duration     : {args.duration:.1f} seconds")

    try:
        ser = open_serial_connection(args.port, args.baud, wait_for_port=True)
    except KeyboardInterrupt:
        print("\n[CANCELLED] Cancelled by user.")
        sys.exit(0)
    except Exception as e:
        print(f"\n[FATAL ERROR] {e}")
        sys.exit(1)

    print(f"\n[SUCCESS] Connected to {args.port}! Recording stream...")
    print("[INFO] Press Ctrl+C at any time to finish.\n")

    captured_samples = []
    count = 0
    total_bytes = 0
    start_time = time.time()
    last_flush_time = start_time
    file_mode = "a" if args.append else "w"

    try:
        with open(args.output, file_mode, encoding="utf-8", errors="replace") as logfile:
            if not args.append:
                logfile.write(
                    f"# Team-04 Log Started: {datetime.datetime.now().isoformat()}\n"
                    f"# Port: {args.port} | Baud: {args.baud} 8-N-1\n"
                )
                logfile.flush()

            while True:
                raw_bytes = ser.readline()
                if not raw_bytes:
                    if args.duration > 0 and (time.time() - start_time) >= args.duration:
                        break
                    continue

                line = raw_bytes.decode("utf-8", errors="replace").strip("\r\n")
                if not line:
                    continue

                if args.timestamp:
                    now_str = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
                    file_entry = f"[{now_str}] {line}"
                else:
                    file_entry = line

                logfile.write(file_entry + "\n")
                count += 1
                total_bytes += len(raw_bytes)

                if len(captured_samples) < 15:
                    captured_samples.append(line)

                curr_time = time.time()
                if (count % 50 == 0) or ((curr_time - last_flush_time) >= 1.0):
                    logfile.flush()
                    last_flush_time = curr_time

                is_system_msg = line.startswith("[") or line.startswith("=") or line.startswith("#")

                if is_system_msg:
                    print(f"\n[DEVICE] {line}")
                elif args.echo_all:
                    print(line)
                else:
                    if count % 25 == 0:
                        elapsed = curr_time - start_time
                        rate = (count / elapsed) if elapsed > 0 else 0.0
                        size_str = format_bytes(total_bytes)
                        preview = line[:40] + ("..." if len(line) > 40 else "")
                        print(
                            f"\r[LOG] {count:06d} lines | {size_str} | {rate:5.1f} L/s | Last: {preview:<43}",
                            end="",
                            flush=True
                        )

                if args.duration > 0 and (curr_time - start_time) >= args.duration:
                    print(f"\n\n[INFO] Duration {args.duration:.1f}s reached.")
                    break

    except KeyboardInterrupt:
        print("\n\n[INFO] Logging stopped by user (Ctrl+C).")
    finally:
        ser.close()

    elapsed_total = time.time() - start_time
    avg_rate = (count / elapsed_total) if elapsed_total > 0 else 0.0

    print("\n" + "=" * 70)
    print(f" [COMPLETE] Total lines captured: {count}")
    print(f" [COMPLETE] Data size           : {format_bytes(total_bytes)}")
    print(f" [COMPLETE] Elapsed time        : {elapsed_total:.2f} s ({avg_rate:.1f} lines/sec)")
    print(f" [COMPLETE] Output saved to     : {os.path.abspath(args.output)}")
    print("=" * 70)

    if captured_samples:
        print("\n--- [PREVIEW: First captured lines] ---")
        for s in captured_samples[:8]:
            print("  " + s)
        print("---------------------------------------")


if __name__ == "__main__":
    main()
