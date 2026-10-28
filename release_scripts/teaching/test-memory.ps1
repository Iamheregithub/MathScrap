# Run only in a disposable Windows CI user profile: this uses native test fixtures.
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$env:TEACHING_SMOKE_REPORT = Join-Path $dist 'memory-workload-report.json'
$env:TEACHING_MEMORY_ONLY = '1'
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
$process = $null
try {
    Remove-Item "$dist/memory-samples.jsonl", "$dist/memory-workload-report.json", "$dist/memory-analysis-report.json" -ErrorAction SilentlyContinue
    $process = Start-Process "$dist/app/MathScrap.exe" -ArgumentList @('--teaching-smoke-test','-verbose') -WorkingDirectory "$dist/app" -PassThru -RedirectStandardError "$dist/memory-stderr.log" -RedirectStandardOutput "$dist/memory-stdout.log"
    if (!$process.WaitForExit(540000)) { throw 'Memory workload timed out' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Memory workload failed: exit $($process.ExitCode)" }
    $report = Get-Content $env:TEACHING_SMOKE_REPORT -Raw | ConvertFrom-Json
    if (!$report.passed) { throw "Memory workload failed: $($report.detail)" }
    python "$PSScriptRoot/analyze-memory.py" "$dist/memory-samples.jsonl" "$dist/memory-analysis-report.json"
    if ($LASTEXITCODE -ne 0) { throw 'Memory growth flagged or measurements invalid; inspect memory reports' }
} finally {
    if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id -Force }
    Remove-Item Env:TEACHING_MEMORY_ONLY -ErrorAction SilentlyContinue
}
