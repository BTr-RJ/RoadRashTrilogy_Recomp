# Forward arguments verbatim, including ROM paths containing spaces.
$ErrorActionPreference = 'Stop'
$python = Join-Path $PSScriptRoot '.tools/python/python.exe'
if (-not (Test-Path -LiteralPath $python)) {
    throw 'Execute primeiro: powershell -ExecutionPolicy Bypass -File scripts/setup.ps1'
}
& $python (Join-Path $PSScriptRoot 'tools/rr.py') @args
exit $LASTEXITCODE
