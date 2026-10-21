$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$env:TEACHING_SMOKE_REPORT = Join-Path $dist 'native-report.json'
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
$process = Start-Process "$dist\app\OpenBoardTeaching.exe" -ArgumentList '--teaching-smoke-test' -WorkingDirectory "$dist\app" -PassThru -RedirectStandardError "$dist\startup-stderr.log" -RedirectStandardOutput "$dist\startup-stdout.log"
if (!$process.WaitForExit(90000)) { Stop-Process -Id $process.Id -Force; throw 'Native smoke test timed out' }
if ($process.ExitCode -ne 0) { throw "Native smoke test failed with exit code $($process.ExitCode)" }
if (!(Test-Path $env:TEACHING_SMOKE_REPORT)) { throw 'Native test did not produce a report' }
$report = Get-Content $env:TEACHING_SMOKE_REPORT -Raw | ConvertFrom-Json
if (!$report.passed) { throw 'Native validation reported failure' }
Get-Content $env:TEACHING_SMOKE_REPORT
