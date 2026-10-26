# start_note_receiver.ps1 - start only the phone note receiver (for Windows logon).
# Waits for Tailscale to get its IP first (it may not be up yet right after
# logon; the receiver exits if it cannot find one). Skips if already running.
# Logs go to build\app\logs\ like start_yachiyo.ps1.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

$ErrorActionPreference = 'Stop'
$Root   = Split-Path -Parent $PSScriptRoot
$Tools  = $PSScriptRoot
$PyW    = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313\pythonw.exe'
$LogDir = Join-Path $Root 'build\app\logs'
$TailscaleExe = 'C:\Program Files\Tailscale\tailscale.exe'
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$procs = Get-CimInstance Win32_Process -Filter "Name='python.exe' OR Name='pythonw.exe'"
if ($procs | Where-Object { $_.CommandLine -like '*note_receiver.py*' }) { exit 0 }

# Wait up to 3 minutes for a Tailscale IPv4 address.
$deadline = (Get-Date).AddMinutes(3)
$ip = $null
while ((Get-Date) -lt $deadline) {
    try { $ip = (& $TailscaleExe ip -4 2>$null | Select-Object -First 1) } catch { $ip = $null }
    if ($ip -match '^100\.') { break }
    Start-Sleep -Seconds 5
}
if ($ip -notmatch '^100\.') {
    Add-Content -Path "$LogDir\note_receiver.err.log" -Value "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] Tailscale IP not available after 3 min - receiver not started."
    exit 1
}

Start-Process -FilePath $PyW -WorkingDirectory $Tools `
    -ArgumentList "-u `"$Tools\note_receiver.py`"" `
    -RedirectStandardOutput "$LogDir\note_receiver.log" -RedirectStandardError "$LogDir\note_receiver.err.log"
