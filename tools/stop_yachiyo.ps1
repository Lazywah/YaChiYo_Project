# stop_yachiyo.ps1 - stop everything started by start_yachiyo.ps1.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

function Say($msg) { Write-Host ("[YaChiYo] " + $msg) }

Get-Process -Name YaChiYo_Project -ErrorAction SilentlyContinue | ForEach-Object {
    Say "Stopping desktop pet (pid $($_.Id))"; Stop-Process -Id $_.Id -Force
}

$helpers = 'mouth_loopback.py', 'wakeword.py', 'note_receiver.py'
Get-CimInstance Win32_Process -Filter "Name='python.exe' OR Name='pythonw.exe'" | ForEach-Object {
    $p = $_
    foreach ($h in $helpers) {
        if ($p.CommandLine -like "*$h*") { Say "Stopping $h (pid $($p.ProcessId))"; Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue }
    }
}

# Hermes: close the whole console tree (cmd -> hermes -> python -> ffplay).
Get-Process -Name hermes -ErrorAction SilentlyContinue | ForEach-Object {
    $parent = (Get-CimInstance Win32_Process -Filter "ProcessId=$($_.Id)").ParentProcessId
    Say "Stopping Hermes (pid $($_.Id))"
    & taskkill.exe /PID $parent /T /F 2>$null | Out-Null
    & taskkill.exe /PID $_.Id /T /F 2>$null | Out-Null
}
Say 'All stopped.'
Start-Sleep -Seconds 2
