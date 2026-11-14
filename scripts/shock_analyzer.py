#!/usr/bin/env python3
"""
@file    shock_analyzer.py
@author  team-04
@brief   Lightweight IMU shock and impulse analyzer for UART telemetry logs.
         Filters sensor noise, detects physical shock events via angular rate
         (gyroscope) and linear acceleration dynamics, and extracts shock profiles.
"""

import os
import sys
import math
import argparse

def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Team-04 IMU UART Log Shock Analyzer (Noise Filter & Shock Detector)"
    )
    parser.add_argument(
        "-i", "--input",
        type=str,
        default=os.path.join(os.path.dirname(__file__), "serial_COM10.log"),
        help="Path to UART telemetry log file (default: scripts/serial_COM10.log)"
    )
    parser.add_argument(
        "-t", "--threshold",
        type=float,
        default=0.0,
        help="Manual gyro dynamic threshold in dps (0.0 = auto adaptive filter)"
    )
    parser.add_argument(
        "--debounce-ms",
        type=float,
        default=350.0,
        help="Debounce window in ms to group impulse wave packets (default: 350 ms)"
    )
    return parser.parse_args()

def analyze_log(file_path: str, user_threshold: float, debounce_ms: float):
    if not os.path.isfile(file_path):
        print(f"[ERROR] File not found: {file_path}")
        sys.exit(1)

    print("=" * 78)
    print(" [Team-04] High-Precision IMU Shock & Gyroscope Impulse Analyzer")
    print("=" * 78)
    print(f"Target log file: {file_path}")

    # 1. Parse UART CSV Stream
    # Format: time_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps
    records = []
    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line_str = line.strip()
            if not line_str or line_str.startswith("#"):
                continue
            parts = line_str.split(",")
            if len(parts) >= 7:
                try:
                    t_ms = float(parts[0])
                    ax = float(parts[1])
                    ay = float(parts[2])
                    az = float(parts[3])
                    gx = float(parts[4])
                    gy = float(parts[5])
                    gz = float(parts[6])
                    records.append((t_ms, ax, ay, az, gx, gy, gz))
                except ValueError:
                    continue

    n_samples = len(records)
    if n_samples < 50:
        print(f"[ERROR] Insufficient data samples parsed ({n_samples}).")
        sys.exit(1)

    t_start_total = records[0][0]
    t_end_total = records[-1][0]
    duration_sec = (t_end_total - t_start_total) / 1000.0
    sample_rate_est = (n_samples - 1) / duration_sec if duration_sec > 0 else 250.0

    print(f"Parsed records  : {n_samples} samples")
    print(f"Time span       : {t_start_total:.0f} ms to {t_end_total:.0f} ms ({duration_sec:.2f} s)")
    print(f"Estimated ODR   : {sample_rate_est:.1f} Hz (~{1000.0/sample_rate_est:.1f} ms interval)")
    print("-" * 78)

    # 2. UART Axis Semantics and Physical Meaning
    print("\n[IMU AXES SEMANTICS & TELEMETRY MAPPING]")
    print("  * time_ms : Hardware timestamp in milliseconds (HAL_GetTick)")
    print("  * ax_g    : Linear acceleration X [g]. Resting ~ +0.09g")
    print("  * ay_g    : Linear acceleration Y [g]. Resting ~ -0.12g")
    print("  * az_g    : Linear acceleration Z [g]. Resting ~ +0.99g (Earth 1g static gravity vector)")
    print("  * gx_dps  : Gyroscope angular rate around X axis (Pitch/Roll) [deg/s]")
    print("  * gy_dps  : Gyroscope angular rate around Y axis (Pitch/Roll) [deg/s]")
    print("  * gz_dps  : Gyroscope angular rate around Z axis (Yaw/Twist)  [deg/s]")

    # 3. Baseline & Noise Calibration (Quiet Window)
    calib_count = min(3000, n_samples // 3)
    quiet_window = records[:calib_count]

    bgx = sum(r[4] for r in quiet_window) / calib_count
    bgy = sum(r[5] for r in quiet_window) / calib_count
    bgz = sum(r[6] for r in quiet_window) / calib_count
    baz = sum(r[3] for r in quiet_window) / calib_count
    bax = sum(r[1] for r in quiet_window) / calib_count
    bay = sum(r[2] for r in quiet_window) / calib_count

    # Measure noise floor
    noise_vals = [
        math.sqrt((r[4] - bgx)**2 + (r[5] - bgy)**2 + (r[6] - bgz)**2)
        for r in quiet_window
    ]
    noise_mean = sum(noise_vals) / len(noise_vals)
    noise_peak = max(noise_vals)

    # Adaptive noise threshold: 3.5x noise floor or user manual threshold
    threshold = user_threshold if user_threshold > 0.0 else max(3.5, noise_peak * 2.8)

    print("\n[NOISE FILTER & SENSOR BASELINE CALIBRATION]")
    print(f"  Gyroscope Zero-Bias   : gx={bgx:+.2f}, gy={bgy:+.2f}, gz={bgz:+.2f} dps")
    print(f"  Accelerometer Gravity : ax={bax:+.3f}, ay={bay:+.3f}, az={baz:+.3f} g (|a|={math.sqrt(bax**2+bay**2+baz**2):.3f} g)")
    print(f"  Sensor Noise Floor    : Avg={noise_mean:.2f} dps, Peak Noise={noise_peak:.2f} dps")
    print(f"  Active Filter Cutoff  : Threshold > {threshold:.2f} dps (SNR > 3.0x above noise floor)")
    print("-" * 78)

    # 4. Shock Event Clustering
    shock_events = []
    current_cluster = []

    for r in records:
        t, ax, ay, az, gx, gy, gz = r
        # Dynamic angular rate vector deviation from bias
        dgx = gx - bgx
        dgy = gy - bgy
        dgz = gz - bgz
        dyn_gyro = math.sqrt(dgx*dgx + dgy*dgy + dgz*dgz)

        # Dynamic linear acceleration vector deviation from gravity
        dax = ax - bax
        day = ay - bay
        daz = az - baz
        dyn_acc = math.sqrt(dax*dax + day*day + daz*daz)

        if dyn_gyro >= threshold:
            if not current_cluster or (t - current_cluster[-1]["t"] <= debounce_ms):
                current_cluster.append({
                    "t": t, "ax": ax, "ay": ay, "az": az,
                    "gx": gx, "gy": gy, "gz": gz,
                    "dgx": dgx, "dgy": dgy, "dgz": dgz,
                    "dyn_gyro": dyn_gyro, "dyn_acc": dyn_acc
                })
            else:
                shock_events.append(current_cluster)
                current_cluster = [{
                    "t": t, "ax": ax, "ay": ay, "az": az,
                    "gx": gx, "gy": gy, "gz": gz,
                    "dgx": dgx, "dgy": dgy, "dgz": dgz,
                    "dyn_gyro": dyn_gyro, "dyn_acc": dyn_acc
                }]

    if current_cluster:
        shock_events.append(current_cluster)

    # 5. Output Results & Characteristics
    n_shocks = len(shock_events)
    print(f"\n[DETECTION RESULT: {n_shocks} PHYSICAL SHOCKS DETECTED]")
    print(f"Filtered out sensor jitter; found {n_shocks} distinct impact events:\n")

    prev_time = None
    for i, cluster in enumerate(shock_events, 1):
        t_start = cluster[0]["t"]
        t_end = cluster[-1]["t"]
        duration = t_end - t_start + 4.0  # including sample span

        peak_gyro_sample = max(cluster, key=lambda x: x["dyn_gyro"])
        peak_acc_sample = max(cluster, key=lambda x: x["dyn_acc"])

        t_peak = peak_gyro_sample["t"]
        cadence_str = f"+{(t_peak - prev_time)/1000.0:.2f} s" if prev_time else "First Event"
        prev_time = t_peak

        # Angular rate components at peak
        pgx = peak_gyro_sample["gx"]
        pgy = peak_gyro_sample["gy"]
        pgz = peak_gyro_sample["gz"]
        gyro_mag = peak_gyro_sample["dyn_gyro"]

        # Linear acceleration peak
        p_acc_mag = peak_acc_sample["dyn_acc"]
        pax = peak_acc_sample["ax"]
        pay = peak_acc_sample["ay"]
        paz = peak_acc_sample["az"]

        # Dominant axis of rotation
        dom_axis = "GX (Pitch/Roll Tilt)" if abs(peak_gyro_sample["dgx"]) >= max(abs(peak_gyro_sample["dgy"]), abs(peak_gyro_sample["dgz"])) else \
                   "GY (Roll/Pitch)" if abs(peak_gyro_sample["dgy"]) >= abs(peak_gyro_sample["dgz"]) else "GZ (Yaw/Twist)"

        print(f"----------------------------------------------------------------------")
        print(f"  SHOCK EVENT #{i} (Cadence from previous: {cadence_str})")
        print(f"----------------------------------------------------------------------")
        print(f"  * Peak Timestamp     : {t_peak:8.0f} ms  ({t_peak/1000.0:6.3f} s)")
        print(f"  * Active Packet Span : {t_start:.0f} ms .. {t_end:.0f} ms  (Duration: {duration:.0f} ms, {len(cluster)} samples)")
        print(f"  * Gyroscope Peak     : {gyro_mag:6.2f} dps  (Noise SNR: {gyro_mag/noise_peak:.1f}x)")
        print(f"    - Instant Rates    : gx={pgx:+6.2f} dps | gy={pgy:+6.2f} dps | gz={pgz:+6.2f} dps")
        print(f"    - Primary Axis     : {dom_axis}")
        print(f"  * Acceleration Peak  : {p_acc_mag:6.2f} g dynamic  (Raw: ax={pax:+.2f}, ay={pay:+.2f}, az={paz:+.2f} g)")
        print(f"  * Cutoff 0.25 * Amax : {0.25 * p_acc_mag:6.2f} g")

    print("\n" + "=" * 78)
    print(f" [CONCLUSION] Exactly {n_shocks} shocks identified in {os.path.basename(file_path)}.")
    print("=" * 78 + "\n")

if __name__ == "__main__":
    args = parse_arguments()
    analyze_log(args.input, args.threshold, args.debounce_ms)

# /***************************** END OF FILE ************************************/
