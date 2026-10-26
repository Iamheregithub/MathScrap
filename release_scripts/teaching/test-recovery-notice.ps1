$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
$env:LESSONBOARD_HISTORY_TEST_ROOT = Join-Path $dist 'recovery-notice-history'
Remove-Item Env:TEACHING_SMOKE_REPORT -ErrorAction SilentlyContinue
function Run-Recovery([string]$Mode, [int]$ExpectedExit = 0) {
    $env:LESSONBOARD_TOOLS_TEST_MODE = $Mode
    $env:LESSONBOARD_TOOLS_TEST_REPORT = Join-Path $dist "$Mode-report.json"
    $p = Start-Process (Join-Path $dist 'app/MathScrap.exe') -ArgumentList @('--lesson-tools-test','-verbose') -WorkingDirectory (Join-Path $dist 'app') -PassThru -RedirectStandardError (Join-Path $dist "$Mode-stderr.log") -RedirectStandardOutput (Join-Path $dist "$Mode-stdout.log")
    if (!$p.WaitForExit(70000)) { Stop-Process -Id $p.Id -Force; throw "Recovery test timed out: $Mode" }
    if (!(Test-Path $env:LESSONBOARD_TOOLS_TEST_REPORT)) { throw "Missing recovery report: $Mode" }
    $report = Get-Content $env:LESSONBOARD_TOOLS_TEST_REPORT -Raw | ConvertFrom-Json
    $report | ConvertTo-Json -Compress | Write-Host
    if ($p.ExitCode -ne $ExpectedExit) { throw "Recovery test exited $($p.ExitCode), expected $ExpectedExit ($Mode): $($report.detail)" }
    if ($ExpectedExit -eq 89 -or $Mode -in @('recovery-dismiss','recovery-delete','save-checkpoint-failure')) {
        if (!$report.prepared) { throw "Recovery fixture not prepared ($Mode): $($report.detail)" }
    } elseif (!$report.passed) { throw "Recovery test failed ($Mode): $($report.detail)" }
    return $report
}
Run-Recovery 'recovery-seed' 89
Run-Recovery 'recovery-dismiss'
Run-Recovery 'recovery-reopen'
Run-Recovery 'recovery-seed' 89
Run-Recovery 'recovery-dismiss-crash' 89
Run-Recovery 'recovery-reopen-ack'
Run-Recovery 'recovery-seed' 89
Run-Recovery 'recovery-review'
Run-Recovery 'recovery-seed' 89
Run-Recovery 'recovery-delete-cancel' 89
Run-Recovery 'recovery-delete'
if (Get-ChildItem $env:LESSONBOARD_HISTORY_TEST_ROOT -Directory | Where-Object { $_.Name -match '^[0-9a-fA-F-]{36}$' }) { throw 'Delete all left lesson history folders after closing' }
Run-Recovery 'recovery-delete-reopen'
Run-Recovery 'recovery-explicit-version'
Run-Recovery 'recovery-new-work'
$failure = Run-Recovery 'save-checkpoint-failure'
$session = Get-Content (Join-Path $env:LESSONBOARD_HISTORY_TEST_ROOT 'session.json') -Raw | ConvertFrom-Json
if (!$session.clean -or !$session.checkpointError) { throw 'A failed recovery backup was marked as a crash or lost its error' }
Remove-Item -LiteralPath $failure.blocked -Force
Move-Item -LiteralPath $failure.held -Destination $failure.blocked
Run-Recovery 'recovery-failure-dismiss'
Run-Recovery 'recovery-reopen'
Run-Recovery 'recovery-delete-damaged'
$env:LESSONBOARD_HISTORY_TEST_ROOT = Join-Path $dist 'recovery-empty-history'
Run-Recovery 'recovery-empty-seed' 89
Run-Recovery 'recovery-empty-reopen'
Remove-Item Env:LESSONBOARD_TOOLS_TEST_MODE,Env:LESSONBOARD_TOOLS_TEST_REPORT,Env:LESSONBOARD_HISTORY_TEST_ROOT -ErrorAction SilentlyContinue
