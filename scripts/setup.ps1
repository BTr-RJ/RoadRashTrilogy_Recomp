# Downloads stay local to this project. Does not change PATH or install globally.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot '.tools'
$downloads = Join-Path $toolsRoot 'downloads'
$lock = Get-Content -LiteralPath (Join-Path $projectRoot 'tools/tools.lock.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Path $downloads -Force | Out-Null
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

foreach ($name in @('python', 'genesisrecomp')) {
    $entry = $lock.$name
    $archive = Join-Path $downloads $entry.archive
    if (-not (Test-Path -LiteralPath $archive)) {
        Write-Host "Baixando $name $($entry.version)..."
        Invoke-WebRequest -UseBasicParsing -Uri $entry.url -OutFile $archive
    }
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $entry.sha256) {
        throw "SHA256 incorreto: $archive. Arquivo mantido para inspecao; nenhuma extracao realizada."
    }
    $destination = Join-Path $toolsRoot $entry.directory
    Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
    Write-Host "$name $($entry.version): SHA256 verificado."
}
& (Join-Path $toolsRoot 'python/python.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'Falha ao iniciar Python.' }
& (Join-Path $toolsRoot 'genesisrecomp/GenesisRecomp.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'Falha ao iniciar GenesisRecomp.' }
Write-Host 'Preparado. Execute: .\rr.ps1 doctor'
