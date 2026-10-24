$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$thirdParty = (Resolve-Path (Join-Path $project '..\OpenBoard-ThirdParty')).Path
if (!$env:QT_ROOT_DIR) { throw 'QT_ROOT_DIR is required' }
$qtbin = Join-Path $env:QT_ROOT_DIR 'bin'
function Run-Native([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE" }
}
# Restore timestamps only when source contents match the cached compilation.
# This lets nmake reuse unchanged objects across fresh GitHub checkouts.
$stampPath = Join-Path $project 'build\teaching-source-stamps.json'
$previous = @{}
if (Test-Path $stampPath) { $previous = Get-Content $stampPath -Raw | ConvertFrom-Json -AsHashtable }
$stamps = @{}
Push-Location $project
try {
    $tracked = & git ls-files
    foreach ($relative in $tracked) {
        $file = Get-Item $relative -ErrorAction SilentlyContinue
        if (!$file -or $file.PSIsContainer) { continue }
        $hash = (Get-FileHash $file.FullName -Algorithm SHA256).Hash
        if ($previous.ContainsKey($relative) -and $previous[$relative].hash -eq $hash) {
            $file.LastWriteTimeUtc = [DateTime]::new([long]$previous[$relative].ticks, [DateTimeKind]::Utc)
        }
        $stamps[$relative] = @{ hash = $hash; ticks = $file.LastWriteTimeUtc.Ticks }
    }
} finally { Pop-Location }
New-Item -ItemType Directory -Force (Split-Path $stampPath) | Out-Null
$stamps | ConvertTo-Json -Depth 4 | Set-Content $stampPath
# Build the upstream Qt-compatible QuaZip rather than reusing a Qt ABI-specific library.
Push-Location (Join-Path $thirdParty 'quazip')
try {
    Run-Native "$qtbin\qmake.exe" @('quazip.pro','CONFIG+=release','CONFIG-=debug','QT+=core5compat')
    Run-Native 'nmake.exe' @('/NOLOGO','release')
} finally { Pop-Location }
Push-Location $project
try {
    Run-Native "$qtbin\qmake.exe" @('OpenBoard.pro','-after','CONFIG+=release','CONFIG-=debug','CONFIG-=debug_and_release','QMAKE_LFLAGS_RELEASE+=/DEBUG')
    Run-Native 'nmake.exe' @('/NOLOGO')
    Run-Native "$qtbin\lrelease.exe" @('OpenBoard.pro')
} finally { Pop-Location }
$exe = Join-Path $project 'build\win32\release\product\OpenBoardTeaching.exe'
if (!(Test-Path $exe)) { throw "Expected application executable is missing: $exe" }
$stage = Join-Path $project 'dist\app'
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item $exe "$stage\MathScrap.exe"
Get-ChildItem (Split-Path $exe) -Filter *.pdb | Copy-Item -Destination $stage
Run-Native "$qtbin\windeployqt.exe" @('--release','--compiler-runtime',"$stage\MathScrap.exe")
foreach ($folder in @('library','fonts','customizations','startupHints')) {
    Copy-Item (Join-Path $project "resources\$folder") $stage -Recurse -Force
}
Copy-Item (Join-Path $project 'resources\etc') $stage -Recurse -Force
Copy-Item "$stage\etc\OpenBoard.config" "$stage\etc\OpenBoardTeaching.config"
Copy-Item "$stage\etc\OpenBoard.css" "$stage\etc\OpenBoardTeaching.css"
New-Item -ItemType Directory -Force "$stage\i18n" | Out-Null
Copy-Item (Join-Path $project 'resources\i18n\*.qm') "$stage\i18n" -Force
Copy-Item (Join-Path $thirdParty 'poppler\bin\*.dll') $stage -Force
Copy-Item (Join-Path $thirdParty 'openssl\openssl-3.0.15-win64\bin\*.dll') $stage -Force
Copy-Item (Join-Path $thirdParty 'zlib\1.2.11\bin\*.dll') $stage -Force
Copy-Item (Join-Path $project 'LICENSE') "$stage\LICENSE.txt"
# Preserve dependency notices in the package.
if (Test-Path "$thirdParty\poppler\share") { Copy-Item "$thirdParty\poppler\share" "$stage\third-party-notices" -Recurse -Force }
# Include Microsoft's official runtime installer for machines missing a recent CRT.
$redist = Join-Path $project 'dist\vc_redist.x64.exe'
Invoke-WebRequest 'https://aka.ms/vs/17/release/vc_redist.x64.exe' -OutFile $redist
$signature = Get-AuthenticodeSignature $redist
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notlike '*Microsoft Corporation*') {
    throw 'The downloaded Visual C++ runtime did not have a valid Microsoft signature'
}
