# Use a disposable Windows CI profile; fixture lessons are generated and deleted.
param(
    [ValidateSet('baseline','ink','pdf','save','full')][string]$Mode = 'full',
    [switch]$TraceHeap
)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$tag = if ($TraceHeap) { "$Mode-trace" } else { $Mode }
$output = Join-Path $dist "memory/$tag"
New-Item -ItemType Directory -Force $output | Out-Null
$env:TEACHING_SMOKE_REPORT = Join-Path $output 'memory-workload-report.json'
$env:TEACHING_MEMORY_ONLY = '1'
$env:TEACHING_MEMORY_MODE = $Mode
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
$process = $null
$traceStarted = $false
$traceConfigured = $false
$trace = @{ requested=[bool]$TraceHeap; status='not-requested' }
try {
    Remove-Item "$output/memory-samples.jsonl", "$output/memory-workload-report.json", "$output/memory-analysis-report.json" -ErrorAction SilentlyContinue
    if ($TraceHeap) {
        $wpr = Get-Command wpr.exe -ErrorAction SilentlyContinue
        if (!$wpr) { throw 'WPR unavailable: heap trace cannot be captured' }
        $trace.status = 'configuring'
        & $wpr.Source -heaptracingconfig MathScrap.exe enable *> "$output/wpr-config.log"
        if ($LASTEXITCODE -ne 0) { throw 'WPR heap configuration failed; inspect wpr-config.log' }
        $traceConfigured = $true
        & $wpr.Source -start Heap -filemode *> "$output/wpr-start.log"
        if ($LASTEXITCODE -ne 0) { throw 'WPR heap recording failed to start; inspect wpr-start.log' }
        $traceStarted = $true
        $trace.status = 'recording'
    }
    $process = Start-Process "$dist/app/MathScrap.exe" -ArgumentList @('--teaching-smoke-test','-verbose') -WorkingDirectory "$dist/app" -PassThru -RedirectStandardError "$output/memory-stderr.log" -RedirectStandardOutput "$output/memory-stdout.log"
    if (!$process.WaitForExit(540000)) { throw 'Memory workload timed out' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Memory workload failed: exit $($process.ExitCode)" }
    $report = Get-Content $env:TEACHING_SMOKE_REPORT -Raw | ConvertFrom-Json
    if (!$report.passed) { throw "Memory workload failed: $($report.detail)" }
    python "$PSScriptRoot/analyze-memory.py" "$output/memory-samples.jsonl" "$output/memory-analysis-report.json"
    if ($LASTEXITCODE -ne 0) { throw 'Memory growth flagged or measurements invalid; inspect memory reports' }
} finally {
    if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id -Force }
    if ($traceStarted) {
        & $wpr.Source -stop "$output/heap.etl" *> "$output/wpr-stop.log"
        if ($LASTEXITCODE -eq 0 -and (Test-Path "$output/heap.etl")) {
            $trace.status = 'captured'
            $trace.bytes = (Get-Item "$output/heap.etl").Length
        } else { $trace.status = 'capture-failed' }
    } elseif ($TraceHeap) { $trace.status = 'unavailable-or-start-failed' }
    if ($traceConfigured) { & $wpr.Source -heaptracingconfig MathScrap.exe disable *> "$output/wpr-disable.log" }
    $trace | ConvertTo-Json | Set-Content "$output/heap-trace-status.json"
    Remove-Item Env:TEACHING_MEMORY_ONLY, Env:TEACHING_MEMORY_MODE -ErrorAction SilentlyContinue
}
