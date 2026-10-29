param([switch]$SkipTrace)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$results = @()
foreach ($mode in @('baseline','ink','pdf','save','full')) {
    # Each lane gets a fresh native process. Do not stop at the first growth flag.
    $failure = $null
    try { & "$PSScriptRoot/test-memory.ps1" -Mode $mode } catch { $failure = $_.Exception.Message }
    $path = Join-Path $project "dist/memory/$mode/memory-analysis-report.json"
    $analysis = if (Test-Path $path) { Get-Content $path -Raw | ConvertFrom-Json } else { $null }
    $results += @{ mode=$mode; failure=$failure; analysis=$analysis }
}
$traceFailure = $null
if (!$SkipTrace) {
    try { & "$PSScriptRoot/test-memory.ps1" -Mode full -TraceHeap } catch { $traceFailure = $_.Exception.Message }
}
@{ lanes=$results; traceRunFailure=$traceFailure; note='Trace run is instrumented; compare uninstrumented lanes separately. Page switching and its automatic persistence remain common to all lanes.' } |
    ConvertTo-Json -Depth 12 | Set-Content (Join-Path $project 'dist/memory/isolation-report.json')
$results | ForEach-Object { Write-Host "$($_.mode): $($_.analysis.status) $($_.failure)" }
if ($traceFailure -or ($results | Where-Object { $_.failure })) { throw 'Isolation diagnostics flagged growth or an incomplete run; inspect per-lane reports and trace status' }
