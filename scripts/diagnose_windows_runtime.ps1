param(
    [string]$BuildDir = "build/windows-observatory"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path
$build = (Resolve-Path (Join-Path $root $BuildDir)).Path
$drivers = Join-Path $build "drivers"
$bad = '^(?i)(msvcr|msvcp|vcruntime|ucrtbase|concrt|vcamp|atl|mfc|mfcm)[0-9_].*\.dll$|^(?i)(api-ms-win-|ext-ms-win-).+\.dll$'

Write-Host "Scanning for app-local Microsoft CRT/UCRT DLLs under $build"
$hits = Get-ChildItem $build,$drivers -Filter *.dll -File -ErrorAction SilentlyContinue |
    Sort-Object FullName -Unique |
    Where-Object { $_.Name -match $bad }
if ($hits) {
    Write-Warning "Potential R6034/SxS hazards found. These should not be copied from vendor SDK folders:"
    $hits | ForEach-Object { Write-Host "  $($_.FullName)" }
    exit 2
}
Write-Host "PASS: no app-local Microsoft CRT/UCRT DLLs detected."
