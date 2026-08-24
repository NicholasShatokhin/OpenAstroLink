param(
    [string]$Preset = "my-windows-observatory",
    [switch]$Clean,
    [int]$Jobs = 0
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path
Set-Location $root

function Import-VcVars64 {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }

    $candidates = @(
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    ) | Where-Object { $_ -and (Test-Path $_) }

    if (-not $candidates) {
        throw "MSVC x64 environment not found. Install VS 2022 C++ tools or run from 'x64 Native Tools Command Prompt for VS 2022'."
    }

    $vcvars = $candidates[0]
    Write-Host "Loading MSVC environment: $vcvars"
    $lines = & cmd.exe /s /c "`"call `"$vcvars`" >nul && set`""
    foreach ($line in $lines) {
        $idx = $line.IndexOf('=')
        if ($idx -gt 0) {
            $name = $line.Substring(0,$idx)
            $value = $line.Substring($idx+1)
            [Environment]::SetEnvironmentVariable($name,$value,'Process')
        }
    }
}

Import-VcVars64

if (-not (Get-Command ninja.exe -ErrorAction SilentlyContinue)) {
    $qtNinja = "C:\Qt\Tools\Ninja"
    if (Test-Path "$qtNinja\ninja.exe") { $env:PATH = "$qtNinja;$env:PATH" }
}
if (-not (Get-Command ninja.exe -ErrorAction SilentlyContinue)) {
    throw "Ninja was not found. Install Ninja or add C:\Qt\Tools\Ninja to PATH."
}

& "$PSScriptRoot/check_build_environment.ps1"

if ($Clean) {
    $presetJson = Get-Content "$root/CMakePresets.json" -Raw | ConvertFrom-Json
    $all = @($presetJson.configurePresets)
    $userPath = "$root/CMakeUserPresets.json"
    if (Test-Path $userPath) { $all += @((Get-Content $userPath -Raw | ConvertFrom-Json).configurePresets) }
    $p = $all | Where-Object { $_.name -eq $Preset } | Select-Object -First 1
    # User presets normally inherit a repository preset; use the known observatory path as a safe fallback.
    $dir = "$root/build/windows-observatory"
    if ($p -and $p.binaryDir) { $dir = $p.binaryDir.Replace('${sourceDir}', $root) }
    Write-Host "Cleaning $dir"
    Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
}

cmake --preset $Preset
if ($Jobs -gt 0) { cmake --build --preset $Preset --parallel $Jobs }
else { cmake --build --preset $Preset --parallel }
