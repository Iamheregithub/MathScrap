$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
function Run-Smoke([string]$Tag, [string]$RelocateTo = '', [string]$Mode = 'move') {
    $env:TEACHING_SMOKE_REPORT = Join-Path $dist "$Tag-report.json"
    $env:TEACHING_TEST_RELOCATE_TO = $RelocateTo
    $env:TEACHING_TEST_RELOCATE_MODE = $Mode
    $process = Start-Process "$dist\app\OpenBoardTeaching.exe" -ArgumentList @('--teaching-smoke-test','-verbose') -WorkingDirectory "$dist\app" -PassThru -RedirectStandardError "$dist\$Tag-stderr.log" -RedirectStandardOutput "$dist\$Tag-stdout.log"
    if (!$process.WaitForExit(180000)) { Stop-Process -Id $process.Id -Force; throw 'Native smoke test timed out' }
    Copy-Item "$dist\app\teaching-crash.log" $dist -ErrorAction SilentlyContinue
    if ($process.ExitCode -ne 0) { throw "Native smoke test failed with exit code $($process.ExitCode)" }
    if (!(Test-Path $env:TEACHING_SMOKE_REPORT)) { throw 'Native test did not produce a report' }
    $report = Get-Content $env:TEACHING_SMOKE_REPORT -Raw | ConvertFrom-Json
    if (!$report.passed) { throw "Native validation failed: $($report.detail)" }
    return $report
}
$moved = Join-Path $dist 'Documents moved by preferences'
$fresh = Join-Path $dist 'Documents without moving'
New-Item -ItemType Directory -Force $moved, $fresh | Out-Null
$first = Run-Smoke 'native' $moved
$original = $first.documentDirectory
$probe = Join-Path $original 'migration-probe.dat'
'OpenBoard storage migration regression' | Set-Content $probe
$probeHash = (Get-FileHash $probe -Algorithm SHA256).Hash
$env:TEACHING_TEST_SKIP_PDF = '1'
$second = Run-Smoke 'storage-moved'
if ($second.documentDirectory -ne $moved.Replace('\','/') -or (Test-Path $original)) { throw 'Document move did not activate on restart' }
if ((Get-FileHash (Join-Path $moved 'migration-probe.dat') -Algorithm SHA256).Hash -ne $probeHash) { throw 'Document move lost or changed content' }
$third = Run-Smoke 'storage-use-scheduled' $fresh 'use'
$fourth = Run-Smoke 'storage-use-applied'
if ($fourth.documentDirectory -ne $fresh.Replace('\','/')) { throw 'Use-folder preference did not activate on restart' }
if ((Get-FileHash (Join-Path $moved 'migration-probe.dat') -Algorithm SHA256).Hash -ne $probeHash) { throw 'Use-folder mode modified the previous documents' }
Remove-Item Env:TEACHING_TEST_SKIP_PDF, Env:TEACHING_TEST_RELOCATE_TO, Env:TEACHING_TEST_RELOCATE_MODE -ErrorAction SilentlyContinue
Get-ChildItem "$env:LOCALAPPDATA\OpenBoardTeaching\log\*.log" -ErrorAction SilentlyContinue | Copy-Item -Destination $dist
$first
'Document move verified across restart; use-without-moving verified across restart; previous documents retained in use mode.' | Set-Content "$dist\storage-validation.log"
