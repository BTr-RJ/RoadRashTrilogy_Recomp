$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vsRoot = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/2022'
$vcvars = Get-ChildItem -Path "$vsRoot/*/VC/Auxiliary/Build/vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $vcvars) { throw 'Instale Visual Studio 2022 Build Tools com ferramentas C++ x64.' }
$output = Join-Path $projectRoot 'build/native-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$batch = Join-Path $output 'run.cmd'
# All paths below come from the workspace or the detected Visual Studio path.
$lines = @(
    '@echo off',
    ('call "{0}" >nul' -f $vcvars.FullName),
    'if errorlevel 1 exit /b 1',
    ('cl /nologo /std:c++17 /EHsc /W4 /WX /I"{0}\src" "{0}\tests\test_presentation.cpp" /Fe:"{1}\test_presentation.exe" /Fo:"{1}\test_presentation.obj"' -f $projectRoot, $output),
    'if errorlevel 1 exit /b 1',
    ('"{0}\test_presentation.exe"' -f $output),
    'exit /b %errorlevel%'
)
[IO.File]::WriteAllLines($batch, $lines, [Text.Encoding]::Default)
& cmd.exe /d /c "`"$batch`""
exit $LASTEXITCODE
