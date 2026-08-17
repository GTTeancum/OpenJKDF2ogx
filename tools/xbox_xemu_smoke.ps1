param(
    [int]$DurationSeconds = 300,
    [int]$PollIntervalSeconds = 15,
    [string]$RunLabel = "xemu-smoke",
    [int]$MonitorPort = 4477,
    [int]$FmvLimitSeconds = 0,
    [string]$AutoStartLevel = "",
    [string]$AutoStartArgs = "",
    [switch]$DisableMusic,
    [switch]$DisableCutscenes,
    [string]$AlwaysOnSoakPlanPath = "",
    [int]$OpenEscapeAfterSeconds = 0,
    [int]$ScreenshotEverySeconds = 0,
    [switch]$InputProbe,
    [switch]$KeepIso,
    [string]$RuntimeSource = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$XemuRoot = "C:\Games\Emulators\Xemu",
    [string]$HddPath = "C:\Games\Emulators\Xemu\HDD\xbox_hdd.qcow2",
    [string]$WorkRoot = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildRoot = Join-Path $RepoRoot "build\xbox"
$ReleaseRoot = Join-Path $BuildRoot "release"
$ArtifactRoot = $BuildRoot
if (![string]::IsNullOrWhiteSpace($WorkRoot)) {
    $ArtifactRoot = [System.IO.Path]::GetFullPath($WorkRoot)
}
$StagePath = Join-Path $ArtifactRoot "xemu_smoke_stage_current"
$IsoPath = Join-Path $ArtifactRoot "openjkdf2_xemu_smoke_current.iso"
$NewIsoPath = Join-Path $ArtifactRoot "openjkdf2_xemu_smoke_current.new.iso"
$OutRoot = Join-Path $ArtifactRoot "xemu_smoke_runs"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$SafeLabel = $RunLabel -replace '[^A-Za-z0-9_.-]', '_'
$RunDir = Join-Path $OutRoot "$Timestamp-$SafeLabel"
$LatestPollDir = Join-Path $RunDir "latest_poll"
$SummaryPath = Join-Path $RunDir "summary.txt"
$InstanceDir = Join-Path $XemuRoot "OpenJKDF2Smoke"
$ConfigPath = Join-Path $InstanceDir "xemu.toml"
$EepromPath = Join-Path $InstanceDir "EEPROM\eeprom_smoke.bin"
$ScreenshotDir = Join-Path $RunDir "screenshots"
$PollScript = Join-Path $RepoRoot "scripts\xbox\poll_xemu_ram_log.py"
$NativeScreenshotScript = Join-Path $RepoRoot "scripts\xbox\xemu_native_screenshot.py"
$XbePath = Join-Path $ReleaseRoot "default.xbe"
$MapPath = Join-Path $ReleaseRoot "openjkdf2_xbox.exe.map"
$XemuExeSource = Join-Path $XemuRoot "xemu.exe"
$XemuExe = Join-Path $InstanceDir "xemu.exe"
$BootRom = Join-Path $XemuRoot "MCPX\mcpx_1.0.bin"
$FlashRom = Join-Path $XemuRoot "BIOS\xbox-4627_debug.bin"
$EepromSource = Join-Path $XemuRoot "EEPROM\eeprom.bin"

$XisoToolCandidates = @(
    "C:\nxdk\tools\extract-xiso\build\extract-xiso.exe",
    (Join-Path $BuildRoot "tools\extract-xiso\artifacts\extract-xiso.exe"),
    "C:\Programming\GitHub\Guitar Hero II\tools\artifacts\extract-xiso.exe",
    (Join-Path $RepoRoot "..\Guitar Hero II\tools\artifacts\extract-xiso.exe")
)

function Require-Path([string]$Path, [string]$Label) {
    if (!(Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Get-FullPath([string]$Path) {
    return [System.IO.Path]::GetFullPath($Path)
}

function Assert-UnderPath([string]$Parent, [string]$Child) {
    $parentFull = (Get-FullPath $Parent).TrimEnd('\')
    $childFull = Get-FullPath $Child
    if (!$childFull.StartsWith($parentFull + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing path outside $parentFull`: $childFull"
    }
}

function Remove-SafeGeneratedPath([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) {
        return
    }
    Assert-UnderPath $ArtifactRoot $Path
    Remove-Item -LiteralPath $Path -Recurse -Force
}

function Find-XisoTool {
    foreach ($candidate in $XisoToolCandidates) {
        $full = Get-FullPath $candidate
        if (Test-Path -LiteralPath $full) {
            return $full
        }
    }
    throw "extract-xiso.exe not found. Checked: $($XisoToolCandidates -join ', ')"
}

function Get-XemuProcessForConfig([string]$Path) {
    $needle = Get-FullPath $Path
    Get-CimInstance Win32_Process -Filter "Name = 'xemu.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($needle, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 }
}

function Stop-XemuForConfig([string]$Path) {
    $procs = @(Get-XemuProcessForConfig $Path)
    foreach ($proc in $procs) {
        Stop-Process -Id $proc.ProcessId -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-HmpCommand([int]$Port, [string]$Command, [int]$TimeoutMs = 2500) {
    $client = New-Object System.Net.Sockets.TcpClient
    try {
        $async = $client.BeginConnect("127.0.0.1", $Port, $null, $null)
        if (!$async.AsyncWaitHandle.WaitOne($TimeoutMs)) {
            throw "monitor connect timed out"
        }
        $client.EndConnect($async)
        $stream = $client.GetStream()
        $stream.ReadTimeout = 250
        Start-Sleep -Milliseconds 100
        $buffer = New-Object byte[] 4096
        while ($stream.DataAvailable) {
            [void]$stream.Read($buffer, 0, $buffer.Length)
        }
        $writer = New-Object System.IO.StreamWriter($stream, [System.Text.Encoding]::ASCII)
        $writer.NewLine = "`n"
        $writer.AutoFlush = $true
        $writer.WriteLine($Command)
        Start-Sleep -Milliseconds 700
        $text = ""
        while ($stream.DataAvailable) {
            $read = $stream.Read($buffer, 0, $buffer.Length)
            if ($read -le 0) {
                break
            }
            $text += [System.Text.Encoding]::ASCII.GetString($buffer, 0, $read)
        }
        return $text
    }
    finally {
        $client.Close()
    }
}

function Wait-ForNonEmptyFile([string]$Path, [int]$TimeoutMs = 3000) {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path -LiteralPath $Path) -and ((Get-Item -LiteralPath $Path).Length -gt 0)) {
            return $true
        }
        Start-Sleep -Milliseconds 100
    }
    return $false
}

function Invoke-XemuScreendump([int]$Port, [string]$OutPath, [string]$WorkingDir) {
    $outFull = Get-FullPath $OutPath
    $outSlash = $outFull -replace '\\', '/'
    $localName = [System.IO.Path]::GetFileName($outFull)
    $localPath = Join-Path $WorkingDir $localName
    $attempts = @(
        @{ Command = "screendump `"$outSlash`""; Source = $outFull },
        @{ Command = "screendump $outSlash"; Source = $outFull },
        @{ Command = "screendump `"$localName`""; Source = $localPath },
        @{ Command = "screendump $localName"; Source = $localPath }
    )
    $replies = New-Object System.Collections.Generic.List[string]

    foreach ($attempt in $attempts) {
        $source = [string]$attempt.Source
        Remove-Item -LiteralPath $outFull -Force -ErrorAction SilentlyContinue
        if (![string]::Equals($source, $outFull, [System.StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $source -Force -ErrorAction SilentlyContinue
        }

        $reply = Invoke-HmpCommand $Port ([string]$attempt.Command) 2500
        $replyOneLine = (($reply -replace "`r", "") -replace "`n", "\n")
        if ($replyOneLine.Length -gt 180) {
            $replyOneLine = $replyOneLine.Substring(0, 180)
        }
        $replies.Add("$($attempt.Command) => $replyOneLine")

        if (Wait-ForNonEmptyFile $source 3000) {
            if (![string]::Equals($source, $outFull, [System.StringComparison]::OrdinalIgnoreCase)) {
                Copy-Item -LiteralPath $source -Destination $outFull -Force
                Remove-Item -LiteralPath $source -Force -ErrorAction SilentlyContinue
            }
            if (Wait-ForNonEmptyFile $outFull 1000) {
                return "captured $outFull via $($attempt.Command)"
            }
        }
    }

    throw "screendump did not create an image: $($replies -join ' | ')"
}

function Invoke-XemuNativeScreenshot([int]$ProcessId, [string]$XemuExePath, [string]$OutDir, [string]$StablePath) {
    Require-Path $NativeScreenshotScript "XEMU native screenshot helper"
    $nativeOut = & python $NativeScreenshotScript --pid ([string]$ProcessId) --xemu-exe $XemuExePath --screenshot-dir $OutDir --timeout 5 2>&1
    $nativeText = ($nativeOut | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw "native screenshot failed: $nativeText"
    }

    $nativeJson = $nativeText | ConvertFrom-Json
    if (!$nativeJson.ok -or !$nativeJson.path -or !(Test-Path -LiteralPath $nativeJson.path)) {
        throw "native screenshot returned no image: $nativeText"
    }

    Copy-Item -LiteralPath $nativeJson.path -Destination $StablePath -Force
    return "captured $StablePath via native XEMU screenshot ($($nativeJson.detail))"
}

function New-Stage {
    Require-Path $RuntimeSource "OpenJKDF2 runtime source"
    Require-Path $XbePath "Release default.xbe"

    Remove-SafeGeneratedPath $StagePath
    Remove-SafeGeneratedPath $NewIsoPath
    if (!$KeepIso) {
        Remove-SafeGeneratedPath $IsoPath
    }

    New-Item -ItemType Directory -Force -Path $StagePath | Out-Null
    robocopy $RuntimeSource $StagePath /E /NFL /NDL /NJH /NJS /NP /XD Logs Screenshots /XF debug_openjkdf2.txt CxbxDebug.txt KrnlDebug.txt | Out-Null
    if ($LASTEXITCODE -gt 7) {
        throw "robocopy runtime staging failed with exit code $LASTEXITCODE"
    }

    Copy-Item -LiteralPath $XbePath -Destination (Join-Path $StagePath "default.xbe") -Force

    if ($FmvLimitSeconds -gt 0) {
        Set-Content -LiteralPath (Join-Path $StagePath "xbox_smoke_fmv_seconds.txt") -Value ([string]$FmvLimitSeconds) -Encoding ASCII
    }
    if (![string]::IsNullOrWhiteSpace($AutoStartLevel)) {
        Set-Content -LiteralPath (Join-Path $StagePath "xbox_smoke_autostart_level.txt") -Value $AutoStartLevel -Encoding ASCII
    }
    if (![string]::IsNullOrWhiteSpace($AutoStartArgs)) {
        Set-Content -LiteralPath (Join-Path $StagePath "xbox_smoke_autostart_args.txt") -Value $AutoStartArgs -Encoding ASCII
    }
    if (![string]::IsNullOrWhiteSpace($AlwaysOnSoakPlanPath)) {
        Require-Path $AlwaysOnSoakPlanPath "Always-on soak plan"
        Copy-Item -LiteralPath $AlwaysOnSoakPlanPath -Destination (Join-Path $StagePath "xbox_soak_always_on.txt") -Force
    }
    if ($OpenEscapeAfterSeconds -gt 0) {
        Set-Content -LiteralPath (Join-Path $StagePath "xbox_smoke_escape_after_seconds.txt") -Value ([string]$OpenEscapeAfterSeconds) -Encoding ASCII
    }
    if ($InputProbe) {
        Set-Content -LiteralPath (Join-Path $StagePath "xbox_smoke_input_probe.txt") -Value "1" -Encoding ASCII
    }
    if ($DisableMusic) {
        Set-Content -LiteralPath (Join-Path $StagePath "xbox_smoke_disable_music.txt") -Value "1" -Encoding ASCII
    }
    if ($DisableCutscenes) {
        Set-Content -LiteralPath (Join-Path $StagePath "xbox_smoke_disable_cutscenes.txt") -Value "1" -Encoding ASCII
    }
}

function New-Xiso {
    $xisoTool = Find-XisoTool
    & $xisoTool -Q -m -c $StagePath $NewIsoPath
    if ($LASTEXITCODE -ne 0) {
        throw "extract-xiso failed with exit code $LASTEXITCODE"
    }
    Move-Item -LiteralPath $NewIsoPath -Destination $IsoPath -Force
}

function Initialize-XemuInstance {
    Require-Path $XemuExeSource "xemu.exe"
    Require-Path $BootRom "MCPX boot ROM"
    Require-Path $FlashRom "Xbox BIOS"
    Require-Path $EepromSource "EEPROM source"
    Require-Path $HddPath "XEMU HDD image"

    New-Item -ItemType Directory -Force -Path $InstanceDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $InstanceDir "EEPROM") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $InstanceDir "shaders") | Out-Null
    New-Item -ItemType Directory -Force -Path $ScreenshotDir | Out-Null

    if (!(Test-Path -LiteralPath $XemuExe)) {
        try {
            New-Item -ItemType HardLink -Path $XemuExe -Target $XemuExeSource | Out-Null
        }
        catch {
            Copy-Item -LiteralPath $XemuExeSource -Destination $XemuExe -Force
        }
    }
    Copy-Item -LiteralPath $EepromSource -Destination $EepromPath -Force

    $config = @"
[general]
show_welcome = false
screenshot_dir = '$ScreenshotDir'
games_dir = '$InstanceDir'
skip_boot_anim = true
last_viewed_menu_index = 1

[general.updates]
check = false

[input]
auto_bind = false
background_input_capture = true

[input.keyboard_controller_scancode_map]
a = 4
b = 5
start = 22
dpad_up = 26
dpad_down = 7
dpad_left = 20
dpad_right = 8

[input.bindings]
port1_driver = 'usb-xbox-gamepad'
port1 = 'keyboard'
port2_driver = 'usb-xbox-gamepad'
port3_driver = 'usb-xbox-gamepad'
port4_driver = 'usb-xbox-gamepad'

[net]
enable = false

[sys.files]
bootrom_path = '$BootRom'
flashrom_path = '$FlashRom'
eeprom_path = '$EepromPath'
hdd_path = '$HddPath'
dvd_path = '$IsoPath'
"@
    Set-Content -LiteralPath $ConfigPath -Value $config -Encoding ASCII
}

function Count-Matches([string[]]$Paths, [string[]]$Patterns) {
    $count = 0
    foreach ($path in $Paths) {
        if (Test-Path -LiteralPath $path) {
            $count += @(Select-String -Path $path -Pattern $Patterns -ErrorAction SilentlyContinue).Count
        }
    }
    return $count
}

function Test-AnyPattern([string[]]$Paths, [string]$Pattern) {
    foreach ($path in $Paths) {
        if ((Test-Path -LiteralPath $path) -and (Select-String -Path $path -Pattern $Pattern -Quiet -ErrorAction SilentlyContinue)) {
            return $true
        }
    }
    return $false
}

function Get-ReachedStates([string[]]$Paths) {
    $states = New-Object System.Collections.Generic.List[string]
    if (Test-AnyPattern $Paths "main: entering game loop") { $states.Add("game-loop") }
    if (Test-AnyPattern $Paths "MotSMode: switching resources .*MotS") { $states.Add("mots-switch") }
    if (Test-AnyPattern $Paths "MotSMode: reload item descriptors done") { $states.Add("mots-items") }
    if (Test-AnyPattern $Paths "CutsceneTrace:|XmvDbg|XMV") { $states.Add("fmv") }
    if (Test-AnyPattern $Paths "XMV finished, releasing movie state only") { $states.Add("xmv-finished") }
    if (Test-AnyPattern $Paths "MenuFlickerDbg: Paint") { $states.Add("menu-paint") }
    if (Test-AnyPattern $Paths "Smoke: AlwaysSoak start") { $states.Add("always-soak-start") }
    if (Test-AnyPattern $Paths "Smoke: AlwaysSoak menu-touch") { $states.Add("always-soak-menu") }
    if (Test-AnyPattern $Paths "Smoke: AlwaysSoak load") { $states.Add("always-soak-load") }
    if (Test-AnyPattern $Paths "Smoke: AlwaysSoak gameplay-ready") { $states.Add("always-soak-gameplay") }
    if (Test-AnyPattern $Paths "Smoke: AlwaysSoak phase-complete") { $states.Add("always-soak-transition") }
    if (Test-AnyPattern $Paths "SmokeInputProbe:|SplitScreenInputProbe:") { $states.Add("input-probe") }
    if (Test-AnyPattern $Paths "jkGuiMain_Show: smoke autostart") { $states.Add("autostart") }
    if (Test-AnyPattern $Paths "GameplayShow:.*loading level|sithWorld_Load: opened OK 'jkl\\[^']+\.jkl'|MPLoadTrace: sithMain_Open begin|MPLoadTrace: GameplayShow after LoadingFinalize") { $states.Add("level-load") }
    if (Test-AnyPattern $Paths "sithWorld_Load: section end things") { $states.Add("level-things") }
    if (Test-AnyPattern $Paths "GameplayShow: done|MPLoadTrace: GameplayShow done") { $states.Add("gameplay-show-done") }
    if (Test-AnyPattern $Paths "GameplayTick: enter|sithTick: enter|XboxFrame: begin n=|XboxFrame: cam n=|TickAll: exit|PerfHW:") { $states.Add("gameplay-frames") }
    if ($states.Count -eq 0) {
        return "none"
    }
    return ($states -join ",")
}

New-Item -ItemType Directory -Force -Path $ArtifactRoot | Out-Null
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
New-Item -ItemType Directory -Force -Path $LatestPollDir | Out-Null

$start = Get-Date
$proc = $null
$pollIndex = 0
$pollOkCount = 0
$lastPollPath = ""
$lastPollText = ""
$lastPollError = ""
$aliveAtEnd = $false

try {
    New-Stage
    New-Xiso
    Remove-SafeGeneratedPath $StagePath
    Initialize-XemuInstance

    Stop-XemuForConfig $ConfigPath
    Start-Sleep -Milliseconds 500

    $args = @("-config_path", $ConfigPath, "-monitor", "tcp:127.0.0.1:$MonitorPort,server,nowait")
    $proc = Start-Process -FilePath $XemuExe -ArgumentList $args -WorkingDirectory $InstanceDir -WindowStyle Hidden -PassThru

    $deadline = $start.AddSeconds($DurationSeconds)
    $nextScreenshot = if ($ScreenshotEverySeconds -gt 0) { $start.AddSeconds($ScreenshotEverySeconds) } else { [DateTime]::MaxValue }

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds $PollIntervalSeconds
        if ($proc.HasExited) {
            break
        }

        $pollIndex++
        $elapsed = [int]((Get-Date) - $start).TotalSeconds
        $pollOut = Join-Path $RunDir ("poll_{0:D3}.txt" -f $pollIndex)
        $pollErr = Join-Path $RunDir ("poll_{0:D3}.err.txt" -f $pollIndex)
        $latestPath = Join-Path $LatestPollDir ("port{0}_openjkdf2_ram_log.txt" -f $MonitorPort)

        & python $PollScript --ports ([string]$MonitorPort) --map $MapPath --xbe $XbePath --out-dir $LatestPollDir --timeout 3 *> $pollOut
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $latestPath)) {
            $pollOkCount++
            $snapshot = Join-Path $RunDir ("ram_poll_{0:D3}_{1:D4}s.txt" -f $pollIndex, $elapsed)
            Copy-Item -LiteralPath $latestPath -Destination $snapshot -Force
            $lastPollPath = $snapshot
            $lastPollText = Get-Content -LiteralPath $snapshot -Raw -ErrorAction SilentlyContinue
        }
        else {
            if (Test-Path -LiteralPath $pollOut) {
                $lastPollError = Get-Content -LiteralPath $pollOut -Raw -ErrorAction SilentlyContinue
            }
            Set-Content -LiteralPath $pollErr -Value $lastPollError -Encoding UTF8
        }

        if ((Get-Date) -ge $nextScreenshot) {
            $shot = Join-Path $ScreenshotDir ("shot_{0:D4}s.png" -f $elapsed)
            try {
                $shotDetail = Invoke-XemuNativeScreenshot $proc.Id $XemuExe $ScreenshotDir $shot
                Set-Content -LiteralPath (Join-Path $RunDir ("screenshot_{0:D4}s.txt" -f $elapsed)) -Value $shotDetail -Encoding ASCII
            }
            catch {
                $fallbackShot = Join-Path $ScreenshotDir ("shot_{0:D4}s.ppm" -f $elapsed)
                try {
                    $shotDetail = Invoke-XemuScreendump $MonitorPort $fallbackShot $InstanceDir
                    Set-Content -LiteralPath (Join-Path $RunDir ("screenshot_{0:D4}s.txt" -f $elapsed)) -Value $shotDetail -Encoding ASCII
                }
                catch {
                    Set-Content -LiteralPath (Join-Path $RunDir ("screenshot_{0:D4}s.err.txt" -f $elapsed)) -Value $_.Exception.Message -Encoding ASCII
                }
            }
            $nextScreenshot = (Get-Date).AddSeconds($ScreenshotEverySeconds)
        }
    }

    $aliveAtEnd = !$proc.HasExited
}
finally {
    if ($proc -and !$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
    }
    Stop-XemuForConfig $ConfigPath

    if (Test-Path -LiteralPath (Join-Path $InstanceDir "xemu.log")) {
        Copy-Item -LiteralPath (Join-Path $InstanceDir "xemu.log") -Destination (Join-Path $RunDir "xemu.log") -Force -ErrorAction SilentlyContinue
    }
    if (!$KeepIso) {
        Remove-SafeGeneratedPath $IsoPath
        Remove-SafeGeneratedPath $NewIsoPath
    }
    Remove-SafeGeneratedPath $StagePath
}

$end = Get-Date
$duration = [int]($end - $start).TotalSeconds
$ramLogs = @(Get-ChildItem -LiteralPath $RunDir -Filter "ram_poll_*.txt" -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
$fatalPatterns = @(
    "Received Exception",
    "FATAL",
    "Out of memory",
    "E_OUTOFMEMORY",
    "EIP",
    "Unhandled",
    "SECTION PARSE FAILED",
    "Memory alloc failure",
    "D3D Error",
    "failed 0x"
)
$fatalCount = Count-Matches $ramLogs $fatalPatterns
$reached = Get-ReachedStates $ramLogs
$screenshotCount = @(
    Get-ChildItem -LiteralPath $ScreenshotDir -Include "*.png", "*.ppm" -File -ErrorAction SilentlyContinue
).Count

$summary = @(
    "runLabel=$RunLabel",
    "runDir=$RunDir",
    "workRoot=$ArtifactRoot",
    "start=$($start.ToString('s'))",
    "durationSeconds=$duration",
    "targetDurationSeconds=$DurationSeconds",
    "pollIntervalSeconds=$PollIntervalSeconds",
    "monitorPort=$MonitorPort",
    "fmvLimitSeconds=$FmvLimitSeconds",
    "autoStartLevel=$AutoStartLevel",
    "autoStartArgs=$AutoStartArgs",
    "disableMusic=$([bool]$DisableMusic)",
    "disableCutscenes=$([bool]$DisableCutscenes)",
    "alwaysOnSoakPlanPath=$AlwaysOnSoakPlanPath",
    "openEscapeAfterSeconds=$OpenEscapeAfterSeconds",
    "inputProbe=$([bool]$InputProbe)",
    "runtimeSource=$RuntimeSource",
    "xbe=$XbePath",
    "map=$MapPath",
    "xemuExe=$XemuExe",
    "hddPath=$HddPath",
    "isoCleaned=$(!$KeepIso)",
    "aliveAtEnd=$aliveAtEnd",
    "pollCount=$pollIndex",
    "pollOkCount=$pollOkCount",
    "fatalCount=$fatalCount",
    "reached=$reached",
    "screenshotCount=$screenshotCount",
    "lastPollPath=$lastPollPath",
    ""
)
if ($lastPollText) {
    $summary += "lastPollTail:"
    $summary += (($lastPollText -split "`r?`n") | Select-Object -Last 80)
}
elseif ($lastPollError) {
    $summary += "lastPollError:"
    $summary += $lastPollError
}

Set-Content -LiteralPath $SummaryPath -Value $summary -Encoding UTF8
Get-Content -LiteralPath $SummaryPath

if ($pollOkCount -eq 0 -or $fatalCount -gt 0) {
    exit 1
}
