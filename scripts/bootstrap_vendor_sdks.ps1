param(
    [string]$QhyVersion = "26.06.04",
    [string]$DestRoot = "$env:LOCALAPPDATA\OpenAstroLink\sdk\native\windows-x64",
    [switch]$SkipQhy,
    [switch]$DiscoverZwoOnly
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path
New-Item -ItemType Directory -Force -Path $DestRoot, "$root\.oal" | Out-Null
$record = [ordered]@{}

function Download-Cached([string]$Url, [string]$Path) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    if ((Test-Path $Path) -and ((Get-Item $Path).Length -gt 0)) { Write-Host "Using cached: $Path"; return }
    Write-Host "Downloading: $Url"
    $tmp = "$Path.part"
    Remove-Item -Force $tmp -ErrorAction SilentlyContinue
    Invoke-WebRequest $Url -OutFile $tmp -UseBasicParsing
    Move-Item -Force $tmp $Path
}

function Expand-ZipTree([string]$Archive, [string]$Dest) {
    Remove-Item -Recurse -Force $Dest -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
    Expand-Archive $Archive -DestinationPath $Dest -Force
    # ZWO's Windows developer download may contain nested platform ZIPs.
    for ($round = 0; $round -lt 3; $round++) {
        $nested = @(Get-ChildItem $Dest -Recurse -File -Filter *.zip -ErrorAction SilentlyContinue | Where-Object { -not $_.FullName.Contains('.oal-expanded') })
        if (-not $nested) { break }
        foreach ($z in $nested) {
            $sub = "$($z.FullName).oal-expanded"
            if (-not (Test-Path $sub)) { Expand-Archive $z.FullName -DestinationPath $sub -Force }
        }
    }
}

function Stage-ZwoWindows([string]$Kind, [string]$SearchRoot, [string]$Stage) {
    if ($Kind -eq 'ASI') {
        $headerName='ASICamera2.h'; $libPattern='ASICamera2.lib'; $dllPattern='ASICamera2.dll'
    } else {
        $headerName='EAF_focuser.h'; $libPattern='*EAF*focuser*.lib'; $dllPattern='*EAF*focuser*.dll'
    }
    $h = Get-ChildItem $SearchRoot -Recurse -File -Filter $headerName -ErrorAction SilentlyContinue | Select-Object -First 1
    $libs = if ($Kind -eq 'ASI') { Get-ChildItem $SearchRoot -Recurse -File -Filter $libPattern -ErrorAction SilentlyContinue } else { Get-ChildItem $SearchRoot -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -like $libPattern } }
    $l = $libs | Where-Object { $_.FullName -match '(?i)(x64|win64|[\\/]64[\\/]|Release)' } | Select-Object -First 1
    if (-not $l) { $l = $libs | Select-Object -First 1 }
    $dlls = if ($Kind -eq 'ASI') { Get-ChildItem $SearchRoot -Recurse -File -Filter $dllPattern -ErrorAction SilentlyContinue } else { Get-ChildItem $SearchRoot -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -like $dllPattern } }
    $d = $dlls | Where-Object { $_.FullName -match '(?i)(x64|win64|[\\/]64[\\/]|Release)' } | Select-Object -First 1
    if (-not $d) { $d = $dlls | Select-Object -First 1 }
    if (-not $h -or -not $l) { return $null }
    Remove-Item -Recurse -Force $Stage -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path "$Stage\include", "$Stage\lib", "$Stage\bin" | Out-Null
    Copy-Item $h.FullName "$Stage\include\$headerName" -Force
    Copy-Item $l.FullName "$Stage\lib\$($l.Name)" -Force
    if ($d) { Copy-Item $d.FullName "$Stage\bin\$($d.Name)" -Force }
    return [ordered]@{ Root=$Stage; Include="$Stage\include"; Library="$Stage\lib\$($l.Name)"; Runtime=$(if($d){"$Stage\bin"}else{$null}) }
}

if (-not $SkipQhy) {
    if ($QhyVersion -notmatch '^\d{2}\.\d{2}\.\d{2}$') { throw "QHY version must be YY.MM.DD" }
    $parts = $QhyVersion.Split('.')
    $key = [int]$parts[0] * 10000 + [int]$parts[1] * 100 + [int]$parts[2]
    if ($key -ge 260604) { $dir = $QhyVersion.Replace('.',''); $file = "sdk_win64_$QhyVersion.zip" }
    else { $dir = $QhyVersion; $file = "sdk_WinMix_$QhyVersion.zip" }
    $cache = "$env:LOCALAPPDATA\OpenAstroLink\cache\qhyccd\$file"
    Download-Cached "https://www.qhyccd.com/file/repository/publish/SDK/$dir/$file" $cache
    $tmp = Join-Path $env:TEMP ("oal-qhy-" + [guid]::NewGuid().ToString('N'))
    Expand-Archive $cache -DestinationPath $tmp -Force
    try {
        $hdr = Get-ChildItem $tmp -Recurse -File -Filter qhyccd.h | Select-Object -First 1
        $lib = Get-ChildItem $tmp -Recurse -File -Filter qhyccd.lib | Where-Object { $_.FullName -match '(?i)(x64|win64|64)' } | Select-Object -First 1
        if (-not $lib) { $lib = Get-ChildItem $tmp -Recurse -File -Filter qhyccd.lib | Select-Object -First 1 }
        $dll = Get-ChildItem $tmp -Recurse -File -Filter qhyccd.dll | Where-Object { $_.FullName -match '(?i)(x64|win64|64)' } | Select-Object -First 1
        if (-not $dll) { $dll = Get-ChildItem $tmp -Recurse -File -Filter qhyccd.dll | Select-Object -First 1 }
        if (-not $hdr -or -not $lib -or -not $dll) { throw "QHY Windows SDK layout changed: qhyccd.h/.lib/.dll not all found" }
        $stage = Join-Path $DestRoot 'qhy'
        Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path "$stage\include", "$stage\lib", "$stage\bin" | Out-Null
        Copy-Item (Join-Path $hdr.Directory.FullName '*.h') "$stage\include" -Force
        Copy-Item $lib.FullName "$stage\lib\qhyccd.lib" -Force
        Copy-Item $dll.FullName "$stage\bin\qhyccd.dll" -Force
        $record.QHYCCD_ROOT = $stage
        $record.QHYCCD_INCLUDE_DIR = "$stage\include"
        $record.QHYCCD_LIBRARY = "$stage\lib\qhyccd.lib"
        $record.QHYCCD_RUNTIME_DIR = "$stage\bin"
        Write-Host "QHY Windows SDK staged: $stage"
    } finally { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }
}

