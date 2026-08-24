param()
$ErrorActionPreference = "Stop"

function Test-VersionAtLeast([version]$Actual, [version]$Minimum) {
    return $Actual -ge $Minimum
}

$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
if (-not $cmake) { throw "cmake.exe was not found in PATH." }
$line = (& cmake --version | Select-Object -First 1)
$verText = ($line -replace '^cmake version\s+','').Trim()
$ver = [version]$verText
if (-not (Test-VersionAtLeast $ver ([version]'3.20'))) {
    throw "CMake >= 3.20 is required; found $verText"
}
Write-Host "CMake: $verText"

$ninja = Get-Command ninja.exe -ErrorAction SilentlyContinue
if ($ninja) { Write-Host "Ninja: $(& ninja --version)" } else { Write-Warning "ninja.exe not found in PATH" }

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($cl) {
    Write-Host "MSVC cl.exe: $($cl.Source)"
} else {
    Write-Warning "cl.exe not found. Run this script from an x64 MSVC developer environment or use scripts/build_windows.ps1, which attempts to load vcvars64.bat."
}

if (Test-Path "CMakeUserPresets.json") {
    $j = Get-Content "CMakeUserPresets.json" -Raw | ConvertFrom-Json
    Write-Host "CMakeUserPresets.json schema version: $($j.version)"
    if ([int]$j.version -gt 2) {
        Write-Warning "User preset schema >2 can fail on older CMake installations. Prefer schema version 2 for this release."
    }
}

& cmake --list-presets
