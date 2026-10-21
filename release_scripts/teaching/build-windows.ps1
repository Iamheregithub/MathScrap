$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$thirdParty = (Resolve-Path (Join-Path $project '..\OpenBoard-ThirdParty')).Path
if (!$env:QT_ROOT_DIR) { throw 'QT_ROOT_DIR is required' }
$qtbin = Join-Path $env:QT_ROOT_DIR 'bin'
function Run-Native([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE" }
}
# Build the upstream Qt-compatible QuaZip rather than reusing a Qt ABI-specific library.
Push-Location (Join-Path $thirdParty 'quazip')
try {
    Run-Native "$qtbin\qmake.exe" @('quazip.pro','CONFIG+=release','CONFIG-=debug','QT+=core5compat')
    Run-Native 'nmake.exe' @('/NOLOGO','release')
} finally { Pop-Location }
Push-Location $project
try {
    Run-Native "$qtbin\qmake.exe" @('OpenBoard.pro','-after','CONFIG+=release','CONFIG-=debug','CONFIG-=debug_and_release')
    Run-Native 'nmake.exe' @('/NOLOGO')
    Run-Native "$qtbin\lrelease.exe" @('OpenBoard.pro')
} finally { Pop-Location }
$exe = Join-Path $project 'build\win32\release\product\OpenBoardTeaching.exe'
if (!(Test-Path $exe)) { throw "Expected application executable is missing: $exe" }
$stage = Join-Path $project 'dist\app'
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item $exe $stage
Get-ChildItem (Split-Path $exe) -Filter *.pdb | Copy-Item -Destination $stage
Run-Native "$qtbin\windeployqt.exe" @('--release','--compiler-runtime',"$stage\OpenBoardTeaching.exe")
foreach ($folder in @('library','fonts','customizations','startupHints')) {
    Copy-Item (Join-Path $project "resources\$folder") $stage -Recurse -Force
}
Copy-Item (Join-Path $project 'resources\etc\*') $stage -Recurse -Force
Copy-Item "$stage\OpenBoard.config" "$stage\OpenBoardTeaching.config"
Copy-Item "$stage\OpenBoard.css" "$stage\OpenBoardTeaching.css"
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
