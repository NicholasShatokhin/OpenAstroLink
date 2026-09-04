param(
    [string]$Version = "6.10.0",
    [string]$DestRoot = "$env:LOCALAPPDATA\OpenAstroLink\qt",
    [switch]$Force
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path

function Get-QtVersion([string]$Prefix) {
    $qtpaths = @("$Prefix\bin\qtpaths6.exe", "$Prefix\bin\qtpaths.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($qtpaths) {
        $v = & $qtpaths --qt-version 2>$null
        if ($LASTEXITCODE -eq 0 -and $v) { return $v.Trim() }
    }
    $vf = "$Prefix\lib\cmake\Qt6\Qt6ConfigVersion.cmake"
    if (Test-Path $vf) {
        $m = Select-String -Path $vf -Pattern 'set\(PACKAGE_VERSION\s+"([^"]+)"' | Select-Object -First 1
        if ($m) { return $m.Matches[0].Groups[1].Value }
    }
    return $null
}

function Test-QtPrefix([string]$Prefix) {
    if (-not $Prefix -or -not (Test-Path "$Prefix\lib\cmake\Qt6\Qt6Config.cmake")) { return $false }
    foreach ($mod in @('Core','Gui','Network','Widgets','Concurrent','SerialPort','WebSockets','HttpServer','Positioning')) {
        if (-not (Test-Path "$Prefix\lib\cmake\Qt6$mod\Qt6${mod}Config.cmake")) { return $false }
    }
    $v = Get-QtVersion $Prefix
    if (-not $v) { return $false }
    return ([version]$v -ge [version]'6.4.0')
}

$candidates = New-Object System.Collections.Generic.List[string]
if ($env:OAS_QT_ROOT) { $candidates.Add($env:OAS_QT_ROOT) }
if ($env:QTDIR) { $candidates.Add($env:QTDIR) }
$candidates.Add("$DestRoot\$Version\msvc2022_64")
$candidates.Add("C:\Qt\$Version\msvc2022_64")
if (Test-Path 'C:\Qt') {
    Get-ChildItem 'C:\Qt' -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | ForEach-Object {
        $p = Join-Path $_.FullName 'msvc2022_64'
        if (Test-Path $p) { $candidates.Add($p) }
    }
}
if (Test-Path $DestRoot) {
    Get-ChildItem $DestRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | ForEach-Object {
        $p = Join-Path $_.FullName 'msvc2022_64'
        if (Test-Path $p) { $candidates.Add($p) }
    }
}

if (-not $Force) {
    foreach ($p in ($candidates | Select-Object -Unique)) {
        if (Test-QtPrefix $p) {
            $v = Get-QtVersion $p
            New-Item -ItemType Directory -Force -Path "$root\.oal" | Out-Null
            $rec = [ordered]@{ OAS_QT_ROOT=$p; CMAKE_PREFIX_PATH=$p; OAS_QT_VERSION=$v }
            $out = "$root\.oal\native-qt-windows-x64.json"
            $rec | ConvertTo-Json | Set-Content -Encoding UTF8 $out
            Write-Host "Using native Qt $v: $p"
            Write-Host "Qt environment record: $out"
            exit 0
        }
    }
}

$pythonExe = $null
$pythonPrefix = @()
if (Get-Command py.exe -ErrorAction SilentlyContinue) { $pythonExe = 'py.exe'; $pythonPrefix = @('-3') }
elseif (Get-Command python.exe -ErrorAction SilentlyContinue) { $pythonExe = 'python.exe' }
if (-not $pythonExe) {
    throw "Python 3 is required for automatic Qt download. Install Python 3 or Qt manually, then retry."
}

$venv = "$env:LOCALAPPDATA\OpenAstroLink\tools\aqt-venv"
if (-not (Test-Path "$venv\Scripts\python.exe")) {
    Write-Host "Creating aqtinstall environment: $venv"
    & $pythonExe @pythonPrefix -m venv $venv
}
& "$venv\Scripts\python.exe" -m pip -q install --upgrade pip
& "$venv\Scripts\python.exe" -m pip -q install --upgrade 'aqtinstall>=3.2,<4'

Write-Host "Downloading Qt $Version for Windows/MSVC2022 x64 through aqtinstall ..."
New-Item -ItemType Directory -Force -Path $DestRoot | Out-Null
& "$venv\Scripts\aqt.exe" install-qt windows desktop $Version win64_msvc2022_64 `
    --outputdir $DestRoot `
    --modules qthttpserver qtwebsockets qtserialport qtpositioning
if ($LASTEXITCODE -ne 0) { throw "aqtinstall failed to install Qt $Version" }

$installed = Get-ChildItem "$DestRoot\$Version" -Recurse -File -Filter Qt6Config.cmake -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\lib\\cmake\\Qt6\\Qt6Config\.cmake$' } |
    ForEach-Object { $_.FullName.Substring(0, $_.FullName.Length - '\lib\cmake\Qt6\Qt6Config.cmake'.Length) } |
    Where-Object { Test-QtPrefix $_ } | Select-Object -First 1
if (-not $installed) { throw "Qt was downloaded, but a complete Qt >= 6.4 prefix was not found under $DestRoot\$Version" }
$v = Get-QtVersion $installed
New-Item -ItemType Directory -Force -Path "$root\.oal" | Out-Null
$out = "$root\.oal\native-qt-windows-x64.json"
[ordered]@{ OAS_QT_ROOT=$installed; CMAKE_PREFIX_PATH=$installed; OAS_QT_VERSION=$v } | ConvertTo-Json | Set-Content -Encoding UTF8 $out
Write-Host "Native Qt is ready:"
Write-Host "  version:    $v"
Write-Host "  prefix:     $installed"
Write-Host "  env record: $out"
