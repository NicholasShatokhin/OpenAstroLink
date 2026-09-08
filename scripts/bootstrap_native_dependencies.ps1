param(
    [string]$QtVersion = "6.10.0",
    [string]$QhyVersion = "26.06.04",
    [switch]$SkipVendor,
    [switch]$SkipOpenCvBootstrap,
    [switch]$SkipWebRtcBootstrap,
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

function Ensure-Vcpkg {
    # PowerShell functions return every object written to the success pipeline,
    # not only the expression after `return`. Native bootstrap tools are noisy,
    # so route their combined stdout/stderr to the host; otherwise first-run
    # bootstrap text can be captured together with the vcpkg root path.
    $vcpkgRoot = Join-Path $env:LOCALAPPDATA 'OpenAstroLink\vcpkg'
    $vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
    $bootstrap = Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat'

    if (-not (Test-Path (Join-Path $vcpkgRoot '.git'))) {
        if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) { throw "git.exe is required to bootstrap vcpkg dependencies." }
        Write-Host "Cloning vcpkg for per-user native dependencies: $vcpkgRoot"
        & git.exe clone --depth 1 https://github.com/microsoft/vcpkg.git $vcpkgRoot | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "vcpkg clone failed" }
    }

    if (-not (Test-Path $vcpkgExe)) {
        if (-not (Test-Path $bootstrap)) { throw "vcpkg bootstrap script not found: $bootstrap" }
        & $bootstrap -disableMetrics | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed" }
    }
    if (-not (Test-Path $vcpkgExe)) { throw "vcpkg bootstrap completed but executable was not created: $vcpkgExe" }

    # This must be the only success-pipeline value emitted by this function.
    return [string]$vcpkgRoot
}

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
    $vcpkg = Ensure-Vcpkg
    Write-Host "Installing OpenCV x64 through vcpkg ..."
    $vcpkgExe = Join-Path $vcpkg 'vcpkg.exe'
    if (-not (Test-Path $vcpkgExe)) { throw "vcpkg executable not found after bootstrap: $vcpkgExe" }
    & $vcpkgExe install opencv4:x64-windows --disable-metrics
    if ($LASTEXITCODE -ne 0) { throw "vcpkg OpenCV installation failed" }
    $candidate = "$vcpkg\installed\x64-windows\share\opencv4"
    if (Test-Path "$candidate\OpenCVConfig.cmake") { $opencvDir = $candidate }
}
if (-not $opencvDir) { throw "OpenCV >= 4 was not found. Set OpenCV_DIR or allow the vcpkg bootstrap." }
Write-Host "OpenCV package: $opencvDir"

# WebRTC preview transport uses libdatachannel.  Official Windows builds keep
# this dependency per-user through vcpkg so no system-wide SDK is required.
$libDataChannelDir = $null
$webrtcCandidates = @()
if ($env:LibDataChannel_DIR) { $webrtcCandidates += $env:LibDataChannel_DIR }
$webrtcCandidates += @(
    "$env:LOCALAPPDATA\OpenAstroLink\vcpkg\installed\x64-windows\share\libdatachannel"
) | Where-Object { $_ -and (Test-Path $_) }
foreach ($p in $webrtcCandidates) {
    $cfg = Get-ChildItem $p -File -Filter '*DataChannel*Config.cmake' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $cfg) { $cfg = Get-ChildItem $p -File -Filter '*datachannel*config.cmake' -ErrorAction SilentlyContinue | Select-Object -First 1 }
    if ($cfg) { $libDataChannelDir = $cfg.Directory.FullName; break }
}
if (-not $libDataChannelDir -and -not $SkipWebRtcBootstrap) {
    $vcpkg = Ensure-Vcpkg
    Write-Host "Installing libdatachannel x64 through vcpkg for WebRTC preview ..."
    $vcpkgExe = Join-Path $vcpkg 'vcpkg.exe'
    if (-not (Test-Path $vcpkgExe)) { throw "vcpkg executable not found after bootstrap: $vcpkgExe" }
    & $vcpkgExe install libdatachannel:x64-windows --disable-metrics
    if ($LASTEXITCODE -ne 0) { throw "vcpkg libdatachannel installation failed" }
    $candidate = "$vcpkg\installed\x64-windows\share\libdatachannel"
    if (Test-Path $candidate) {
        $cfg = Get-ChildItem $candidate -File -Filter '*Config.cmake' -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cfg) { $libDataChannelDir = $cfg.Directory.FullName }
    }
}
if (-not $libDataChannelDir) { throw "libdatachannel was not found. Set LibDataChannel_DIR or allow the WebRTC bootstrap." }
Write-Host "libdatachannel package: $libDataChannelDir"

