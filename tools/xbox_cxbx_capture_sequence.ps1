param(
    [string]$Session = "openjkdf2-q3dm5-review",
    [string]$BuildDir = "",
    [string]$AppDir = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$CaptureRoot = "C:\Games\Emulators\CXBX-CodexCapture",
    [string]$OutDir = "",
    [int]$FrameCount = 60,
    [int]$FrameIntervalMs = 250,
    [int]$BotCamera = 1,
    [string]$AutoStartArgs = "",
    [int]$StartupTimeoutSeconds = 180,
    [int]$RealtimeSeconds = 0,
    [int]$RealtimeFps = 30,
    [string]$WindowTitle = "Cxbx-Reloaded"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if (!$BuildDir) { $BuildDir = Join-Path $repoRoot "build\xbox\release" }
if (!$OutDir) { $OutDir = Join-Path $repoRoot "build\xbox\recordings" }

$xbeSource = Join-Path $BuildDir "default.xbe"
$xbeTarget = Join-Path $AppDir "default.xbe"
$gameLog = Join-Path $AppDir "debug_openjkdf2.txt"
$autoStartArgsPath = Join-Path $AppDir "xbox_smoke_autostart_args.txt"
$disableMusicPath = Join-Path $AppDir "xbox_smoke_disable_music.txt"
$muteAudioPath = Join-Path $AppDir "xbox_smoke_mute_audio.txt"
$startScript = Join-Path $CaptureRoot "Start-CodexCaptureSession.ps1"
$safeSession = $Session -replace '[^\w.-]+', '_'
$sequenceDir = Join-Path $OutDir $safeSession
$framesDir = Join-Path $sequenceDir "frames"
$videoPath = Join-Path $sequenceDir "$safeSession.mp4"

function Stop-CaptureProcesses {
    Get-CimInstance Win32_Process |
        Where-Object {
            $_.ExecutablePath -and
            $_.ExecutablePath.StartsWith($CaptureRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
            $_.Name -match '^cxbx'
        } |
        ForEach-Object {
            Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
        }
}

if (!(Test-Path -LiteralPath $xbeSource)) { throw "Missing XBE: $xbeSource" }
if (!(Test-Path -LiteralPath $startScript)) { throw "Missing capture launcher: $startScript" }

New-Item -ItemType Directory -Force -Path $sequenceDir, $framesDir | Out-Null
Stop-CaptureProcesses
Copy-Item -LiteralPath $xbeSource -Destination $xbeTarget -Force
if (!$AutoStartArgs) {
    $AutoStartArgs = "-autostart -mp -episode q3dm5 -map q3dm5.jkl -bots 6 -botmatch-seconds 120 -botcam $BotCamera"
}
Set-Content -LiteralPath $autoStartArgsPath `
    -Value $AutoStartArgs `
    -Encoding ASCII -NoNewline
Set-Content -LiteralPath $disableMusicPath -Value "1" -Encoding ASCII
Set-Content -LiteralPath $muteAudioPath -Value "1" -Encoding ASCII
Remove-Item -LiteralPath $gameLog -Force -ErrorAction SilentlyContinue

try {
    $sessionInfo = & $startScript -Xbe $xbeTarget -Session $safeSession
    $deadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $gameLog) {
            if (Select-String -Path $gameLog -Pattern "BotMatch: start" -Quiet) {
                break
            }
        }
        Start-Sleep -Milliseconds 500
    }
    if (!(Test-Path -LiteralPath $gameLog) -or
        !(Select-String -Path $gameLog -Pattern "BotMatch: start" -Quiet)) {
        throw "Timed out waiting for BotMatch start."
    }

    Add-Type -AssemblyName System.Drawing
    $captureOutput = [string]$sessionInfo.capture_output
    $captureTrigger = [string]$sessionInfo.capture_trigger
    $framesToCapture = if ($RealtimeSeconds -gt 0) { 1 } else { $FrameCount }
    for ($i = 0; $i -lt $framesToCapture; $i++) {
        $framePath = Join-Path $framesDir ("frame-{0:D3}.png" -f $i)
        Remove-Item -LiteralPath $captureOutput -Force -ErrorAction SilentlyContinue
        New-Item -ItemType File -Force -Path $captureTrigger | Out-Null

        $captureDeadline = (Get-Date).AddSeconds(30)
        $triggerRetryAt = (Get-Date).AddSeconds(2)
        $frameReady = $false
        while ((Get-Date) -lt $captureDeadline) {
            Start-Sleep -Milliseconds 20
            if (!(Test-Path -LiteralPath $captureOutput)) {
                if ((Get-Date) -ge $triggerRetryAt) {
                    New-Item -ItemType File -Force -Path $captureTrigger | Out-Null
                    $triggerRetryAt = (Get-Date).AddSeconds(2)
                }
                continue
            }
            try {
                $image = [System.Drawing.Image]::FromFile($captureOutput)
                $image.Dispose()
                Copy-Item -LiteralPath $captureOutput -Destination $framePath -Force
                $frameReady = $true
                break
            }
            catch {
                # The capture DLL publishes the path before the PNG is always complete.
            }
        }
        if (!$frameReady) {
            throw "Timed out waiting for complete frame $i."
        }
        if ($RealtimeSeconds -le 0 -and $FrameIntervalMs -gt 0) {
            Start-Sleep -Milliseconds $FrameIntervalMs
        }
    }

    if ($RealtimeSeconds -gt 0) {
        $windowDeadline = (Get-Date).AddSeconds(20)
        while ((Get-Date) -lt $windowDeadline) {
            if (Get-Process | Where-Object { $_.MainWindowTitle -eq $WindowTitle }) {
                break
            }
            Start-Sleep -Milliseconds 250
        }
        if (!(Get-Process | Where-Object { $_.MainWindowTitle -eq $WindowTitle })) {
            throw "Timed out waiting for capture window '$WindowTitle'."
        }

        ffmpeg -hide_banner -loglevel error -y `
            -f gdigrab -framerate $RealtimeFps -i "title=$WindowTitle" `
            -t $RealtimeSeconds `
            -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" `
            -c:v libx264 -preset veryfast -pix_fmt yuv420p -crf 20 `
            $videoPath
    }
    else {
        ffmpeg -hide_banner -loglevel error -y `
            -framerate ([string]::Format(
                [System.Globalization.CultureInfo]::InvariantCulture,
                "{0:0.###}",
                $(if ($FrameIntervalMs -gt 0) { 1000.0 / $FrameIntervalMs } else { 4.0 }))) `
            -i (Join-Path $framesDir "frame-%03d.png") `
            -c:v libx264 -pix_fmt yuv420p -crf 20 `
            $videoPath
    }
    if ($LASTEXITCODE -ne 0) {
        throw "ffmpeg recording failed with exit code $LASTEXITCODE."
    }

    if (Test-Path -LiteralPath $gameLog) {
        Copy-Item -LiteralPath $gameLog -Destination (Join-Path $sequenceDir "debug_openjkdf2.txt") -Force
    }

    [pscustomobject]@{
        Session = $safeSession
        Frames = $framesToCapture
        FramesDir = $framesDir
        RecordingMode = if ($RealtimeSeconds -gt 0) { "realtime" } else { "frame-sequence" }
        RealtimeFps = if ($RealtimeSeconds -gt 0) { $RealtimeFps } else { 0 }
        Video = $videoPath
        VideoCreated = Test-Path -LiteralPath $videoPath
        ProcessId = $sessionInfo.process_id
    }
}
finally {
    Stop-CaptureProcesses
    Remove-Item -LiteralPath $autoStartArgsPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $disableMusicPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $muteAudioPath -Force -ErrorAction SilentlyContinue
}
