param(
    [Parameter(Mandatory=$true)][string]$RemoteHost,
    [string]$RemoteUser = "pi",
    [string]$HeaderSource = "C:\workspace\astro\QHYCCD_Linux",
    [string]$LibrarySource = "C:\workspace\astro\qhysdk\lib\libqhy.so.0.1.8",
    [string]$RemoteStage = "/tmp/oal-qhy-sdk"
)
$ErrorActionPreference = "Stop"
if (!(Test-Path "$HeaderSource\qhyccd.h")) { throw "qhyccd.h not found under $HeaderSource" }
if (!(Test-Path $LibrarySource)) { throw "QHY library not found: $LibrarySource" }
ssh "$RemoteUser@$RemoteHost" "rm -rf '$RemoteStage' && mkdir -p '$RemoteStage/QHYCCD_Linux' '$RemoteStage/lib'"
scp -r "$HeaderSource\*" "$RemoteUser@${RemoteHost}:$RemoteStage/QHYCCD_Linux/"
scp "$LibrarySource" "$RemoteUser@${RemoteHost}:$RemoteStage/lib/"
Write-Host "QHY SDK staged on $RemoteHost:$RemoteStage"
Write-Host "On the Pi run from the OpenAstroSuite source tree:"
Write-Host "  sudo ./scripts/stage_qhy_sdk.sh $RemoteStage"
