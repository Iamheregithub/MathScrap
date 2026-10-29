# Use a disposable Windows CI profile; fixture lessons are generated and deleted.
param(
    [ValidateSet('baseline','ink','pdf','save','full')][string]$Mode = 'full',
    [switch]$TraceHeap,
    [switch]$HeapSnapshots
)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$tag = if ($TraceHeap) { "$Mode-trace" } elseif ($HeapSnapshots) { "$Mode-heap" } else { $Mode }
$output = Join-Path $dist "memory/$tag"
New-Item -ItemType Directory -Force $output | Out-Null
$env:TEACHING_SMOKE_REPORT = Join-Path $output 'memory-workload-report.json'
$env:TEACHING_MEMORY_ONLY = '1'
$env:TEACHING_MEMORY_MODE = $Mode
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
$process = $null
$traceStarted = $false
$traceConfigured = $false
$heapConfigured = $false
$heap = @{ requested=[bool]$HeapSnapshots; status='not-requested' }
$trace = @{ requested=[bool]$TraceHeap; status='not-requested' }
try {
    Remove-Item "$output/memory-samples.jsonl", "$output/memory-workload-report.json", "$output/memory-analysis-report.json" -ErrorAction SilentlyContinue
    if ($HeapSnapshots) {
        if ($TraceHeap) { throw 'Use separate processes for WPR and UMDH' }
        $debuggers = "${env:ProgramFiles(x86)}/Windows Kits/10/Debuggers/x64"
        $gflags = Join-Path $debuggers 'gflags.exe'
        $umdh = Join-Path $debuggers 'umdh.exe'
        if (!(Test-Path $gflags) -or !(Test-Path $umdh)) { throw 'Windows x64 debugging tools unavailable' }
        & $gflags /i MathScrap.exe +ust *> "$output/gflags-enable.log"
        if ($LASTEXITCODE -ne 0) { throw 'Allocation stack database configuration failed' }
        $heapConfigured = $true
        & $gflags /i MathScrap.exe /tracedb 512 *> "$output/gflags-size.log"
        if ($LASTEXITCODE -ne 0) { throw 'Could not enlarge allocation stack database' }
        $heap.status = 'configured'
        $env:TEACHING_MEMORY_UMDH = $umdh
        $env:OANOCACHE = '1'
        $env:_NT_SYMBOL_PATH = "$dist/app;srv*$output/symbols*https://msdl.microsoft.com/download/symbols"
    }
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
    if ($heapConfigured) {
        if ((Test-Path "$output/heap-warm.txt") -and (Test-Path "$output/heap-final.txt")) {
            & $umdh -v "$output/heap-warm.txt" "$output/heap-final.txt" *> "$output/heap-diff.txt"
            $heap.status = if ($LASTEXITCODE -eq 0) { 'compared' } else { 'comparison-failed' }
        } else { $heap.status = 'snapshots-incomplete' }
        & $gflags /i MathScrap.exe /tracedb 0 *> "$output/gflags-size-reset.log"
        & $gflags /i MathScrap.exe -ust *> "$output/gflags-disable.log"
    } elseif ($HeapSnapshots) { $heap.status = 'configuration-failed' }
    $heap | ConvertTo-Json | Set-Content "$output/heap-snapshot-status.json"
    Remove-Item Env:TEACHING_MEMORY_UMDH, Env:OANOCACHE, Env:_NT_SYMBOL_PATH -ErrorAction SilentlyContinue
    $trace | ConvertTo-Json | Set-Content "$output/heap-trace-status.json"
    Remove-Item Env:TEACHING_MEMORY_ONLY, Env:TEACHING_MEMORY_MODE -ErrorAction SilentlyContinue
}
