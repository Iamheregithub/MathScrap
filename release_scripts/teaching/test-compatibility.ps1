$ErrorActionPreference = 'Stop'
$fork = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$original = (Resolve-Path (Join-Path $fork '..\OpenBoardOriginal')).Path
$root = Join-Path $fork 'dist\compatibility'
New-Item -ItemType Directory -Force $root | Out-Null
$env:LESSONBOARD_COMPAT_DIR = $root
$env:QTWEBENGINE_CHROMIUM_FLAGS = '--disable-gpu'
Remove-Item Env:TEACHING_SMOKE_REPORT -ErrorAction SilentlyContinue
function Run-Compatibility([string]$App,[string]$Mode) {
    $env:LESSONBOARD_COMPAT_MODE=$Mode
    $p=Start-Process $App -ArgumentList @('--file-compatibility-test','-verbose') -WorkingDirectory (Split-Path $App) -PassThru -RedirectStandardError "$root\$Mode-stderr.log" -RedirectStandardOutput "$root\$Mode-stdout.log"
    if (!$p.WaitForExit(150000)) { Stop-Process -Id $p.Id -Force; throw "Compatibility timeout: $Mode" }
    if ($p.ExitCode -ne 0) { throw "Compatibility failure $Mode, exit $($p.ExitCode)" }
    $report=Get-Content "$root\$Mode-report.json" -Raw | ConvertFrom-Json
    if (!$report.passed) {throw "Compatibility failure: $($report.error)"}
    return $report
}
function Folder-Hashes([string]$Path) {
    $r=@{};Get-ChildItem $Path -Recurse -File | ForEach-Object {$r[$_.FullName.Substring($Path.Length)]=(Get-FileHash $_.FullName -Algorithm SHA256).Hash};return $r
}
$first=Run-Compatibility "$original\dist\app\Mathscrap.exe" 'original-create'
$sourceHashes=Folder-Hashes $first.documentPath
$sourceArchiveHash=(Get-FileHash "$root\original.ubz" -Algorithm SHA256).Hash
Run-Compatibility "$fork\dist\app\Mathscrap.exe" 'lessonboard-edit' | Out-Null
$after=Folder-Hashes $first.documentPath
if ($sourceHashes.Count -ne $after.Count) {throw 'Mathscrap changed original lesson file count'}
foreach($k in $sourceHashes.Keys) {if($after[$k] -ne $sourceHashes[$k]) {throw "Mathscrap modified the original lesson: $k"}}
if((Get-FileHash "$root\original.ubz" -Algorithm SHA256).Hash -ne $sourceArchiveHash) {throw 'Original UBZ archive was modified'}
$forkHash=(Get-FileHash "$root\lessonboard.ubz" -Algorithm SHA256).Hash
Run-Compatibility "$original\dist\app\Mathscrap.exe" 'original-edit' | Out-Null
if((Get-FileHash "$root\lessonboard.ubz" -Algorithm SHA256).Hash -ne $forkHash) {throw 'OpenBoard modified the Mathscrap source archive'}
$originalResaveHash=(Get-FileHash "$root\original-resaved.ubz" -Algorithm SHA256).Hash
Run-Compatibility "$fork\dist\app\Mathscrap.exe" 'lessonboard-reopen' | Out-Null
if((Get-FileHash "$root\original-resaved.ubz" -Algorithm SHA256).Hash -ne $originalResaveHash) {throw 'Mathscrap modified the resaved source archive'}
'Original lesson repository and all source UBZ archives preserved byte for byte during cross-application imports.' | Set-Content "$root\source-integrity.log"
python "$fork\release_scripts\teaching\compare-compatibility.py" $root
if($LASTEXITCODE -ne 0) {throw 'Compatibility comparison failed'}
