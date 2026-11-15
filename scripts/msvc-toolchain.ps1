# Shared MSVC helpers for build.ps1 and the toolchain regression test (dot-source this file).
# Nothing here changes the user's machine-wide settings: every cleanup applies only to the
# temporary cmd.exe session that runs CMake, through "set VAR=" lines in a generated batch file.

# Environment variables that silently change how cl.exe / CMake behave. The CL variable is the
# one that was proven to break detection (GitHub issue #2): cl.exe prepends its value to every
# command line, so a stray option such as /Zs, /E or /P makes the compiler-identification probe
# produce no object file. CMake then reports "compiler identification is unknown", falls back to
# GNU-style flags (-o file.obj -c) and the linker fails with LNK1181 on testCCompiler.c.obj.
$script:MsvcPoisonVariables = @(
    'CL', '_CL_', 'LINK', '_LINK_', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS', 'CPPFLAGS',
    'INCLUDE', 'LIB', 'LIBPATH', 'CMAKE_GENERATOR', 'CMAKE_GENERATOR_PLATFORM',
    'CMAKE_GENERATOR_TOOLSET', 'CMAKE_TOOLCHAIN_FILE', 'CMAKE_PREFIX_PATH', 'CMAKE_BUILD_TYPE',
    'CMAKE_C_COMPILER', 'CMAKE_CXX_COMPILER', 'CMAKE_C_FLAGS', 'CMAKE_CXX_FLAGS',
    'CMAKE_MAKE_PROGRAM', 'CMAKE_EXPORT_COMPILE_COMMANDS'
)

function Find-VcVars64 {
    $vsRoot = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/2022'
    $vcvars = Get-ChildItem -Path "$vsRoot/*/VC/Auxiliary/Build/vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $detected = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($detected) {
            $candidate = Join-Path $detected 'VC/Auxiliary/Build/vcvars64.bat'
            if (Test-Path -LiteralPath $candidate) { $vcvars = Get-Item -LiteralPath $candidate }
        }
    }
    return $vcvars
}

# PATH entries wrapped in double quotes break cmd.exe/vcvars; strip them for this process only.
function Repair-ProcessPath {
    $env:PATH = (($env:PATH -split ';') | ForEach-Object { $_.Replace('"', '').Trim() } | Where-Object { $_ }) -join ';'
}

# Batch lines that must run BEFORE vcvars64.bat: unset every variable that can change compiler
# behaviour, and repair TEMP/TMP when they point to a missing folder.
function Get-CleanEnvBatchLines([string]$FallbackTemp) {
    $lines = @()
    $names = @($script:MsvcPoisonVariables)
    $names += @(Get-ChildItem Env: | Where-Object { $_.Name -like 'CMAKE_*' } | ForEach-Object { $_.Name })
    foreach ($name in ($names | Sort-Object -Unique)) { $lines += ('set "{0}="' -f $name) }
    $badTemp = $false
    foreach ($t in @($env:TEMP, $env:TMP)) { if (-not $t -or -not (Test-Path -LiteralPath $t -PathType Container)) { $badTemp = $true } }
    if ($badTemp) {
        New-Item -ItemType Directory -Path $FallbackTemp -Force | Out-Null
        $lines += ('set "TEMP={0}"' -f $FallbackTemp)
        $lines += ('set "TMP={0}"' -f $FallbackTemp)
    }
    return $lines
}

# Describes which sensitive variables are set (names and values; none of them hold secrets).
function Get-ToolchainEnvReport {
    $report = @()
    foreach ($name in $script:MsvcPoisonVariables) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if ($value) { $report += ('{0}={1}' -f $name, $value) }
    }
    return $report
}

# Builds, in a scratch folder, one C and one C++ program with the same cmd.exe session recipe the
# real build uses, and requires CMAKE_C_COMPILER_ID = CMAKE_CXX_COMPILER_ID = MSVC plus a
# successful compile and link of both. Returns @{Ok=bool; Output=string}.
function Invoke-ToolchainProbe {
    param(
        [Parameter(Mandatory)][string]$Cmake,
        [Parameter(Mandatory)][string]$Ninja,
        [Parameter(Mandatory)][string]$Vcvars,
        [Parameter(Mandatory)][string]$WorkFolder,
        [bool]$Sanitize = $true
    )
    $src = Join-Path $WorkFolder 'src'
    $bin = Join-Path $WorkFolder 'bin'
    if (Test-Path -LiteralPath $WorkFolder) { Remove-Item -LiteralPath $WorkFolder -Recurse -Force }
    New-Item -ItemType Directory -Path $src -Force | Out-Null
    $utf8 = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText((Join-Path $src 'CMakeLists.txt'), @'
cmake_minimum_required(VERSION 3.20)
project(rr_toolchain_probe LANGUAGES C CXX)
foreach(lang C CXX)
  if(NOT CMAKE_${lang}_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "CMAKE_${lang}_COMPILER_ID is '${CMAKE_${lang}_COMPILER_ID}', expected MSVC")
  endif()
endforeach()
add_executable(probe_c probe.c)
add_executable(probe_cpp probe.cpp)
'@, $utf8)
    [IO.File]::WriteAllText((Join-Path $src 'probe.c'), "int main(void){return 0;}`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $src 'probe.cpp'), "int main(){return 0;}`n", $utf8)
    $lines = @('@echo off')
    if ($Sanitize) { $lines += Get-CleanEnvBatchLines (Join-Path $WorkFolder 'tmp') }
    $lines += ('call "{0}" >nul 2>nul' -f $Vcvars)
    $lines += 'if errorlevel 1 exit /b 1'
    $lines += ('"{0}" -S "{1}" -B "{2}" -G Ninja -DCMAKE_MAKE_PROGRAM="{3}" -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=Release' -f $Cmake, $src, $bin, $Ninja)
    $lines += 'if errorlevel 1 exit /b 1'
    $lines += ('"{0}" --build "{1}"' -f $Cmake, $bin)
    $lines += 'exit /b %errorlevel%'
    $batch = Join-Path $WorkFolder 'probe.cmd'
    [IO.File]::WriteAllLines($batch, $lines, $utf8)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $text = (& cmd.exe /d /c "`"$batch`"" 2>&1 | ForEach-Object { "$_" } | Out-String); $exit = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previous }
    $ok = ($exit -eq 0) -and (Test-Path -LiteralPath (Join-Path $bin 'probe_c.exe')) -and (Test-Path -LiteralPath (Join-Path $bin 'probe_cpp.exe'))
    return @{ Ok = $ok; Output = $text }
}
