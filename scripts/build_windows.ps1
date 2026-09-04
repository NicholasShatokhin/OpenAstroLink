param(
    [string]$Preset = "my-windows-observatory",
    [switch]$Clean,
    [int]$Jobs = 0,
    [switch]$NoAutoDeps,
    [switch]$BootstrapDeps,
    [string]$QtVersion = "6.10.0",
    [string]$QhyVersion = "26.06.04"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path
Set-Location $root

function Import-VcVars64 {
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($cl -and $env:VCToolsInstallDir) { return }
    $candidates = @(
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    ) | Where-Object { $_ -and (Test-Path $_) }
    if (-not $candidates) { throw "MSVC x64 environment not found. Install VS 2022 C++ tools." }
    $vcvars = $candidates[0]
    Write-Host "Loading MSVC environment: $vcvars"
    $lines = & cmd.exe /s /c "`"call `"$vcvars`" >nul && set`""
    foreach ($line in $lines) {
        $idx = $line.IndexOf('=')
        if ($idx -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0,$idx),$line.Substring($idx+1),'Process') }
    }
}

Import-VcVars64

function Ensure-Ninja {
    if (Get-Command ninja.exe -ErrorAction SilentlyContinue) { return }
    $candidates = @(
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    ) | Where-Object { $_ -and (Test-Path $_) }
    if ($candidates) {
        $dir = Split-Path -Parent $candidates[0]
        $env:PATH = "$dir;$env:PATH"
        return
    }
    throw "Ninja was not found. Install Ninja or the Visual Studio CMake tools component."
}

Ensure-Ninja

$cmakeExtra = @()
if (-not $NoAutoDeps) {
    $depRecord = "$root\.oal\native-deps-windows-x64.json"
    $need = $BootstrapDeps -or -not (Test-Path $depRecord)
    if (-not $need) {
        try {
            $d = Get-Content $depRecord -Raw | ConvertFrom-Json
            if (-not (Test-Path "$($d.OAS_QT_ROOT)\lib\cmake\Qt6\Qt6Config.cmake")) { $need = $true }
            if (-not (Test-Path "$($d.OpenCV_DIR)\OpenCVConfig.cmake")) { $need = $true }
            if (-not (Test-Path "$($d.QHYCCD_INCLUDE_DIR)\qhyccd.h")) { $need = $true }
        } catch { $need = $true }
    }
    if ($need) {
        & "$PSScriptRoot\bootstrap_native_dependencies.ps1" -QtVersion $QtVersion -QhyVersion $QhyVersion
    }
    $d = Get-Content $depRecord -Raw | ConvertFrom-Json
    foreach ($name in @('CMAKE_PREFIX_PATH','OpenCV_DIR','QHYCCD_INCLUDE_DIR','QHYCCD_LIBRARY','QHYCCD_RUNTIME_DIR','QHYCCD_ROOT','ZWO_ASI_INCLUDE_DIR','ZWO_ASI_LIBRARY','ZWO_ASI_RUNTIME_DIR','ZWO_ASI_ROOT','ZWO_EAF_INCLUDE_DIR','ZWO_EAF_LIBRARY','ZWO_EAF_RUNTIME_DIR','ZWO_EAF_ROOT','CANON_EDSDK_INCLUDE_DIR','CANON_EDSDK_LIBRARY','CANON_EDSDK_RUNTIME_DIR','CANON_EDSDK_ROOT')) {
        $prop = $d.PSObject.Properties[$name]
        if ($prop -and $prop.Value) { $cmakeExtra += "-D${name}=$($prop.Value)" }
    }
}

& "$PSScriptRoot/check_build_environment.ps1"

function Resolve-PresetBinaryDir([string]$Name) {
    $all = @()
    $project = Get-Content "$root/CMakePresets.json" -Raw | ConvertFrom-Json
    $all += @($project.configurePresets)
    $userPath = "$root/CMakeUserPresets.json"
    if (Test-Path $userPath) { $all += @((Get-Content $userPath -Raw | ConvertFrom-Json).configurePresets) }

    $seen = @{}
    $current = $Name
    while ($current -and -not $seen.ContainsKey($current)) {
        $seen[$current] = $true
        $preset = $all | Where-Object { $_.name -eq $current } | Select-Object -First 1
        if (-not $preset) { break }
        if ($preset.binaryDir) { return $preset.binaryDir.Replace('${sourceDir}', $root) }
        if ($preset.inherits -is [System.Array]) { $current = [string]$preset.inherits[0] }
        else { $current = [string]$preset.inherits }
    }
    return "$root/build/windows-observatory-msvc-ninja"
}

if ($Clean) {
    $dir = Resolve-PresetBinaryDir $Preset
    Write-Host "Cleaning $dir"
    Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
}
cmake --preset $Preset @cmakeExtra
if ($Jobs -gt 0) { cmake --build --preset $Preset --parallel $Jobs }
else { cmake --build --preset $Preset --parallel }
