param([string]$Target = 'all', [string]$Configuration = 'RelWithDebInfo', [string]$BuildFolder = 'build/native')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'msvc-toolchain.ps1')
# cmd.exe cannot parse PATH entries wrapped in double quotes (vendor installers write some);
# stripping them only normalizes this build process' PATH; the user's machine setting is untouched.
Repair-ProcessPath
$python = Join-Path $projectRoot '.tools/python/python.exe'
& $python (Join-Path $projectRoot 'tools/apply_runtime_patches.py')
if ($LASTEXITCODE -ne 0) { throw 'Failed to apply pinned runtime integration patches.' }
& $python (Join-Path $projectRoot 'tools/prepare_runtime_inputs.py')
if ($LASTEXITCODE -ne 0) { throw 'Failed to prepare verified game inputs.' }
$cmake = Join-Path $projectRoot '.tools/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
$ninja = Join-Path $projectRoot '.tools/ninja/ninja.exe'
$vcvars = Find-VcVars64
if (-not $vcvars) { throw 'Microsoft C++ x64 tools were not found.' }
if ($Target -notmatch '^[a-zA-Z0-9_]+$' -or $Configuration -notin @('Debug','Release','RelWithDebInfo')) { throw 'Invalid build target or configuration.' }
$output = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildFolder))
if (-not $output.StartsWith(($projectRoot + [IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)) { throw 'BuildFolder must stay inside the project.' }
New-Item -ItemType Directory -Path $output -Force | Out-Null

# Toolchain self-test: CMake must identify MSVC for C and C++ and link a probe program, in the same
# sanitized cmd.exe session used below. Fails early with a readable diagnosis instead of a CMake log.
$probe = Invoke-ToolchainProbe -Cmake $cmake -Ninja $ninja -Vcvars $vcvars.FullName -WorkFolder (Join-Path $projectRoot '.local/toolchain-probe')
if (-not $probe.Ok) {
    Write-Output $probe.Output
    Write-Output 'Environment variables that can interfere with the compiler (this session):'
    Get-ToolchainEnvReport | ForEach-Object { Write-Output "  $_" }
    throw 'The Microsoft C/C++ compiler could not compile and link a test program. See the output above.'
}
Write-Output 'Toolchain check: MSVC C and C++ compile and link correctly.'

# A cache created by a previous failed detection keeps the wrong compiler forever; discard it.
$cache = Join-Path $output 'CMakeCache.txt'
if (Test-Path -LiteralPath $cache) {
    # Judge only by what CMake recorded about the detected compilers (both C and C++ must be MSVC).
    $idFiles = @(Get-ChildItem -Path (Join-Path $output 'CMakeFiles') -Include 'CMakeCCompiler.cmake','CMakeCXXCompiler.cmake' -Recurse -ErrorAction SilentlyContinue)
    $badId = $idFiles | Where-Object { (Get-Content -LiteralPath $_.FullName -Raw) -notmatch 'COMPILER_ID "MSVC"' }
    if ($badId) {
        Remove-Item -LiteralPath $cache -Force
        Remove-Item -LiteralPath (Join-Path $output 'CMakeFiles') -Recurse -Force -ErrorAction SilentlyContinue
        Write-Output 'Discarded a stale CMake configuration from a failed earlier attempt.'
    }
}

$batch = Join-Path $output 'build.cmd'
$lines = @('@echo off', 'chcp 65001 >nul')
$lines += Get-CleanEnvBatchLines (Join-Path $projectRoot '.local/tmp')
$lines += @(
    ('call "{0}" >nul 2>nul' -f $vcvars.FullName),
    'if errorlevel 1 exit /b 1',
    ('"{0}" -S "{1}" -B "{2}" -G Ninja -DCMAKE_MAKE_PROGRAM="{3}" -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE={4}' -f $cmake,$projectRoot,$output,$ninja,$Configuration),
    'if errorlevel 1 exit /b 1',
    ('"{0}" --build "{1}" --target {2} --parallel 8' -f $cmake,$output,$Target),
    'exit /b %errorlevel%'
)
[IO.File]::WriteAllLines($batch,$lines,(New-Object Text.UTF8Encoding($false)))
& cmd.exe /d /c "`"$batch`""
exit $LASTEXITCODE
