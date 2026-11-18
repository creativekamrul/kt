# Bootstrap a source checkout. Offline laptop kits use setup-windows.ps1.
param([switch]$ToolsOnly, [switch]$Quick)
$ErrorActionPreference = 'Stop'
$ktRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location -LiteralPath $ktRoot

function Invoke-Checked([string]$Exe,[string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe exited with $LASTEXITCODE" }
}

$ktPython = Join-Path $ktRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $ktPython)) {
    Invoke-Checked 'python' @('-m','venv','.venv')
}
Invoke-Checked $ktPython @('-m','pip','install','-r','requirements-dev.txt')

function Get-Source([string]$Name,[string]$Remote,[string]$Commit) {
    $dir = Join-Path $ktRoot ".deps\$Name"
    if (-not (Test-Path -LiteralPath (Join-Path $dir '.git'))) {
        if (Test-Path -LiteralPath $dir) { throw "Existing dependency lacks Git metadata: $dir. Use the offline kit setup for bundled source." }
        Invoke-Checked 'git' @('init','-q',$dir)
        Invoke-Checked 'git' @('-C',$dir,'remote','add','origin',$Remote)
    }
    $head = & git -C $dir rev-parse -q --verify HEAD
    if ($head -ne $Commit) {
        if ($head) { throw "$dir already has a different checkout. Expected $Commit." }
        Invoke-Checked 'git' @('-C',$dir,'fetch','--depth','1','origin',$Commit)
        Invoke-Checked 'git' @('-C',$dir,'checkout','--detach','FETCH_HEAD')
    }
}
Get-Source 'SFML' 'https://github.com/SFML/SFML.git' '5383d2b3948f805af55c9f8a4587ac72ec5981d1'
Get-Source 'freetype' 'https://github.com/freetype/freetype.git' '42608f77f20749dd6ddc9e0536788eaad70ea4b5'
if (-not $ToolsOnly) {
    Invoke-Checked $ktPython @('tools/kt.py','build')
    $checks=@('tools/verify_setup.py')
    if ($Quick) {$checks+='--quick'}
    Invoke-Checked $ktPython $checks
}
