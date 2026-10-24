$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
Remove-Item Env:TEACHING_SMOKE_REPORT -ErrorAction SilentlyContinue
foreach ($scenario in @('autosaved','quick')) {
    $env:LESSONBOARD_HISTORY_TEST_ROOT = Join-Path $dist "normal-save-history-$scenario"
    $env:LESSONBOARD_SAVE_QUICK = if ($scenario -eq 'quick') { '1' } else { '0' }
    foreach ($mode in @('save-write','save-reopen')) {
        $env:LESSONBOARD_TOOLS_TEST_MODE = $mode
        $env:LESSONBOARD_TOOLS_TEST_REPORT = Join-Path $dist "normal-$scenario-$mode-report.json"
        $p = Start-Process (Join-Path $dist 'app/MathScrap.exe') -ArgumentList @('--lesson-tools-test','-verbose') -WorkingDirectory (Join-Path $dist 'app') -PassThru -RedirectStandardError (Join-Path $dist "normal-$scenario-$mode-stderr.log") -RedirectStandardOutput (Join-Path $dist "normal-$scenario-$mode-stdout.log")
        if (!$p.WaitForExit(85000)) { Stop-Process -Id $p.Id -Force; throw "Normal save lifecycle timed out: $scenario/$mode" }
        if ($p.ExitCode -ne 0) { throw "Normal save lifecycle exited $($p.ExitCode): $scenario/$mode" }
        if (!(Test-Path $env:LESSONBOARD_TOOLS_TEST_REPORT)) { throw "No normal save report: $scenario/$mode" }
        $report = Get-Content $env:LESSONBOARD_TOOLS_TEST_REPORT -Raw | ConvertFrom-Json
        if ($mode -eq 'save-write') {
            if (!$report.prepared) { throw "Lesson was not prepared for clean close: $($report.detail)" }
            $session = Get-Content (Join-Path $env:LESSONBOARD_HISTORY_TEST_ROOT 'session.json') -Raw | ConvertFrom-Json
            if (!$session.clean) { throw "Closing the window left recovery marked unclean: $scenario" }
        } elseif (!$report.passed) { throw "Normal restart failed: $($report.detail)" }
        $report
    }
}
Remove-Item Env:LESSONBOARD_TOOLS_TEST_MODE,Env:LESSONBOARD_TOOLS_TEST_REPORT,Env:LESSONBOARD_HISTORY_TEST_ROOT,Env:LESSONBOARD_SAVE_QUICK -ErrorAction SilentlyContinue
