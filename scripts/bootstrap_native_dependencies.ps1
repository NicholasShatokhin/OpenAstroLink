param(
    [string]$QtVersion = "6.10.0",
    [string]$QhyVersion = "26.06.04",
    [switch]$SkipVendor,
    [switch]$SkipOpenCvBootstrap,
    [switch]$RequireCanon
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path
New-Item -ItemType Directory -Force -Path "$root\.oal" | Out-Null

& "$PSScriptRoot\bootstrap_qt_native.ps1" -Version $QtVersion
$qtRecordPath = "$root\.oal\native-qt-windows-x64.json"
if (-not (Test-Path $qtRecordPath)) { throw "Qt bootstrap record not found: $qtRecordPath" }
$qt = Get-Content $qtRecordPath -Raw | ConvertFrom-Json
$aqtPython = "$env:LOCALAPPDATA\OpenAstroLink\tools\aqt-venv\Scripts\python.exe"
if (Test-Path $aqtPython) { & $aqtPython -m pip -q install --upgrade ninja }

$opencvDir = $null
$opencvCandidates = @()
if ($env:OpenCV_DIR) { $opencvCandidates += $env:OpenCV_DIR }
$opencvCandidates += @(
    'C:\opencv\opencv\build\x64\vc16\lib',
    'C:\opencv\build\x64\vc16\lib',
    'C:\opencv\opencv\build',
    'C:\opencv\build',
    "$env:LOCALAPPDATA\OpenAstroLink\vcpkg\installed\x64-windows\share\opencv4"
) | Where-Object { $_ -and (Test-Path $_) }
foreach ($p in $opencvCandidates) {
    if (Test-Path (Join-Path $p 'OpenCVConfig.cmake')) { $opencvDir = $p; break }
    $cfg = Get-ChildItem $p -Recurse -File -Filter OpenCVConfig.cmake -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cfg) { $opencvDir = $cfg.Directory.FullName; break }
}

if (-not $opencvDir -and -not $SkipOpenCvBootstrap) {
    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) { throw "OpenCV was not found and git.exe is required to bootstrap vcpkg/OpenCV." }
    $vcpkg = "$env:LOCALAPPDATA\OpenAstroLink\vcpkg"
    if (-not (Test-Path "$vcpkg\.git")) {
        Write-Host "Cloning vcpkg for per-user OpenCV bootstrap: $vcpkg"
        git clone --depth 1 https://github.com/microsoft/vcpkg.git $vcpkg
    }
    if (-not (Test-Path "$vcpkg\vcpkg.exe")) { & "$vcpkg\bootstrap-vcpkg.bat" -disableMetrics }
    Write-Host "Installing OpenCV x64 through vcpkg ..."
    & "$vcpkg\vcpkg.exe" install opencv4:x64-windows --disable-metrics
    if ($LASTEXITCODE -ne 0) { throw "vcpkg OpenCV installation failed" }
    $candidate = "$vcpkg\installed\x64-windows\share\opencv4"
    if (Test-Path "$candidate\OpenCVConfig.cmake") { $opencvDir = $candidate }
}
if (-not $opencvDir) { throw "OpenCV >= 4 was not found. Set OpenCV_DIR or allow the vcpkg bootstrap." }
Write-Host "OpenCV package: $opencvDir"

$vendor = [ordered]@{}
if (-not $SkipVendor) {
    & "$PSScriptRoot\bootstrap_vendor_sdks.ps1" -QhyVersion $QhyVersion
    $vp = "$root\.oal\native-vendor-windows-x64.json"
    if (-not (Test-Path $vp)) { throw "Vendor SDK record not found: $vp" }
    $vj = Get-Content $vp -Raw | ConvertFrom-Json
    foreach ($prop in $vj.PSObject.Properties) { $vendor[$prop.Name] = $prop.Value }
}

# Canon EDSDK is searched locally but intentionally never downloaded because
# Canon distributes it under its own SDK terms.
$canon = [ordered]@{}
$canonRoots = @()
if ($env:CANON_EDSDK_ROOT) { $canonRoots += $env:CANON_EDSDK_ROOT }
$canonRoots += @(
    'C:\SDK\EDSDK', 'C:\SDK\Canon\EDSDK',
    (Join-Path (Split-Path $root -Parent) 'edsdk')
) | Where-Object { $_ -and (Test-Path $_) }
foreach ($r in $canonRoots) {
    $h = Get-ChildItem $r -Recurse -File -Filter EDSDK.h -ErrorAction SilentlyContinue | Select-Object -First 1
    $l = Get-ChildItem $r -Recurse -File -Filter EDSDK.lib -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match '(?i)(x64|64)' } | Select-Object -First 1
    if (-not $l) { $l = Get-ChildItem $r -Recurse -File -Filter EDSDK.lib -ErrorAction SilentlyContinue | Select-Object -First 1 }
    if ($h -and $l) {
        $canon.CANON_EDSDK_INCLUDE_DIR = $h.Directory.FullName
        $canon.CANON_EDSDK_LIBRARY = $l.FullName
        $dll = Get-ChildItem $r -Recurse -File -Filter EDSDK.dll -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match '(?i)(x64|64|Dll)' } | Select-Object -First 1
        if ($dll) { $canon.CANON_EDSDK_RUNTIME_DIR = $dll.Directory.FullName }
        $canon.CANON_EDSDK_ROOT = $r
        break
    }
}
if ($RequireCanon -and -not $canon.CANON_EDSDK_LIBRARY) { throw "Canon EDSDK is required but was not auto-discovered." }

$record = [ordered]@{
    OAS_QT_ROOT = $qt.OAS_QT_ROOT
    CMAKE_PREFIX_PATH = $qt.CMAKE_PREFIX_PATH
    OAS_QT_VERSION = $qt.OAS_QT_VERSION
    OpenCV_DIR = $opencvDir
}
foreach ($k in $vendor.Keys) { $record[$k] = $vendor[$k] }
foreach ($k in $canon.Keys) { $record[$k] = $canon[$k] }
$out = "$root\.oal\native-deps-windows-x64.json"
$record | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 $out
Write-Host "Native OpenAstroLink dependency environment is ready."
Write-Host "  Qt:         $($qt.OAS_QT_VERSION) ($($qt.OAS_QT_ROOT))"
Write-Host "  OpenCV:     $opencvDir"
Write-Host "  env record: $out"
if ($canon.CANON_EDSDK_LIBRARY) { Write-Host "  Canon:      EDSDK discovered" }
else { Write-Host "  Canon:      not found (manual SDK required for Canon EDSDK builds)" }
