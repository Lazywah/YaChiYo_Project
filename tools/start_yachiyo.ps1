# start_yachiyo.ps1 - one-click launcher for YaChiYo + Hermes voice assistant.
# Starts: Hermes (classic conhost window), mouth sync, wake word, phone note
# receiver, desktop pet.
# Helper scripts run windowless (pythonw); logs go to build\app\logs\.
# Stop everything with stop_yachiyo.ps1.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

$ErrorActionPreference = 'Stop'
$Root   = Split-Path -Parent $PSScriptRoot
$Tools  = $PSScriptRoot
$PetExe = Join-Path $Root 'build\app\YaChiYo_Project.exe'
$PyDir  = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313'
$PyW    = Join-Path $PyDir 'pythonw.exe'
$QtBin  = 'C:\msys64\ucrt64\bin'
$Title  = 'YaChiYo-Hermes'
$LogDir = Join-Path $Root 'build\app\logs'
$AgentLog = Join-Path $env:LOCALAPPDATA 'hermes\logs\agent.log'
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

function Say($msg) { Write-Host ("[YaChiYo] " + $msg) }

# 1) Rebuild the environment from the registry so we do not depend on the
#    state of explorer.exe (its PATH once lost the whole user PATH, which hid
#    ffplay and silenced Hermes' voice replies).
$machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
$userPath    = [Environment]::GetEnvironmentVariable('Path', 'User')
$env:Path = "$machinePath;$userPath"
$userEnv = Get-Item 'HKCU:\Environment'
foreach ($name in $userEnv.GetValueNames()) {
    if ($name -ieq 'Path') { continue }
    Set-Item -Path "Env:$name" -Value ([Environment]::GetEnvironmentVariable($name, 'User'))
}
$basePath = $env:Path

function Test-PyScriptRunning($script) {
    $procs = Get-CimInstance Win32_Process -Filter "Name='python.exe' OR Name='pythonw.exe'"
    return [bool]($procs | Where-Object { $_.CommandLine -like "*$script*" })
}

# 2) Hermes in a classic console window (WriteConsoleInput injection needs conhost).
$hermesStarted = $false
if (Get-Process -Name hermes -ErrorAction SilentlyContinue) {
    Say 'Hermes already running - skip.'
} else {
    Say 'Starting Hermes...'
    $launchTime = Get-Date
    Start-Process -FilePath 'conhost.exe' -ArgumentList "cmd.exe /k `"title $Title && hermes`""
    $hermesStarted = $true
}

# 3) Mouth sync and wake word (windowless).
if (Test-PyScriptRunning 'mouth_loopback.py') {
    Say 'mouth_loopback already running - skip.'
} else {
    Say 'Starting mouth sync...'
    Start-Process -FilePath $PyW -WorkingDirectory $Tools `
        -ArgumentList "-u `"$Tools\mouth_loopback.py`" --quiet" `
        -RedirectStandardOutput "$LogDir\mouth_loopback.log" -RedirectStandardError "$LogDir\mouth_loopback.err.log"
}
if (Test-PyScriptRunning 'wakeword.py') {
    Say 'wakeword already running - skip.'
} else {
    Say 'Starting wake word (say: ba qian dai)...'
    Start-Process -FilePath $PyW -WorkingDirectory $Tools `
        -ArgumentList "-u `"$Tools\wakeword.py`" --window-title $Title --window-class ConsoleWindowClass --inject conhost" `
        -RedirectStandardOutput "$LogDir\wakeword.log" -RedirectStandardError "$LogDir\wakeword.err.log"
}

# 3b) Phone note receiver: binds the Tailscale IP only (exits if Tailscale is down).
if (Test-PyScriptRunning 'note_receiver.py') {
    Say 'note_receiver already running - skip.'
} else {
    Say 'Starting phone note receiver (Tailscale only)...'
    Start-Process -FilePath $PyW -WorkingDirectory $Tools `
        -ArgumentList "-u `"$Tools\note_receiver.py`"" `
        -RedirectStandardOutput "$LogDir\note_receiver.log" -RedirectStandardError "$LogDir\note_receiver.err.log"
}

# 4) Desktop pet. Only this process gets PATH = Python313 -> msys64 Qt -> rest:
#    Qt DLLs must come from msys64 (MiKTeX ships another Qt6Core.dll), and the
#    pet's "pythonw" must resolve to Python313, not msys64's own python.
if (Get-Process -Name YaChiYo_Project -ErrorAction SilentlyContinue) {
    Say 'Desktop pet already running - skip.'
} elseif (-not (Test-Path $PetExe)) {
    Say "Desktop pet not built: $PetExe (build with cmake --build build/app)"
} else {
    Say 'Starting desktop pet...'
    $env:Path = "$PyDir;$QtBin;$basePath"
    Start-Process -FilePath $PetExe -ArgumentList '--live2d', '--voice' -WorkingDirectory (Split-Path $PetExe)
    $env:Path = $basePath
}

# 5) Turn on Hermes voice mode once its CLI is ready (plugin discovery logged
#    after launch + a short grace period for the prompt to appear).
if ($hermesStarted) {
    Say 'Waiting for Hermes to be ready...'
    $ready = $false
    $deadline = (Get-Date).AddSeconds(90)
    while ((Get-Date) -lt $deadline -and -not $ready) {
        Start-Sleep -Milliseconds 700
        if (Test-Path $AgentLog) {
            foreach ($line in (Get-Content $AgentLog -Tail 80 -ErrorAction SilentlyContinue)) {
                if ($line -match '^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}).*Plugin discovery complete') {
                    $t = [datetime]::ParseExact($matches[1], 'yyyy-MM-dd HH:mm:ss', $null)
                    if ($t -ge $launchTime.AddSeconds(-1)) { $ready = $true }
                }
            }
        }
    }
    if (-not $ready) { Say 'Hermes readiness not detected within 90s - trying anyway.' }
    Start-Sleep -Seconds 4
    Say 'Sending /voice on to Hermes...'
    Start-Process -FilePath $PyW -WorkingDirectory $Tools -Wait `
        -ArgumentList "`"$Tools\hermes_inject.py`" type-text `"/voice on`" --window-title $Title --window-class ConsoleWindowClass" `
        -RedirectStandardOutput "$LogDir\inject.log" -RedirectStandardError "$LogDir\inject.err.log"
}

Say 'Done. Say "ba qian dai", double-click the pet, or press Ctrl+B in the Hermes window.'
Start-Sleep -Seconds 3
