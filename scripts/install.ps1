param(
    [Parameter(Mandatory=$true)][string]$Destination,
    [string]$RomFolder1='', [string]$RomFolder2='', [string]$RomFolder3='',
    [string]$RomPathsFile='', [switch]$SkipBuild, [switch]$NoToolInstall
)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=New-Object Text.UTF8Encoding($false)
$OutputEncoding=[Console]::OutputEncoding
[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
$packageRoot=Split-Path -Parent $PSScriptRoot
$installRoot=[IO.Path]::GetFullPath($Destination)
$sourceRoot=[IO.Path]::GetFullPath($packageRoot)
if ($installRoot -eq [IO.Path]::GetPathRoot($installRoot)) { throw 'Choose a dedicated installation folder, not a drive root.' }
if ($installRoot.StartsWith($sourceRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Choose an installation folder outside the installer package.' }
$marker=Join-Path $installRoot '.local/installation.json'
if (Test-Path -LiteralPath $installRoot) {
    $contents=@(Get-ChildItem -LiteralPath $installRoot -Force)
    if ($contents.Count -gt 0) {
        if (-not (Test-Path -LiteralPath $marker)) { throw 'The destination is not empty. Choose a new empty folder or an existing Road Rash Trilogy installation.' }
        $installed=Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
        if ($installed.product -ne 'RoadRashTrilogyRecomp') { throw 'This folder belongs to a different product.' }
    }
}
$romFolders=@($RomFolder1,$RomFolder2,$RomFolder3) | Where-Object { $_ }
if ($RomPathsFile) { $romFolders=@(Get-Content -LiteralPath $RomPathsFile -Encoding UTF8 | Where-Object { $_ }) }
if ($romFolders.Count -eq 0) { throw 'Select at least one folder containing your own compatible ROMs.' }
foreach ($folder in $romFolders) { if (-not (Test-Path -LiteralPath $folder)) { throw "ROM folder not found: $folder" } }
Write-Output '[STAGE 1/7] Preparing installation folder'
New-Item -ItemType Directory -Path $installRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $installRoot '.local') -Force | Out-Null
@{product='RoadRashTrilogyRecomp';version='0.50';status='installing'} | ConvertTo-Json | Set-Content -LiteralPath $marker -Encoding UTF8
if ($sourceRoot -ne $installRoot) {
    foreach ($name in @('src','tools','scripts','games','assets','patches','docs','config','tests','music')) {
        $from=Join-Path $sourceRoot $name
        if (Test-Path -LiteralPath $from) { Copy-Item -LiteralPath $from -Destination $installRoot -Recurse -Force }
    }
    foreach ($name in @('CMakeLists.txt','RoadRashTrilogyRecomp.exe','SDL2.dll','README.md','CREDITS.md','THIRD_PARTY_NOTICES.md','AGENTS.md','PRODUCT.md','DESIGN.md','Jogar.cmd')) {
        $from=Join-Path $sourceRoot $name
        if (Test-Path -LiteralPath $from) { Copy-Item -LiteralPath $from -Destination $installRoot -Force }
    }
}
$cache=Join-Path $installRoot '.tools/downloads'
New-Item -ItemType Directory -Path $cache -Force | Out-Null
if (Test-Path -LiteralPath (Join-Path $sourceRoot 'Tools/downloads')) {
    Get-ChildItem -LiteralPath (Join-Path $sourceRoot 'Tools/downloads') -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $cache -Force
    }
}
Write-Output '[STAGE 2/7] Verifying Python bootstrap'
$lock=Get-Content -LiteralPath (Join-Path $installRoot 'tools/public-tools.lock.json') -Raw | ConvertFrom-Json
$pyEntry=$lock.downloads | Where-Object { $_.name -eq 'python' }
$archive=Join-Path $cache $pyEntry.archive
if (-not (Test-Path -LiteralPath $archive)) { Invoke-WebRequest -UseBasicParsing -Uri $pyEntry.url -OutFile $archive }
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pyEntry.sha256) { throw 'Python archive checksum mismatch. Nothing was extracted.' }
$pythonHome=Join-Path $installRoot '.tools/python'
if (-not (Test-Path -LiteralPath (Join-Path $pythonHome 'python.exe'))) { Expand-Archive -LiteralPath $archive -DestinationPath $pythonHome -Force }
$python=Join-Path $pythonHome 'python.exe'
Write-Output '[STAGE 3/7] Finding and verifying your ROM revisions'
$arguments=@((Join-Path $installRoot 'tools/install_roms.py'))
foreach ($folder in $romFolders) { $arguments+=@('--rom-dir',$folder) }
& $python @arguments
if ($LASTEXITCODE -ne 0) { throw 'No compatible ROMs were staged. Check the supported revisions in the installation guide.' }
if ($SkipBuild) { Write-Output '[STAGE 7/7] ROM validation complete (build intentionally skipped)'; exit 0 }
Write-Output '[STAGE 4/7] Preparing verified SDK dependencies'
& $python (Join-Path $installRoot 'tools/bootstrap_public.py')
if ($LASTEXITCODE -ne 0) { throw 'Dependency preparation failed. Check your Internet connection and the log.' }
Write-Output '[STAGE 5/7] Checking the Microsoft C++ toolchain'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vcRoot=$null
if (Test-Path -LiteralPath $vswhere) { $vcRoot=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
if (-not $vcRoot) {
    $fallback=Get-ChildItem -Path "${env:ProgramFiles(x86)}/Microsoft Visual Studio/2022/*/VC/Auxiliary/Build/vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $fallback) {
        if ($NoToolInstall) { throw 'Microsoft C++ Build Tools are required; automatic tool installation was disabled.' }
        Write-Output 'Microsoft C++ Build Tools are missing. Windows may request administrator approval.'
        $vsInstaller=Join-Path $cache 'vs_BuildTools.exe'
        Invoke-WebRequest -UseBasicParsing -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile $vsInstaller
        $signature=Get-AuthenticodeSignature -LiteralPath $vsInstaller
        if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') { throw 'The Microsoft installer signature could not be verified.' }
        $setup=Start-Process -FilePath $vsInstaller -ArgumentList '--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended' -Verb RunAs -WindowStyle Hidden -Wait -PassThru
        if ($setup.ExitCode -notin @(0,3010)) { throw "Microsoft Build Tools installation failed ($($setup.ExitCode))." }
    }
}
Write-Output '[STAGE 6/7] Generating and compiling the games locally'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $installRoot 'scripts/build.ps1') -BuildFolder 'build/native'
if ($LASTEXITCODE -ne 0) { throw 'Local compilation failed. Your original ROM folders were not modified. See the installation log.' }
Write-Output '[STAGE 7/7] Finalizing installation'
$installedGames=@()
foreach ($game in @('rr1','rr2','rr3')) { if (Test-Path -LiteralPath (Join-Path $installRoot "build/native/bin/$game/$game.exe")) { $installedGames+=$game } }
if ($installedGames.Count -eq 0) { throw 'No game executable was produced.' }
@{product='RoadRashTrilogyRecomp';version='0.50';games=$installedGames;installed_utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath $marker -Encoding UTF8
$shortcut=Join-Path $installRoot 'Road Rash Trilogy Recomp.lnk'
if (Test-Path -LiteralPath (Join-Path $installRoot 'RoadRashTrilogyRecomp.exe')) {
    $link=(New-Object -ComObject WScript.Shell).CreateShortcut($shortcut)
    $link.TargetPath=Join-Path $installRoot 'RoadRashTrilogyRecomp.exe';$link.WorkingDirectory=$installRoot;$link.Save()
}
Write-Output "Installation complete. Games ready: $($installedGames -join ', ')."
