param(
    [string]$BuildDir = "build/windows-observatory-vs2022",
    [switch]$Strict
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path
$buildPath = Join-Path $root $BuildDir
if (-not (Test-Path $buildPath)) { throw "Build directory not found: $buildPath" }
$build = (Resolve-Path $buildPath).Path
$drivers = Join-Path $build "drivers"
$badCrt = '^(?i)(msvcr|msvcp|vcruntime|ucrtbase|concrt|vcamp|atl|mfc|mfcm)[0-9_].*\.dll$|^(?i)(api-ms-win-|ext-ms-win-).+\.dll$'

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
    } catch { return "ERROR:$($_.Exception.Message)" }
}

Write-Host "Scanning OpenAstroLink Windows runtime under $build"
$allDlls = @(Get-ChildItem $build,$drivers -Filter *.dll -File -ErrorAction SilentlyContinue | Sort-Object FullName -Unique)
$crtHits = @($allDlls | Where-Object { $_.Name -match $badCrt })
if ($crtHits) {
    Write-Warning "Potential app-local Microsoft CRT/UCRT DLL hazards:"
    $crtHits | ForEach-Object { Write-Host "  $($_.FullName)" }
} else {
    Write-Host "PASS: no app-local Microsoft CRT/UCRT DLLs detected."
}

$wrong = @()
Write-Host "PE architecture inventory (expected AMD64):"
foreach ($dll in $allDlls) {
    $m = Get-PeMachine $dll.FullName
    $tag = if ($m -eq 'AMD64') { 'OK' } else { 'BAD' }
    Write-Host ("  [{0}] {1,-7} {2}" -f $tag,$m,$dll.FullName)
    if ($m -ne 'AMD64') { $wrong += [pscustomobject]@{Path=$dll.FullName; Machine=$m} }
}

$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if ($dumpbin) {
    foreach ($pluginName in 'oal_driver_canon.dll','oal_driver_zwo_eaf.dll','EDSDK.dll','EAF_focuser.dll','EAFFocuser.dll') {
        $plugin = Join-Path $drivers $pluginName
        if (-not (Test-Path $plugin)) { $plugin = Join-Path $build $pluginName }
        if (Test-Path $plugin) {
            Write-Host "Dependencies for ${pluginName}:"
            & $dumpbin.Source /dependents $plugin | Select-String -Pattern '\.dll' | ForEach-Object { Write-Host "  $($_.Line.Trim())" }
        }
    }
} else {
    Write-Host "NOTE: dumpbin.exe not in PATH; dependency-name listing skipped."
}

if ($wrong) {
    Write-Warning "Wrong/non-PE DLLs are present in the runtime search path. These can cause Win32 error 193 (%1 is not a valid Win32 application)."
    if ($Strict) { exit 3 }
} elseif ($crtHits -and $Strict) {
    exit 2
} else {
    Write-Host "WINDOWS_RUNTIME_ABI_CHECK_PASS"
}
