#Requires -Version 5.1
<#
  Aurora Player — Windows release + installer build.

  Usage:
    .\scripts\build_installer.ps1 -QtDir C:\Qt\6.7.2\msvc2019_64

  Produces:
    dist\                                   deployable app folder (windeployqt)
    dist-installer\AuroraPlayer-1.0.0-win64-setup.exe
#>
param(
  [string]$QtDir = $env:QTDIR,
  [string]$Config = "Release",
  [string]$Iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not $QtDir) {
  throw "Qt not found. Pass -QtDir C:\Qt\6.7.2\msvc2019_64 or set the QTDIR environment variable."
}

Write-Host "==> Configuring (Qt: $QtDir)" -ForegroundColor Cyan
cmake -S . -B build-win -DCMAKE_BUILD_TYPE=$Config -DCMAKE_PREFIX_PATH="$QtDir" -DAURORA_BUILD_GUI=ON -DAURORA_BUILD_CLI=ON -DAURORA_LTO=ON

Write-Host "==> Building" -ForegroundColor Cyan
cmake --build build-win --config $Config --parallel

Write-Host "==> Running tests" -ForegroundColor Cyan
ctest --test-dir build-win -C $Config --output-on-failure

$dist = Join-Path $root "dist"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist | Out-Null

$exe = Get-ChildItem -Path build-win -Recurse -Filter "aurora-player.exe" | Select-Object -First 1
if (-not $exe) { throw "aurora-player.exe was not built" }
Copy-Item $exe.FullName $dist

$cli = Get-ChildItem -Path build-win -Recurse -Filter "aurora-cli.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($cli) { Copy-Item $cli.FullName $dist }

Write-Host "==> Deploying Qt runtime" -ForegroundColor Cyan
& "$QtDir\bin\windeployqt.exe" --qmldir "$root\app\qml" --release --no-translations --no-system-d3d-compiler "$dist\aurora-player.exe"

Copy-Item -Recurse "$root\i18n" (Join-Path $dist "i18n") -Force

# Bundle optional external tools when they are on PATH (offline playback works without them)
foreach ($tool in @("ffmpeg.exe", "ffprobe.exe", "yt-dlp.exe")) {
  $found = Get-Command $tool -ErrorAction SilentlyContinue
  if ($found) {
    Copy-Item $found.Source $dist
    Write-Host "    bundled $tool"
  } else {
    Write-Host "    $tool not on PATH — skipped (users can install it later)"
  }
}

if (-not (Test-Path $Iscc)) {
  Write-Warning "Inno Setup 6 not found at $Iscc. Portable build is ready in dist\, installer skipped."
  exit 0
}

Write-Host "==> Building installer" -ForegroundColor Cyan
& $Iscc "packaging\installer\aurora-player.iss"

Write-Host ""
Write-Host "Done:" -ForegroundColor Green
Write-Host "  portable : dist\"
Write-Host "  installer: dist-installer\AuroraPlayer-1.0.0-win64-setup.exe"
