$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$tests = Join-Path $project 'tests\teaching\recovery-qt'
$build = Join-Path $tests 'build'
$qtbin = Join-Path $env:QT_ROOT_DIR 'bin'
$env:PATH = "$qtbin;$env:PATH"
python (Join-Path $tests 'prepare.py')
if ($LASTEXITCODE -ne 0) { throw 'Recovery test preparation failed' }
New-Item -ItemType Directory -Force $build | Out-Null
Push-Location $build
try {
    & "$qtbin\qmake.exe" (Join-Path $tests 'test.pro') 'CONFIG+=release' 'CONFIG-=debug' 'CONFIG-=debug_and_release'
    if ($LASTEXITCODE -ne 0) { throw 'Recovery test configuration failed' }
    & nmake.exe /NOLOGO
    if ($LASTEXITCODE -ne 0) { throw 'Recovery test compilation failed' }
    $env:QT_QPA_PLATFORM = 'offscreen'
    & .\recovery-qt-test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Recovery responsiveness or integrity checks failed' }
} finally {
    Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    Pop-Location
}
