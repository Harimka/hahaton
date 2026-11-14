<#
.SYNOPSIS
    Team-04 High-Speed Serial Logger for STM32G474RE Nucleo (PowerShell Native)
    Reads COM9 at 230400 8-N-1 and streams directly to disk.
#>

param (
    [string]$Port = "COM9",
    [int]$BaudRate = 230400,
    [string]$Output = "serial_com9.log",
    [int]$Duration = 0
)

Write-Host "====================================================================" -ForegroundColor Cyan
Write-Host " [Team-04] PowerShell Serial Logger: $Port @ $BaudRate (8-N-1)" -ForegroundColor Cyan
Write-Host " Output file: $Output" -ForegroundColor Cyan
if ($Duration -gt 0) {
    Write-Host " Duration   : $Duration seconds" -ForegroundColor Cyan
}
Write-Host "====================================================================" -ForegroundColor Cyan

# Check if port is locked and wait if needed
$sp = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One
$sp.ReadTimeout = 1000

$opened = $false
$warned = $false

while (-not $opened) {
    try {
        $sp.Open()
        $opened = $true
    } catch {
        if (-not $warned) {
            Write-Host "`n[PORT BUSY] Cannot open $Port : Access is denied!" -ForegroundColor Red
            Write-Host "[REASON]    Your serial monitor window in VS Code / PuTTY is holding $Port open." -ForegroundColor Yellow
            Write-Host "[ACTION]    Please CLOSE or DISCONNECT that window now!" -ForegroundColor Yellow
            Write-Host "Waiting for $Port to be released... (Press Ctrl+C to cancel)" -ForegroundColor Gray
            $warned = $true
        }
        Start-Sleep -Milliseconds 800
    }
}

Write-Host "`n[SUCCESS] Connected to $Port! Recording stream..." -ForegroundColor Green
Write-Host "[INFO] Press Ctrl+C at any time to stop.`n" -ForegroundColor Gray

$sw = [System.IO.File]::CreateText($Output)
$sw.WriteLine("# Team-04 Log Started: $(Get-Date -Format o)")
$sw.WriteLine("# Port: $Port | Baud: $BaudRate 8-N-1")
$sw.Flush()

$count = 0
$startTime = [System.Diagnostics.Stopwatch]::StartNew()
$lastFlush = [System.Diagnostics.Stopwatch]::StartNew()
$preview = @()

try {
    while ($true) {
        try {
            $line = $sp.ReadLine()
            if ($line) {
                $sw.WriteLine($line)
                $count++
                if ($preview.Count -lt 10) { $preview += $line }

                if ($count % 50 -eq 0 -or $lastFlush.ElapsedMilliseconds -gt 1000) {
                    $sw.Flush()
                    $lastFlush.Restart()
                }

                if ($line.StartsWith("[") -or $line.StartsWith("=") -or $line.StartsWith("#")) {
                    Write-Host "`n[DEVICE] $line" -ForegroundColor Yellow
                } elseif ($count % 25 -eq 0) {
                    $elapsedSec = $startTime.Elapsed.TotalSeconds
                    $rate = if ($elapsedSec -gt 0) { $count / $elapsedSec } else { 0 }
                    Write-Host -NoNewline "`r[LOG] $count lines | $([Math]::Round($rate, 1)) L/s | Last: $($line.Substring(0, [Math]::Min(35, $line.Length)))   "
                }

                if ($Duration -gt 0 -and $startTime.Elapsed.TotalSeconds -ge $Duration) {
                    Write-Host "`n[INFO] Target duration of $Duration seconds reached." -ForegroundColor Green
                    break
                }
            }
        } catch [System.TimeoutException] {
            if ($Duration -gt 0 -and $startTime.Elapsed.TotalSeconds -ge $Duration) { break }
        }
    }
} finally {
    $sw.Flush()
    $sw.Close()
    $sp.Close()
    $startTime.Stop()
}

Write-Host "`n====================================================================" -ForegroundColor Cyan
Write-Host " [COMPLETE] Total lines captured: $count" -ForegroundColor Green
Write-Host " [COMPLETE] Output file saved to : $((Get-Item $Output).FullName)" -ForegroundColor Green
Write-Host "====================================================================" -ForegroundColor Cyan

if ($preview.Count -gt 0) {
    Write-Host "`n--- [PREVIEW: First lines] ---" -ForegroundColor White
    $preview | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    Write-Host "------------------------------" -ForegroundColor White
}
