param(
    [string]$GamePath,
    [string]$Compiler
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$GamePath) { $GamePath = Join-Path $root 'out\KT.exe' }
$game = Get-Item -LiteralPath $GamePath
if ($game.Name -ne 'KT.exe') { throw 'Select the bundled KT.exe as installer input.' }
if (!$Compiler) {
    $Compiler = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
}
$project = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
$version = [regex]::Match($project, '\bVERSION (\d+\.\d+\.\d+)').Groups[1].Value
if (!$version) { throw 'Missing KT version in CMakeLists.txt.' }
& $Compiler "/DGamePath=$($game.FullName)" "/DGameVersion=$version" (Join-Path $PSScriptRoot 'kt-installer.iss')
if ($LASTEXITCODE -ne 0) { throw "Installer compiler exited with code $LASTEXITCODE" }
