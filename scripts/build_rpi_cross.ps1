param(
    [ValidateSet("arm64", "armhf")]
    [string]$Arch = "arm64",
    [Parameter(Mandatory = $true)]
    [string]$Sysroot,
    [string]$Preset = "",
    [string]$QtHostPath = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root
if (-not $Preset) {
    $Preset = if ($Arch -eq "arm64") { "rpi4-cross-arm64-node-windows-release" } else { "rpi-cross-armhf-node-windows-release" }
}
if (-not (Test-Path -LiteralPath $Sysroot -PathType Container)) {
    throw "Target sysroot does not exist: $Sysroot. The automatic sysroot bootstrap currently runs on Linux/WSL; use scripts/bootstrap_rpi_cross.sh there, then pass the resulting/copied sysroot here."
}
Write-Host "Cross preset: $Preset"
Write-Host "Target sysroot: $Sysroot"
$cmakeArgs = @("--preset", $Preset, "-DOAS_CROSS_SYSROOT=$Sysroot")
if ($QtHostPath) {
    if (-not (Test-Path -LiteralPath $QtHostPath -PathType Container)) { throw "Qt host path does not exist: $QtHostPath" }
    Write-Host "Qt host tools: $QtHostPath"
    $cmakeArgs += "-DOAS_QT_HOST_PATH=$QtHostPath"
}
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $Preset --parallel
exit $LASTEXITCODE
