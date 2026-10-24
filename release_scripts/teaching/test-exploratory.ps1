$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$env:TEACHING_SMOKE_REPORT = Join-Path $dist 'exploration-report.json'
$env:TEACHING_EXPLORE_ONLY = '1'
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
try {
    $process = Start-Process (Join-Path $dist 'app/MathScrap.exe') -ArgumentList @('--teaching-smoke-test','-verbose') -WorkingDirectory (Join-Path $dist 'app') -PassThru -RedirectStandardError (Join-Path $dist 'exploration-stderr.log') -RedirectStandardOutput (Join-Path $dist 'exploration-stdout.log')
    if (!$process.WaitForExit(65000)) { Stop-Process -Id $process.Id -Force; throw 'Exploratory interaction check timed out' }
    if (!(Test-Path $env:TEACHING_SMOKE_REPORT)) { throw 'No exploratory interaction report' }
    $report = Get-Content $env:TEACHING_SMOKE_REPORT -Raw | ConvertFrom-Json
    if ($process.ExitCode -ne 0 -or !$report.passed -or !$report.exploratoryInteractionsChecked) {
        throw "Exploratory interaction check failed: $($report.detail) (exit $($process.ExitCode))"
    }
    $report
} finally {
    Remove-Item Env:TEACHING_EXPLORE_ONLY -ErrorAction SilentlyContinue
}
