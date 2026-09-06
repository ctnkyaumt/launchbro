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
$testBatch = Join-Path $testDir 'run.cmd'
@(
    '@echo off'
    ('call "{0}" >nul' -f $vcvars)
    'if errorlevel 1 exit /b 1'
    ('cl /nologo /W4 /WX /TC "{0}" /Fo"{1}" /Fe"{2}"' -f $testSource, $testObj, $testExe)
    'if errorlevel 1 exit /b 1'
    ('"{0}"' -f $testExe)
    'exit /b %errorlevel%'
) | Set-Content -LiteralPath $testBatch -Encoding ascii
& cmd.exe /d /c $testBatch
if ($LASTEXITCODE) { throw "Release regression tests failed: $LASTEXITCODE" }
