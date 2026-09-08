param(
    [string]$BuildDir = "build/windows-observatory-vs2022"
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$buildPath = Join-Path $root $BuildDir
if (-not (Test-Path $buildPath)) { throw "Build directory not found: $buildPath" }
$build = (Resolve-Path $buildPath).Path
$drivers = Join-Path $build 'drivers'
$cachePath = Join-Path $build 'CMakeCache.txt'

function Get-PeMachine([string]$Path) {
    try {
        $fs = [System.IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
        try {
            $br = New-Object System.IO.BinaryReader($fs)
            if ($br.ReadUInt16() -ne 0x5A4D) { return 'NOT_PE' }
            $fs.Position = 0x3C; $pe = $br.ReadUInt32()
            if ($pe + 6 -gt $fs.Length) { return 'TRUNCATED' }
            $fs.Position = $pe
            if ($br.ReadUInt32() -ne 0x00004550) { return 'NOT_PE' }
            switch ($br.ReadUInt16()) {
                0x8664 { 'AMD64' }
                0x014c { 'I386' }
                0xaa64 { 'ARM64' }
                0x01c4 { 'ARMNT' }
                default { ('0x{0:X4}' -f $_) }
            }
        } finally { $fs.Dispose() }
    } catch { "ERROR:$($_.Exception.Message)" }
}
function Cache-Value([string]$Name) {
    if (-not (Test-Path $cachePath)) { return $null }
    $line = Select-String -Path $cachePath -Pattern ('^' + [regex]::Escape($Name) + ':[^=]*=(.*)$') | Select-Object -First 1
    if ($line -and $line.Matches.Count) { return $line.Matches[0].Groups[1].Value }
    return $null
}
function Add-Root([System.Collections.Generic.List[string]]$List,[string]$Path) {
    if (-not $Path) { return }
    $p = $Path -replace '/', '\'
    if (Test-Path $p -PathType Leaf) { $p = Split-Path -Parent $p }
    if ((Test-Path $p) -and -not $List.Contains($p)) { $List.Add($p) }
}
function Find-Amd64Dll([string[]]$Names,[string[]]$Roots) {
    $seen = @{}
    foreach ($r in $Roots) {
        if (-not $r -or -not (Test-Path $r)) { continue }
        foreach ($name in $Names) {
            $items = @(Get-ChildItem $r -Recurse -File -Filter $name -ErrorAction SilentlyContinue)
            foreach ($item in $items) {
                if ($seen.ContainsKey($item.FullName)) { continue }; $seen[$item.FullName]=$true
                $m=Get-PeMachine $item.FullName
                Write-Host "  candidate $m  $($item.FullName)"
                if ($m -eq 'AMD64') { return $item }
            }
        }
    }
    return $null
}
function Stage-Runtime([string]$Label,[string[]]$Names,[string[]]$Roots) {
    Write-Host "Resolving $Label AMD64 runtime..."
    $dll = Find-Amd64Dll $Names $Roots
    if (-not $dll) { throw "${Label}: no AMD64 runtime DLL found in candidate SDK roots." }
    foreach ($name in $Names) {
        Remove-Item -Force (Join-Path $build $name) -ErrorAction SilentlyContinue
        Remove-Item -Force (Join-Path $drivers $name) -ErrorAction SilentlyContinue
    }
    # Stage all same-architecture DLLs in the selected runtime directory. Some
    # vendor runtimes have private sibling dependencies beyond the primary DLL.
    $dir=$dll.Directory.FullName
    $copied=0
    foreach($f in Get-ChildItem $dir -File -Filter *.dll -ErrorAction SilentlyContinue) {
        if ((Get-PeMachine $f.FullName) -eq 'AMD64') {
            Copy-Item $f.FullName (Join-Path $build $f.Name) -Force
            Copy-Item $f.FullName (Join-Path $drivers $f.Name) -Force
            $copied++
        }
    }
    Write-Host "$Label runtime repaired from: $dir ($copied AMD64 DLL(s) staged)"
}

$eafRoots = New-Object 'System.Collections.Generic.List[string]'
Add-Root $eafRoots (Cache-Value 'ZWO_EAF_RUNTIME_DIR')
Add-Root $eafRoots (Cache-Value 'ZWO_EAF_LIBRARY')
Add-Root $eafRoots (Cache-Value 'ZWO_EAF_ROOT')
Add-Root $eafRoots 'C:\SDK\ZWO\EAF'
Add-Root $eafRoots "$env:LOCALAPPDATA\OpenAstroLink\sdk\native\windows-x64\zwo\eaf"

$canonRoots = New-Object 'System.Collections.Generic.List[string]'
Add-Root $canonRoots (Cache-Value 'CANON_EDSDK_RUNTIME_DIR')
Add-Root $canonRoots (Cache-Value 'CANON_EDSDK_LIBRARY')
Add-Root $canonRoots (Cache-Value 'CANON_EDSDK_ROOT')
Add-Root $canonRoots 'C:\SDK\EDSDK'
Add-Root $canonRoots 'C:\SDK\Canon\EDSDK'

Stage-Runtime 'ZWO EAF' @('EAF_focuser.dll','EAFFocuser.dll') $eafRoots.ToArray()
Stage-Runtime 'Canon EDSDK' @('EDSDK.dll') $canonRoots.ToArray()

foreach($plugin in 'oal_driver_zwo_eaf.dll','oal_driver_canon.dll') {
    $p=Join-Path $drivers $plugin
    if(Test-Path $p) {
        $m=Get-PeMachine $p
        Write-Host "Plugin $plugin machine: $m"
        if($m -ne 'AMD64') { throw "$plugin itself is not AMD64 ($m). Rebuild the driver." }
    }
}
& "$PSScriptRoot\diagnose_windows_runtime.ps1" -BuildDir $BuildDir -Strict
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host 'WINDOWS_VENDOR_RUNTIME_REPAIR_PASS'
