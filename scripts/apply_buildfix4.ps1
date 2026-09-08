$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$target = Join-Path $repo 'src\backends\synscan_network_mount.cpp'
if (-not (Test-Path $target)) { throw "Target not found: $target" }
$old = 'QString("Axis %1 stop could not be confirmed: %2").arg(axis,last)'
$new = 'QString("Axis %1 stop could not be confirmed: %2").arg(axis).arg(last)'
$text = [System.IO.File]::ReadAllText($target)
if ($text.Contains($new)) {
    Write-Host 'buildfix4 already applied.'
} elseif ($text.Contains($old)) {
    $text = $text.Replace($old, $new)
    [System.IO.File]::WriteAllText($target, $text, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host 'buildfix4 applied.'
} else {
    throw 'Expected old or new QString::arg form was not found; refusing blind edit.'
}
$line = Select-String -Path $target -Pattern 'Axis %1 stop could not be confirmed' | Select-Object -First 1
if (-not $line) { throw 'Verification line not found after edit.' }
Write-Host ("Verified line {0}: {1}" -f $line.LineNumber, $line.Line.Trim())
if ($line.Line -notmatch '\.arg\(axis\)\.arg\(last\)') { throw 'Verification failed: chained arg form is absent.' }
Write-Host 'BUILD_FIX4_VERIFY_PASS'
