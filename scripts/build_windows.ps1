param(
    [string]$Preset = "windows-core-release",
    [switch]$Clean,
    [int]$Jobs = 0
)
$ErrorActionPreference = "Stop"

if ($Clean) {
    $presetJson = Get-Content "$PSScriptRoot/../CMakePresets.json" -Raw | ConvertFrom-Json
    $p = $presetJson.configurePresets | Where-Object { $_.name -eq $Preset }
    if ($p -and $p.binaryDir) {
        $dir = $p.binaryDir.Replace('${sourceDir}', (Resolve-Path "$PSScriptRoot/..").Path)
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

cmake --preset $Preset
if ($Jobs -gt 0) { cmake --build --preset $Preset --parallel $Jobs }
else { cmake --build --preset $Preset --parallel }
