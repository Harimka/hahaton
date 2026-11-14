#!/usr/bin/env python3
"""
Team-04 High-Speed Shock Telemetry Recorder & Live Plotter
Reads 500 Hz CSV stream from STM32G474RE Nucleo over ST-LINK VCP UART at 921600 baud.
Saves data to CSV and generates impact waveform plot.
"""

import sys
import time
import argparse
import csv

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[ERROR] pyserial is not installed. Install via: pip install pyserial")
    sys.exit(1)

def find_stlink_port():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        desc = (port.description or "") + (port.manufacturer or "")
        if "STMicroelectronics" in desc or "ST-Link" in desc or "STM32" in desc:
            return port.device
    if ports:
        return ports[0].device
    return None

def main():
    parser = argparse.ArgumentParser(description="Team-04 IMU Shock Recorder (500 Hz, 921600 baud)")
    parser.add_argument("--port", "-p", type=str, default=None, help="COM port (e.g. COM3 or /dev/ttyACM0)")
    parser.add_argument("--baud", "-b", type=int, default=230400, help="Baud rate (default: 230400)")
    parser.add_argument("--output", "-o", type=str, default="shock_test_data.csv", help="Output CSV filename")
    parser.add_argument("--duration", "-d", type=float, default=0.0, help="Record duration in seconds (0 = until Ctrl+C)")
    args = parser.parse_args()

    port = args.port
    if not port:
        port = find_stlink_port()
        if not port:
            print("[ERROR] No serial ports found! Please specify port with --port COMx")
            sys.exit(1)
        print(f"[INFO] Auto-detected ST-LINK port: {port}")

    print(f"[INFO] Connecting to {port} @ {args.baud} baud...")
    try:
        ser = serial.Serial(port, args.baud, timeout=1.0)
    except Exception as e:
        print(f"[ERROR] Could not open port {port}: {e}")
        sys.exit(1)

    print(f"[INFO] Connected! Recording to '{args.output}'...")
    print("[INFO] Press Ctrl+C at any time to stop recording and generate plot.\n")

    timestamps = []
    ax_list, ay_list, az_list = [], [], []
    gx_list, gy_list, gz_list = [], [], []

    count = 0
    start_time = time.time()

    with open(args.output, "w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["time_ms", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps"])

        try:
            while True:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                # Ignore banners and logs that start with [ or =
                if line.startswith("[") or line.startswith("=") or line.startswith("time_ms"):
                    print(f" [DEVICE] {line}")
                    continue

                parts = line.split(",")
                if len(parts) == 7:
                    try:
                        t = float(parts[0])
                        ax, ay, az = float(parts[1]), float(parts[2]), float(parts[3])
                        gx, gy, gz = float(parts[4]), float(parts[5]), float(parts[6])

                        writer.writerow([t, ax, ay, az, gx, gy, gz])
                        timestamps.append(t)
                        ax_list.append(ax)
                        ay_list.append(ay)
                        az_list.append(az)
                        gx_list.append(gx)
                        gy_list.append(gy)
                        gz_list.append(gz)

                        count += 1
                        if count % 100 == 0:
                            print(f"\r  Recorded {count:05d} samples | Current: ax={ax:+6.2f}g ay={ay:+6.2f}g az={az:+6.2f}g", end="", flush=True)

                        if args.duration > 0 and (time.time() - start_time) >= args.duration:
                            break

                    except ValueError:
                        pass
        except KeyboardInterrupt:
            print("\n[INFO] Recording stopped by user.")

    ser.close()
    print(f"\n[SUCCESS] Saved {count} samples to '{args.output}'.")

    # Generate plot if matplotlib is available
    if count > 10:
        try:
            import matplotlib.pyplot as plt
            # Normalize time to start at 0
            t0 = timestamps[0]
            rel_t = [(t - t0) for t in timestamps]

            fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 6), sharex=True)
            fig.suptitle("Team-04 Test Bench IMU Shock Waveform (500 Hz)", fontsize=14, fontweight="bold")

            ax1.plot(rel_t, ax_list, label="Ax [g]", color="red", alpha=0.8)
            ax1.plot(rel_t, ay_list, label="Ay [g]", color="green", alpha=0.8)
            ax1.plot(rel_t, az_list, label="Az [g]", color="blue", alpha=0.8)
            ax1.set_ylabel("Acceleration [g]")
            ax1.grid(True, linestyle="--", alpha=0.6)
            ax1.legend(loc="upper right")

            ax2.plot(rel_t, gx_list, label="Gx [dps]", color="magenta", alpha=0.7)
            ax2.plot(rel_t, gy_list, label="Gy [dps]", color="cyan", alpha=0.7)
            ax2.plot(rel_t, gz_list, label="Gz [dps]", color="orange", alpha=0.7)
            ax2.set_xlabel("Time [ms]")
            ax2.set_ylabel("Angular Rate [dps]")
            ax2.grid(True, linestyle="--", alpha=0.6)
            ax2.legend(loc="upper right")

            plt.tight_layout()
            plot_file = args.output.replace(".csv", ".png")
            plt.savefig(plot_file, dpi=150)
            print(f"[SUCCESS] Waveform plot saved to '{plot_file}'!")
        except ImportError:
            print("[INFO] matplotlib not installed. CSV data is ready to plot in Excel.")

if __name__ == "__main__":
    main()
