@echo off
setlocal
cd /d "%~dp0"
if exist "RoadRashTrilogyRecomp.exe" (
  start "" "RoadRashTrilogyRecomp.exe"
  exit /b 0
)
if exist "build\release02\bin\TrilogyLauncher.exe" (
  start "" "build\release02\bin\TrilogyLauncher.exe"
  exit /b 0
)
if exist "build\native\bin\TrilogyLauncher.exe" (
  start "" "build\native\bin\TrilogyLauncher.exe"
  exit /b 0
)
echo The launcher has not been built. See docs\INSTALLATION.md.
pause
exit /b 1