$vendor = [ordered]@{}
if (-not $SkipVendor) {
    & "$PSScriptRoot\bootstrap_vendor_sdks.ps1" -QhyVersion $QhyVersion
    $vp = "$root\.oal\native-vendor-windows-x64.json"
    if (-not (Test-Path $vp)) { throw "Vendor SDK record not found: $vp" }
    $vj = Get-Content $vp -Raw | ConvertFrom-Json
    foreach ($prop in $vj.PSObject.Properties) { $vendor[$prop.Name] = $prop.Value }
}

function Get-PeMachine([string]$Path) {
    try {
        $fs = [System.IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
        try {
            $br = New-Object System.IO.BinaryReader($fs)
            if ($br.ReadUInt16() -ne 0x5A4D) { return 'NOT_PE' }
            $fs.Position = 0x3C
            $pe = $br.ReadUInt32()
            if ($pe + 6 -gt $fs.Length) { return 'TRUNCATED' }
            $fs.Position = $pe
            if ($br.ReadUInt32() -ne 0x00004550) { return 'NOT_PE' }
            switch ($br.ReadUInt16()) {
                0x8664 { return 'AMD64' }
                0x014c { return 'I386' }
                0xaa64 { return 'ARM64' }
                0x01c4 { return 'ARMNT' }
                default { return ('0x{0:X4}' -f $_) }
            }
        } finally { $fs.Dispose() }
    } catch { return 'ERROR' }
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
        # Prefer the runtime paired with the selected import library. Canon's
        # Windows SDK contains parallel x86/x64 trees with identical DLL names;
        # a global recursive first-match can silently mix the two SDK halves.
        $dll = $null
        $archRoot = Split-Path -Parent $l.Directory.FullName
        foreach ($leaf in @('Dll','DLL','dll')) {
            $paired = Join-Path (Join-Path $archRoot $leaf) 'EDSDK.dll'
            if ((Test-Path $paired) -and (Get-PeMachine $paired) -eq 'AMD64') {
                $dll = Get-Item $paired
                break
            }
        }
        if (-not $dll) {
            $canonDlls = @(Get-ChildItem $r -Recurse -File -Filter EDSDK.dll -ErrorAction SilentlyContinue | Where-Object { (Get-PeMachine $_.FullName) -eq 'AMD64' })
            $dll = $canonDlls | Select-Object -First 1
        }
        if ($dll) { $canon.CANON_EDSDK_RUNTIME_DIR = $dll.Directory.FullName }
        elseif ($RequireCanon) { throw "Canon EDSDK import library was found under '$r' but no AMD64 EDSDK.dll runtime was found." }
        $canon.CANON_EDSDK_ROOT = $r
        break
    }
}
if ($RequireCanon -and -not $canon.CANON_EDSDK_LIBRARY) { throw "Canon EDSDK is required but was not auto-discovered." }

# Keep the vcpkg installation prefix on CMAKE_PREFIX_PATH as well as Qt.
# LibDataChannelConfig.cmake can refer to transitive packages (OpenSSL,
# usrsctp/libjuice, etc.), and those package configs live beside libdatachannel
# under the same vcpkg installed triplet.
$cmakePrefixes = New-Object System.Collections.Generic.List[string]
if ($qt.CMAKE_PREFIX_PATH) {
    foreach ($prefix in ([string]$qt.CMAKE_PREFIX_PATH -split ';')) {
        if ($prefix -and -not $cmakePrefixes.Contains($prefix)) { $cmakePrefixes.Add($prefix) }
    }
}
$vcpkgInstalledPrefix = "$env:LOCALAPPDATA\OpenAstroLink\vcpkg\installed\x64-windows"
if (Test-Path $vcpkgInstalledPrefix) {
    if (-not $cmakePrefixes.Contains($vcpkgInstalledPrefix)) { $cmakePrefixes.Add($vcpkgInstalledPrefix) }
}

$record = [ordered]@{
    OAS_QT_ROOT = $qt.OAS_QT_ROOT
    CMAKE_PREFIX_PATH = ($cmakePrefixes -join ';')
    OAS_QT_VERSION = $qt.OAS_QT_VERSION
    OpenCV_DIR = $opencvDir
    LibDataChannel_DIR = $libDataChannelDir
}
foreach ($k in $vendor.Keys) { $record[$k] = $vendor[$k] }
foreach ($k in $canon.Keys) { $record[$k] = $canon[$k] }
$out = "$root\.oal\native-deps-windows-x64.json"
$record | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 $out
Write-Host "Native OpenAstroLink dependency environment is ready."
Write-Host "  Qt:         $($qt.OAS_QT_VERSION) ($($qt.OAS_QT_ROOT))"
Write-Host "  OpenCV:     $opencvDir"
Write-Host "  WebRTC:     libdatachannel ($libDataChannelDir)"
Write-Host "  env record: $out"
if ($canon.CANON_EDSDK_LIBRARY) { Write-Host "  Canon:      EDSDK discovered" }
else { Write-Host "  Canon:      not found (manual SDK required for Canon EDSDK builds)" }
