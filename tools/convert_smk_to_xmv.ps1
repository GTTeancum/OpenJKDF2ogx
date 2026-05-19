param(
    [string]$VideoDir = "C:\Games\Emulators\CXBX\openJKDF2x\Resource\VIDEO",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$ffmpeg = Get-ChildItem -Path "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter ffmpeg.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $ffmpeg) {
    throw "ffmpeg.exe not found. Install Gyan.FFmpeg with winget or put ffmpeg on PATH."
}

$xmvtool = "C:\XDK_5558\XDK\xbox\bin\xmvtool.exe"
if (-not (Test-Path -LiteralPath $xmvtool)) {
    throw "xmvtool.exe not found at $xmvtool"
}

if (-not (Test-Path -LiteralPath $VideoDir)) {
    throw "Video directory not found: $VideoDir"
}

$smks = Get-ChildItem -LiteralPath $VideoDir -Filter *.SMK -File | Sort-Object Name
if (-not $smks) {
    throw "No .SMK files found in $VideoDir"
}

$workDir = Join-Path $env:TEMP "openjkdf2_smk_to_xmv"
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

foreach ($smk in $smks) {
    $base = [IO.Path]::GetFileNameWithoutExtension($smk.Name)
    $out = Join-Path $VideoDir "$base.XMV"
    $wmv = Join-Path $workDir "$base.video.wmv"
    $wav = Join-Path $workDir "$base.audio.wav"

    if ((Test-Path -LiteralPath $out) -and -not $Force) {
        Write-Host "Skipping $($smk.Name): $base.XMV already exists"
        continue
    }

    Write-Host "Converting $($smk.Name) -> $base.XMV"
    Remove-Item -LiteralPath $wmv, $wav, $out -ErrorAction SilentlyContinue

    & $ffmpeg -y -hide_banner -i $smk.FullName `
        -map 0:v:0 `
        -vf "scale=640:300,pad=640:480:0:90:black,format=yuv420p" `
        -c:v wmv2 -b:v 1200k -r 15 -g 15 -an `
        $wmv
    if ($LASTEXITCODE -ne 0) {
        throw "ffmpeg video encode failed for $($smk.Name) with exit code $LASTEXITCODE"
    }

    & $ffmpeg -y -hide_banner -i $smk.FullName `
        -map 0:a:0 `
        -c:a pcm_s16le -ar 22050 -ac 2 `
        $wav
    if ($LASTEXITCODE -ne 0) {
        throw "ffmpeg audio extract failed for $($smk.Name) with exit code $LASTEXITCODE"
    }

    & $xmvtool -v $wmv -ac $wav -o $out
    if ($LASTEXITCODE -ne 0) {
        throw "xmvtool failed for $($smk.Name) with exit code $LASTEXITCODE"
    }

    Remove-Item -LiteralPath $wmv, $wav -ErrorAction SilentlyContinue
    $item = Get-Item -LiteralPath $out
    Write-Host ("  Wrote {0:n1} MB" -f ($item.Length / 1MB))
}
