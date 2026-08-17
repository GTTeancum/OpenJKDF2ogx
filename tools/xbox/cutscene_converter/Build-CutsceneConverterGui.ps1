param(
    [string]$Name = "OpenJKDF2xCutsceneConverter",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$entry = Join-Path $scriptDir "openjkdf2x_cutscene_gui.py"
if (-not (Test-Path -LiteralPath $entry)) {
    throw "Missing GUI entry point: $entry"
}

function Find-Tool([string]$ExeName) {
    $cmd = Get-Command $ExeName -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $wingetRoot = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path -LiteralPath $wingetRoot) {
        $match = Get-ChildItem -LiteralPath $wingetRoot -Recurse -Filter $ExeName -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($match) {
            return $match.FullName
        }
    }

    return $null
}

$pyinstallerArgs = @(
    "--onefile",
    "--windowed",
    "--name", $Name,
    "--distpath", (Join-Path $scriptDir "dist"),
    "--workpath", (Join-Path $scriptDir "build"),
    "--specpath", $scriptDir,
    "--paths", $scriptDir
)

if ($Clean) {
    $pyinstallerArgs += "--clean"
}

$ffmpeg = Find-Tool "ffmpeg.exe"
$ffprobe = Find-Tool "ffprobe.exe"
if ($ffmpeg -and $ffprobe) {
    $pyinstallerArgs += @("--add-binary", "$ffmpeg;.")
    $pyinstallerArgs += @("--add-binary", "$ffprobe;.")
    Write-Host "Bundling ffmpeg:  $ffmpeg"
    Write-Host "Bundling ffprobe: $ffprobe"
} else {
    Write-Warning "ffmpeg.exe and/or ffprobe.exe were not found. The EXE will require them beside it or on PATH."
}

$pyinstallerArgs += $entry

python -m PyInstaller @pyinstallerArgs
if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller failed with exit code $LASTEXITCODE"
}

$exe = Join-Path (Join-Path $scriptDir "dist") "$Name.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Expected output was not created: $exe"
}

$distDir = Split-Path -Parent $exe
$notice = Join-Path $scriptDir "THIRD_PARTY_NOTICES.txt"
if (Test-Path -LiteralPath $notice) {
    Copy-Item -LiteralPath $notice -Destination (Join-Path $distDir "THIRD_PARTY_NOTICES.txt") -Force
}

$ffmpegNoticeDir = Join-Path $scriptDir "third_party\ffmpeg"
foreach ($noticeName in @("FFMPEG_LICENSE.txt", "FFMPEG_GYAN_README.txt")) {
    $noticePath = Join-Path $ffmpegNoticeDir $noticeName
    if (Test-Path -LiteralPath $noticePath) {
        Copy-Item -LiteralPath $noticePath -Destination (Join-Path $distDir $noticeName) -Force
    }
}

Write-Host "Built: $exe"
