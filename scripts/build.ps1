param([string]$Target = 'all', [string]$Configuration = 'RelWithDebInfo', [string]$BuildFolder = 'build/native')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $projectRoot '.tools/python/python.exe'
& $python (Join-Path $projectRoot 'tools/apply_runtime_patches.py')
if ($LASTEXITCODE -ne 0) { throw 'Failed to apply pinned runtime integration patches.' }
& $python (Join-Path $projectRoot 'tools/prepare_runtime_inputs.py')
if ($LASTEXITCODE -ne 0) { throw 'Failed to prepare verified game inputs.' }
$cmake = Join-Path $projectRoot '.tools/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
$ninja = Join-Path $projectRoot '.tools/ninja/ninja.exe'
$vsRoot = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/2022'
$vcvars = Get-ChildItem -Path "$vsRoot/*/VC/Auxiliary/Build/vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $detected=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($detected) { $candidate=Join-Path $detected 'VC/Auxiliary/Build/vcvars64.bat';if (Test-Path -LiteralPath $candidate) { $vcvars=Get-Item -LiteralPath $candidate } }
}
if (-not $vcvars) { throw 'Microsoft C++ x64 tools were not found.' }
$output = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildFolder))
if (-not $output.StartsWith(($projectRoot + [IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)) { throw 'BuildFolder must stay inside the project.' }
New-Item -ItemType Directory -Path $output -Force | Out-Null
$batch = Join-Path $output 'build.cmd'
$lines = @(
    '@echo off',
    'chcp 65001 >nul',
    ('call "{0}" >nul' -f $vcvars.FullName),
    'if errorlevel 1 exit /b 1',
    ('"{0}" -S "{1}" -B "{2}" -G Ninja -DCMAKE_MAKE_PROGRAM="{3}" -DCMAKE_BUILD_TYPE={4}' -f $cmake,$projectRoot,$output,$ninja,$Configuration),
    'if errorlevel 1 exit /b 1',
    ('"{0}" --build "{1}" --target {2} --parallel 8' -f $cmake,$output,$Target),
    'exit /b %errorlevel%'
)
if ($Target -notmatch '^[a-zA-Z0-9_]+$' -or $Configuration -notin @('Debug','Release','RelWithDebInfo')) { throw 'Invalid build target or configuration.' }
[IO.File]::WriteAllLines($batch,$lines,(New-Object Text.UTF8Encoding($false)))
& cmd.exe /d /c "`"$batch`""
exit $LASTEXITCODE
