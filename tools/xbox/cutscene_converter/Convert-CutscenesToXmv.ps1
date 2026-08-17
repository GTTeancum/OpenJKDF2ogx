param(
    [string]$GameRoot = "",
    [switch]$Force,
    [switch]$DeleteOriginals,
    [switch]$KeepOriginals,
    [switch]$DryRun,
    [int]$Width = 640,
    [int]$Height = 480,
    [int]$Fps = 15,
    [string]$VideoBitrate = "1200k"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($GameRoot)) {
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot "default.xbe")) {
        $GameRoot = $PSScriptRoot
    } elseif (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) "default.xbe")) {
        $GameRoot = Split-Path -Parent $PSScriptRoot
    } else {
        $GameRoot = (Get-Location).Path
    }
}

$root = (Resolve-Path -LiteralPath $GameRoot).Path
$defaultXbe = Join-Path $root "default.xbe"
if (-not (Test-Path -LiteralPath $defaultXbe)) {
    Write-Warning "default.xbe was not found in '$root'. Continuing, but this does not look like the extracted game root."
}

if (-not $DeleteOriginals -and -not $KeepOriginals -and -not $DryRun) {
    $answer = Read-Host "Delete original .SMK/.SAN videos after each .XMV validates? [y/N]"
    if ($answer -match '^(y|yes)$') {
        $DeleteOriginals = $true
    }
}

$script = Join-Path $PSScriptRoot "openjkdf2x_cutscene_packager.py"
if (-not (Test-Path -LiteralPath $script)) {
    throw "Missing converter script: $script"
}

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
}
if (-not $python) {
    throw "Python was not found. Install Python 3 or include it in the beta tools package."
}

$argsList = @(
    $script,
    "--game-root", $root,
    "--width", $Width,
    "--height", $Height,
    "--fps", $Fps,
    "--video-bitrate", $VideoBitrate
)

if ($Force) { $argsList += "--force" }
if ($DeleteOriginals) { $argsList += "--delete-originals" }
if ($DryRun) { $argsList += "--dry-run" }

& $python.Source @argsList
if ($LASTEXITCODE -ne 0) {
    throw "Cutscene conversion failed with exit code $LASTEXITCODE"
}
