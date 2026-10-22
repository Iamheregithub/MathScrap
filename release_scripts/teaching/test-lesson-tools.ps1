$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dist = Join-Path $project 'dist'
$env:LESSONBOARD_HISTORY_TEST_ROOT = Join-Path $dist 'lesson-history-test'
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
Remove-Item Env:TEACHING_SMOKE_REPORT -ErrorAction SilentlyContinue
foreach ($mode in @('exercise','crash','recover')) {
    $env:LESSONBOARD_TOOLS_TEST_MODE = $mode
    $env:LESSONBOARD_TOOLS_TEST_REPORT = Join-Path $dist "lesson-tools-$mode-report.json"
    $p = Start-Process "$dist\app\LessonBoard.exe" -ArgumentList @('--lesson-tools-test','-verbose') -WorkingDirectory "$dist\app" -PassThru -RedirectStandardError "$dist\lesson-tools-$mode-stderr.log" -RedirectStandardOutput "$dist\lesson-tools-$mode-stdout.log"
    if (!$p.WaitForExit(150000)) { Stop-Process -Id $p.Id -Force; if (Test-Path "$dist\app\teaching-crash.log") { Copy-Item "$dist\app\teaching-crash.log" "$dist\lesson-tools-$mode-crash.log" -Force }; if (Test-Path $env:LESSONBOARD_TOOLS_TEST_REPORT) { Get-Content $env:LESSONBOARD_TOOLS_TEST_REPORT -Raw | Write-Output }; throw "Lesson tools timeout: $mode" }
    if (Test-Path "$dist\app\teaching-crash.log") { Copy-Item "$dist\app\teaching-crash.log" "$dist\lesson-tools-$mode-crash.log" -Force }
    if (Test-Path $env:LESSONBOARD_TOOLS_TEST_REPORT) { Get-Content $env:LESSONBOARD_TOOLS_TEST_REPORT -Raw | Write-Output }
    if ($mode -eq 'crash') {
        if ($p.ExitCode -ne 89 -or !(Test-Path "$dist\crash-tools-state.json")) {throw 'Forced crash fixture was not prepared'}
    } else {
        if ($p.ExitCode -ne 0) {throw "Lesson tools failed: $mode, exit $($p.ExitCode)"}
        $report = Get-Content $env:LESSONBOARD_TOOLS_TEST_REPORT -Raw | ConvertFrom-Json
        if (!$report.passed) {throw "Lesson tools failed: $($report.detail)"}
    }
}
Remove-Item Env:LESSONBOARD_TOOLS_TEST_MODE,Env:LESSONBOARD_TOOLS_TEST_REPORT,Env:LESSONBOARD_HISTORY_TEST_ROOT -ErrorAction SilentlyContinue
