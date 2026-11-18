param([switch]$ToolsOnly, [switch]$Quick)
$ErrorActionPreference = 'Stop'
$ktRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $ktRoot
if (-not [Environment]::Is64BitOperatingSystem) { throw 'This kit needs 64-bit Windows.' }
$ktCache = Join-Path $ktRoot '.cache'
$ktTmp = Join-Path $ktCache 'tmp'
New-Item -ItemType Directory -Path $ktTmp, (Join-Path $ktRoot 'out') -Force | Out-Null
$env:TEMP = $ktTmp
$env:TMP = $ktTmp
$env:PYTHONNOUSERSITE = '1'
$env:SDP_FORCE_ZIG = '1'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $ktCache 'zig'
$env:MPLCONFIGDIR = Join-Path $ktCache 'matplotlib'

function Invoke-KtChecked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code ${LASTEXITCODE}: $Exe" }
}

function Get-KtHash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $sha.Dispose() }
}

function Expand-KtArchive([string]$Archive, [string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $base = [IO.Path]::GetFullPath($Destination).TrimEnd('\') + '\'
    if (-not $base.StartsWith($ktRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Extraction path is outside the project: $base" }
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        foreach ($entry in $zip.Entries) {
            $target = [IO.Path]::GetFullPath((Join-Path $Destination $entry.FullName))
            if (-not $target.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) { throw "Unsafe archive entry: $($entry.FullName)" }
        }
        foreach ($entry in $zip.Entries) {
            $target = Join-Path $Destination $entry.FullName
            if ([string]::IsNullOrEmpty($entry.Name)) {
                New-Item -ItemType Directory -Path $target -Force | Out-Null
            } else {
                New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
            }
        }
    } finally { $zip.Dispose() }
}

function Backup-KtGenerated([string]$Name) {
    $path = [IO.Path]::GetFullPath((Join-Path $ktRoot $Name))
    if (-not $path.StartsWith($ktRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Backup path must stay inside the project.' }
    if (Test-Path -LiteralPath $path) {
        $backup = $path + '.backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
        Write-Host "Preserving old generated files as $backup"
        Move-Item -LiteralPath $path -Destination $backup
    }
}

Start-Transcript -Path (Join-Path $ktRoot 'out\setup.log') -Force | Out-Null
try {
    Write-Host 'KT laptop setup: verifying the bundled tools.'
    $manifestPath = Join-Path $ktRoot 'support\manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Missing support/manifest.json. Copy the complete laptop kit.' }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    foreach ($file in $manifest.files) {
        $path = [IO.Path]::GetFullPath((Join-Path $ktRoot $file.path))
        if (-not $path.StartsWith($ktRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid manifest path.' }
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing bundled file: $($file.path)" }
        if ((Get-KtHash $path) -ne $file.sha256) { throw "File failed its checksum: $($file.path). Copy the kit again." }
    }
    $ktPythonRoot = Join-Path $ktRoot '.tools\python'
    $ktPython = Join-Path $ktPythonRoot 'tools\python.exe'
    if (-not (Test-Path -LiteralPath $ktPython)) {
        Write-Host 'Installing project-local Python 3.13.12.'
        Expand-KtArchive (Join-Path $ktRoot 'support\downloads\python.3.13.12.nupkg') $ktPythonRoot
    }
    Invoke-KtChecked $ktPython @('-c', 'import sys, struct; assert sys.version_info[:2] == (3,13) and struct.calcsize(chr(80)) == 8; print(sys.version)')
    $ktVenv = Join-Path $ktRoot '.venv\Scripts\python.exe'
    $validVenv = $false
    if (Test-Path -LiteralPath $ktVenv) {
        try {
            & $ktVenv -c 'import pathlib,sys; assert pathlib.Path(sys.prefix).resolve() == pathlib.Path(sys.argv[1]).resolve(); assert pathlib.Path(sys.base_prefix).resolve() == pathlib.Path(sys.argv[2]).resolve()' (Join-Path $ktRoot '.venv') (Split-Path -Parent $ktPython) 2>$null
            $validVenv = ($LASTEXITCODE -eq 0)
        } catch { $validVenv = $false }
    }
    if (-not $validVenv) {
        Backup-KtGenerated '.venv'
        Backup-KtGenerated 'build'
        Invoke-KtChecked $ktPython @('-m', 'venv', (Join-Path $ktRoot '.venv'))
    }
    $env:PATH = (Join-Path $ktRoot '.venv\Scripts') + ';' + $env:PATH
    Invoke-KtChecked $ktVenv @('-m', 'pip', 'install', '--no-index', '--no-cache-dir', '--disable-pip-version-check', '--find-links', (Join-Path $ktRoot 'support\wheels'), '-r', (Join-Path $ktRoot 'requirements-dev.lock'))
    foreach ($dep in @('SFML', 'freetype')) {
        $dest = Join-Path $ktRoot ".deps\$dep"
        if (-not (Test-Path -LiteralPath (Join-Path $dest 'CMakeLists.txt'))) {
            Write-Host "Extracting $dep source."
            Expand-KtArchive (Join-Path $ktRoot "support\downloads\$dep-source.zip") $dest
        }
    }
    if (-not $ToolsOnly) {
        Write-Host 'Building C++ and running tests. The first compiler run can be quiet for several minutes.'
        Invoke-KtChecked $ktVenv @('tools\kt.py', 'build')
        $verifyArgs = @('tools\verify_setup.py')
        if ($Quick) { $verifyArgs += '--quick' }
        Invoke-KtChecked $ktVenv $verifyArgs
        Write-Host 'Setup and verification passed. Run .\kt.cmd play to open KT.'
    } else { Write-Host 'Tools installed. Run .\setup.cmd to build and verify KT.' }
} catch {
    Write-Host "SETUP FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally { Stop-Transcript | Out-Null }
