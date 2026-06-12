<#
.SYNOPSIS
    YaChiYo Desktop Pet - Windows deploy script (windeployqt + MinGW runtime + skin examples)

.DESCRIPTION
    Packages an already-compiled YaChiYo_Project.exe into a self-contained folder
    (dist/YaChiYo/), gathering Qt DLLs/plugins, the MinGW runtime DLLs, skin
    examples and a sounds placeholder. The result can be zipped and shared, or fed
    to tools/installer.iss to build an installer.

    NOTE: This script does NOT compile. Build a Release exe first (Qt Creator / CMake).

    (ASCII-only on purpose: Windows PowerShell 5.1 misreads UTF-8 scripts without a BOM.)

.PARAMETER ExePath
    Path to the compiled YaChiYo_Project.exe. If omitted, the newest one under build\ is used.

.PARAMETER QtBin
    Qt / MinGW bin directory (contains windeployqt.exe and the runtime DLLs).

.PARAMETER DistDir
    Output directory. Defaults to dist\YaChiYo.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\deploy.ps1
    powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -ExePath build\Release\YaChiYo_Project.exe
#>
param(
    [string]$ExePath = "",
    [string]$QtBin   = "C:\msys64\ucrt64\bin",
    [string]$DistDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent   # project root = parent of tools\

if ([string]::IsNullOrEmpty($DistDir)) { $DistDir = Join-Path $root "dist\YaChiYo" }

# ---- 1. Locate the exe -------------------------------------------------------
if ([string]::IsNullOrEmpty($ExePath)) {
    $found = Get-ChildItem -Path (Join-Path $root "build") -Recurse -Filter "YaChiYo_Project.exe" -ErrorAction SilentlyContinue |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($null -eq $found) {
        throw "YaChiYo_Project.exe not found. Build Release first, or pass -ExePath."
    }
    $ExePath = $found.FullName
}
if (-not (Test-Path $ExePath)) { throw "exe does not exist: $ExePath" }
Write-Host "Source exe : $ExePath"

# ---- 2. Check tools ----------------------------------------------------------
$windeployqt = Join-Path $QtBin "windeployqt.exe"
if (-not (Test-Path $windeployqt)) { throw "windeployqt not found: $windeployqt (use -QtBin to set the Qt bin dir)" }

# ---- 3. Clean & create dist --------------------------------------------------
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
Copy-Item $ExePath (Join-Path $DistDir "YaChiYo_Project.exe")
Write-Host "Output dir : $DistDir"

# ---- 4. windeployqt: gather Qt dependencies ----------------------------------
$env:PATH = "$QtBin;" + $env:PATH
& $windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $DistDir "YaChiYo_Project.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed (exit $LASTEXITCODE)" }

# ---- 5. Copy MinGW runtime DLLs ----------------------------------------------
# windeployqt omits the MinGW runtime - copy these manually.
$runtimeDlls = @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
foreach ($dll in $runtimeDlls) {
    $src = Join-Path $QtBin $dll
    if (Test-Path $src) { Copy-Item $src $DistDir }
    else { Write-Warning "Missing runtime DLL: $dll (app may not start on machines without MinGW)" }
}

# ---- 6. Skin examples --------------------------------------------------------
# The default skin is embedded in the exe; ship a filesystem copy as an authoring reference.
$skinSrc = Join-Path $root "resources\skins"
$skinDst = Join-Path $DistDir "skins"
New-Item -ItemType Directory -Force -Path $skinDst | Out-Null
Copy-Item (Join-Path $skinSrc "README.md")  $skinDst
Copy-Item (Join-Path $skinSrc "default")    $skinDst -Recurse

# ---- 7. sounds placeholder ---------------------------------------------------
# Users drop their own .wav here (requires a Multimedia-enabled build to play).
$soundDir = Join-Path $DistDir "sounds"
New-Item -ItemType Directory -Force -Path $soundDir | Out-Null
@"
Put these 4 .wav files in this folder to enable sound effects:
  land.wav     landing
  wall.wav     wall hit
  grab.wav     grab
  release.wav  release
Note: requires a build with Qt Multimedia for sound to actually play.
"@ | Out-File -FilePath (Join-Path $soundDir "README.txt") -Encoding ascii

# ---- 8. Done -----------------------------------------------------------------
$size = [math]::Round((Get-ChildItem $DistDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
Write-Host ""
Write-Host "Done. Deploy folder size: $size MB" -ForegroundColor Green
Write-Host "Run directly : $DistDir\YaChiYo_Project.exe"
Write-Host "Make installer: compile tools\installer.iss with Inno Setup"