# ZWO: search local SDKs first. If absent, use ZWO's official developer
# download endpoints. The endpoints are product-oriented rather than versioned,
# so the downloaded payload is cached and its exact layout is validated before use.
$zwoAsiCandidates = @(
  'C:\SDK\ZWO\ASI SDK', 'C:\SDK\ZWO\ASI',
  (Join-Path (Split-Path $root -Parent) 'zwo\asi'),
  "$env:ProgramFiles\ZWO Design\ASI SDK"
) | Where-Object { $_ -and (Test-Path $_) }
$zwoEafCandidates = @(
  'C:\SDK\ZWO\EAF', 'C:\SDK\ZWO\EAF SDK',
  (Join-Path (Split-Path $root -Parent) 'zwo\eaf'),
  "$env:ProgramFiles\ZWO Design\EAF SDK"
) | Where-Object { $_ -and (Test-Path $_) }

$asiStaged = $null
foreach ($r in $zwoAsiCandidates) { $asiStaged = Stage-ZwoWindows 'ASI' $r (Join-Path $DestRoot 'zwo\asi'); if ($asiStaged) { break } }
$eafStaged = $null
foreach ($r in $zwoEafCandidates) { $eafStaged = Stage-ZwoWindows 'EAF' $r (Join-Path $DestRoot 'zwo\eaf'); if ($eafStaged) { break } }

if ((-not $asiStaged -or -not $eafStaged) -and -not $DiscoverZwoOnly) {
    $zcache = "$env:LOCALAPPDATA\OpenAstroLink\cache\zwo"
    if (-not $asiStaged) {
        $a = Join-Path $zcache 'ASI_Camera_SDK_latest.zip'
        Download-Cached 'https://dl.zwoastro.com/software?app=DeveloperCameraSdk&platform=windows86&region=Overseas' $a
        $tmp = Join-Path $env:TEMP ('oal-zwo-asi-' + [guid]::NewGuid().ToString('N'))
        try { Expand-ZipTree $a $tmp; $asiStaged = Stage-ZwoWindows 'ASI' $tmp (Join-Path $DestRoot 'zwo\asi') }
        finally { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }
    }
    if (-not $eafStaged) {
        $a = Join-Path $zcache 'EAF_SDK_latest.zip'
        Download-Cached 'https://dl.zwoastro.com/software?app=DeveloperEafSdk&platform=windows86&region=Overseas' $a
        $tmp = Join-Path $env:TEMP ('oal-zwo-eaf-' + [guid]::NewGuid().ToString('N'))
        try { Expand-ZipTree $a $tmp; $eafStaged = Stage-ZwoWindows 'EAF' $tmp (Join-Path $DestRoot 'zwo\eaf') }
        finally { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }
    }
}

if ($asiStaged) {
    $record.ZWO_ASI_ROOT=$asiStaged.Root; $record.ZWO_ASI_INCLUDE_DIR=$asiStaged.Include; $record.ZWO_ASI_LIBRARY=$asiStaged.Library
    if($asiStaged.Runtime){$record.ZWO_ASI_RUNTIME_DIR=$asiStaged.Runtime}
}
if ($eafStaged) {
    $record.ZWO_EAF_ROOT=$eafStaged.Root; $record.ZWO_EAF_INCLUDE_DIR=$eafStaged.Include; $record.ZWO_EAF_LIBRARY=$eafStaged.Library
    if($eafStaged.Runtime){$record.ZWO_EAF_RUNTIME_DIR=$eafStaged.Runtime}
}

$out = "$root\.oal\native-vendor-windows-x64.json"
$record | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 $out
Write-Host "Native vendor SDK record: $out"
if (-not $record.ZWO_ASI_LIBRARY -or -not $record.ZWO_EAF_LIBRARY) {
    Write-Host "NOTE: QHY is pinned/versioned. ZWO Windows uses the official product download endpoint when a local SDK is not found; the payload is cached and layout-validated, but upstream may advance its version."
}
