param(
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [string]$OutputDir = "dist/windows",
    [string]$QtBin = "",
    [string]$OpenCvBin = "",
    [string[]]$VendorRuntimeDirs = @(),
    [switch]$Zip
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path
$build = (Resolve-Path $BuildDir).Path
$out = Join-Path $root $OutputDir
Remove-Item -Recurse -Force $out -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $out | Out-Null
cmake --install $build --prefix $out

$bin = Join-Path $out "bin"
if (-not $QtBin) {
    $qtTool = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($qtTool) { $QtBin = Split-Path $qtTool.Source }
}
if ($QtBin) {
    $windeploy = Join-Path $QtBin "windeployqt.exe"
    foreach ($exe in @("OpenAstroSuite.exe","openastrolink-node.exe","oal-driver-host.exe","oal-hardware-probe.exe")) {
        $path = Join-Path $bin $exe
        if (Test-Path $path) { & $windeploy --release --no-translations --dir $bin $path }
    }
} else {
    Write-Warning "windeployqt was not found. Add Qt bin to PATH or pass -QtBin."
}

function Copy-RuntimeDlls([string]$dir) {
    if (-not $dir -or -not (Test-Path $dir)) { return }
    $crtPattern = '^(?i)(msvcr|msvcp|vcruntime|ucrtbase|concrt|vcamp|atl|mfc|mfcm)[0-9_].*\.dll$|^(?i)(api-ms-win-|ext-ms-win-).+\.dll$'
    Get-ChildItem $dir -Filter *.dll -File -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.Name -match $crtPattern) {
            Write-Host "Skipping Microsoft CRT/UCRT from vendor runtime directory: $($_.Name)"
        } else {
            Copy-Item $_.FullName $bin -Force
        }
    }
}
Copy-RuntimeDlls $OpenCvBin
foreach ($d in $VendorRuntimeDirs) { Copy-RuntimeDlls $d }

# Keep build feature metadata beside the package for support diagnostics.
$features = Join-Path $build "build_features.json"
if (Test-Path $features) { Copy-Item $features (Join-Path $out "build_features.json") -Force }

if ($Zip) {
    $zipPath = "$out.zip"
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path "$out/*" -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "Created $zipPath"
}
Write-Host "Windows package: $out"
