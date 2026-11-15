# Regression test for GitHub issue #2 ("Local compilation failed", C compiler identification unknown).
# Run: powershell -NoProfile -File tests/test_msvc_detection.ps1
# It changes variables only in this process and restores them. Needs MSVC Build Tools and .tools.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $projectRoot 'scripts/msvc-toolchain.ps1')
$cmake = Join-Path $projectRoot '.tools/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
$ninja = Join-Path $projectRoot '.tools/ninja/ninja.exe'
$vcvars = Find-VcVars64
if (-not $vcvars) { throw 'SKIP: Microsoft C++ x64 tools not found.' }
Repair-ProcessPath
# Work folder with spaces and square brackets, like the path reported in the issue.
$work = Join-Path ([IO.Path]::GetTempPath()) ('rr msvc [probe] ' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$failures = 0
function Check([string]$name, [bool]$condition) {
    if ($condition) { Write-Output "PASS  $name" } else { Write-Output "FAIL  $name"; $script:failures++ }
}
function With-Env([hashtable]$vars, [scriptblock]$body) {
    $saved = @{}
    foreach ($k in $vars.Keys) { $saved[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k, $vars[$k]) }
    try { & $body } finally { foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) } }
}
try {
    $clean = Invoke-ToolchainProbe -Cmake $cmake -Ninja $ninja -Vcvars $vcvars.FullName -WorkFolder (Join-Path $work 'clean')
    Check 'clean environment: C and C++ identified as MSVC, compile and link ok' $clean.Ok

    # The proven cause: a stray CL variable (here /Zs = syntax check only, no .obj) breaks identification.
    $poison = @{ CL = '/WX /Zs'; CFLAGS = '-o junk.obj'; CXXFLAGS = '/WX /W4'; _CL_ = '/W4'; LIB = 'Z:\nope'; INCLUDE = 'Z:\nope' }
    With-Env $poison {
        $control = Invoke-ToolchainProbe -Cmake $cmake -Ninja $ninja -Vcvars $vcvars.FullName -WorkFolder (Join-Path $work 'control') -Sanitize $false
        Check 'negative control: poisoned CL without cleanup fails (reproduces the issue)' (-not $control.Ok)
        Check 'negative control: failure is the unknown compiler identification' ($control.Output -match 'identification is unknown|COMPILER_ID is')
        $fixed = Invoke-ToolchainProbe -Cmake $cmake -Ninja $ninja -Vcvars $vcvars.FullName -WorkFolder (Join-Path $work 'fixed')
        Check 'poisoned CL/CFLAGS/CXXFLAGS/LIB/INCLUDE with cleanup: MSVC identified, compile and link ok' $fixed.Ok
    }
    With-Env @{ TEMP = 'Z:\missing'; TMP = 'Z:\missing' } {
        $tmp = Invoke-ToolchainProbe -Cmake $cmake -Ninja $ninja -Vcvars $vcvars.FullName -WorkFolder (Join-Path $work 'tmp')
        Check 'missing TEMP/TMP folder is repaired for the build session' $tmp.Ok
    }
    With-Env @{ CC = 'gcc'; CXX = 'g++'; CMAKE_GENERATOR = 'MinGW Makefiles'; CMAKE_C_COMPILER = 'gcc' } {
        $cc = Invoke-ToolchainProbe -Cmake $cmake -Ninja $ninja -Vcvars $vcvars.FullName -WorkFolder (Join-Path $work 'cc')
        Check 'CC/CXX/CMAKE_GENERATOR from other toolchains are ignored' $cc.Ok
    }
} finally {
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
}
if ($failures -gt 0) { Write-Output "$failures check(s) failed."; exit 1 }
Write-Output 'All toolchain detection checks passed.'
exit 0
