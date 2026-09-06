$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ build tools are required.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$testDir = Join-Path $repoRoot 'temp\tests'
New-Item -ItemType Directory -Force $testDir | Out-Null
$testSource = Join-Path $PSScriptRoot 'update_release_test.c'
$testExe = Join-Path $testDir 'update_release_test.exe'
$testObj = Join-Path $testDir 'update_release_test.obj'
& cmd.exe /d /c "`"`"$vcvars`" >nul && cl /nologo /W4 /WX /TC `"$testSource`" /Fo`"$testObj`" /Fe`"$testExe`" && `"$testExe`"`""
if ($LASTEXITCODE) { throw "Release regression tests failed: $LASTEXITCODE" }
