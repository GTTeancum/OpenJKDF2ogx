param(
    [switch]$PrepareOnly,
    [switch]$Run,
    [switch]$SystemLinkOnly,
    [switch]$CleanArtifacts,
    [switch]$SkipSystemLinkPrep,
    [int]$MenuSeconds = 900,
    [int]$MapSeconds = 1200,
    [int]$SplitSeconds = 1200,
    [int]$PollIntervalSeconds = 30,
    [int]$ScreenshotEverySeconds = 120,
    [int]$FmvLimitSeconds = 3,
    [switch]$DisableCutscenes,
    [int]$MonitorPort = 4477,
    [string[]]$SinglePlayerMaps = @("01narshadda.jkl", "06abarons.jkl", "15maw.jkl"),
    [string]$RuntimeSource = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$XemuRoot = "C:\Games\Emulators\Xemu",
    [string]$HddPath = "C:\Games\Emulators\Xemu\HDD\xbox_hdd.qcow2",
    [string]$HostHdd = "C:\Games\Emulators\Xemu\UT99Test\HDD\ut99_hdd.qcow2",
    [string]$ClientHdd = "C:\Games\Emulators\Xemu\UT99Fresh\HDD\ut99_hdd.qcow2"
)

$ErrorActionPreference = "Stop"

if ((@($Run.IsPresent, $PrepareOnly.IsPresent, $SystemLinkOnly.IsPresent) | Where-Object { $_ }).Count -gt 1) {
    throw "Choose only one of -Run, -PrepareOnly, or -SystemLinkOnly."
}

$DoRun = $Run.IsPresent
$DoPrepare = $PrepareOnly.IsPresent -or (!$DoRun -and !$SystemLinkOnly.IsPresent)

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildRoot = Join-Path $RepoRoot "build\xbox"
$ReleaseRoot = Join-Path $BuildRoot "release"
$StagePath = Join-Path $BuildRoot "xemu_soak_stage_current"
$IsoPath = Join-Path $BuildRoot "openjkdf2_xemu_soak_current.iso"
$NewIsoPath = Join-Path $BuildRoot "openjkdf2_xemu_soak_current.new.iso"
$OutRoot = Join-Path $BuildRoot "xemu_soak_runs"
$ManualRunDir = Join-Path $OutRoot "manual_start"
$ManualScreenshotDir = Join-Path $ManualRunDir "screenshots"
$ManifestPath = Join-Path $ManualRunDir "soak_manifest.txt"
$SmokeScript = Join-Path $RepoRoot "tools\xbox_xemu_smoke.ps1"
$SyslinkScript = Join-Path $RepoRoot "scripts\xbox\launch_xemu_syslink_pair.ps1"
$PollScript = Join-Path $RepoRoot "scripts\xbox\poll_xemu_ram_log.py"
$VerifySyslinkScript = Join-Path $RepoRoot "scripts\xbox\verify_syslink_logs.py"
$NativeScreenshotScript = Join-Path $RepoRoot "scripts\xbox\xemu_native_screenshot.py"
$XbePath = Join-Path $ReleaseRoot "default.xbe"
$MapPath = Join-Path $ReleaseRoot "openjkdf2_xbox.exe.map"
$InstanceDir = Join-Path $XemuRoot "OpenJKDF2Soak"
$ConfigPath = Join-Path $InstanceDir "xemu.toml"
$EepromPath = Join-Path $InstanceDir "EEPROM\eeprom_soak.bin"
$XemuExeSource = Join-Path $XemuRoot "xemu.exe"
$XemuExe = Join-Path $InstanceDir "xemu.exe"
$BootRom = Join-Path $XemuRoot "MCPX\mcpx_1.0.bin"
$FlashRom = Join-Path $XemuRoot "BIOS\xbox-4627_debug.bin"
$EepromSource = Join-Path $XemuRoot "EEPROM\eeprom.bin"
$RamLogDir = Join-Path $BuildRoot "xemu_ram_logs"
$SyslinkSourceIso = Join-Path $BuildRoot "openjkdf2_xemu_current.iso"
$SyslinkHostRoleIso = Join-Path $BuildRoot "openjkdf2_xemu_current_host_role.iso"
$SyslinkClientRoleIso = Join-Path $BuildRoot "openjkdf2_xemu_current_client_role.iso"
$SyslinkHostDir = Join-Path $XemuRoot "OpenJKDF2SyslinkHost"
$SyslinkClientDir = Join-Path $XemuRoot "OpenJKDF2SyslinkClient"
$SyslinkHostConfig = Join-Path $SyslinkHostDir "xemu.toml"
$SyslinkClientConfig = Join-Path $SyslinkClientDir "xemu.toml"
$SyslinkHostExe = Join-Path $SyslinkHostDir "xemu.exe"
$SyslinkClientExe = Join-Path $SyslinkClientDir "xemu.exe"
$SyslinkHostIso = Join-Path $SyslinkHostDir "openjkdf2_xemu_current_host.iso"
$SyslinkClientIso = Join-Path $SyslinkClientDir "openjkdf2_xemu_current_client.iso"
$SyslinkScreenshotRoot = Join-Path $BuildRoot "xemu_syslink_screenshots"

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

function Remove-SafeBuildPath([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) {
        return
    }
    Assert-UnderPath $BuildRoot $Path
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

function Ensure-SmokePlayerProfile([string]$StageRoot) {
    $stagePlayerDir = Join-Path $StageRoot "player\Xbox"
    $runtimePlayerDir = Join-Path $RuntimeSource "player\Xbox"
    $repoPlayerDir = Join-Path $RepoRoot "player\Xbox"
    $plrPath = Join-Path $stagePlayerDir "Xbox.plr"

    New-Item -ItemType Directory -Force -Path $stagePlayerDir | Out-Null

    if (Test-Path -LiteralPath $runtimePlayerDir) {
        Copy-Item -Path (Join-Path $runtimePlayerDir "*") -Destination $stagePlayerDir -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $repoPlayerDir) {
        Copy-Item -Path (Join-Path $repoPlayerDir "*.mpc") -Destination $stagePlayerDir -Force -ErrorAction SilentlyContinue
    }

    if (!(Test-Path -LiteralPath $plrPath)) {
        $profile = @'
version 1
diff 1
fullsubtitles 1
disablecutscenes 1
rotateoverlaymap 1
drawstatus 1
crosshair 1
sabercam 0
autoPickup 1
autoSwitch 3
autoReload 0
multiAutoPickup 15
multiAutoSwitch 3
multiAutoReload 3
autoAim 1
flags=4
numCutscenes 0
'@
        Set-Content -LiteralPath $plrPath -Value $profile -Encoding ASCII
    }

    if (!(Get-ChildItem -LiteralPath $stagePlayerDir -Filter "*.mpc" -File -ErrorAction SilentlyContinue | Select-Object -First 1)) {
        $mpcPath = Join-Path $stagePlayerDir "Katarn0.mpc"
        $mpc = @'
version 1
model: ky.3do
soundclass: ky.snd
sidemat: sabergreen1.mat
tipmat: sabergreen0.mat

forcepowers:
bin: 20 value: 8.000000
bin: 21 value: 4.000000
bin: 22 value: 4.000000
bin: 23 value: 4.000000
bin: 24 value: 4.000000
bin: 25 value: 4.000000
bin: 26 value: 4.000000
bin: 27 value: 4.000000
bin: 28 value: 4.000000
bin: 29 value: 4.000000
bin: 30 value: 4.000000
bin: 31 value: 4.000000
bin: 32 value: 4.000000
bin: 33 value: 4.000000
bin: 34 value: 4.000000
spendable stars: 0.000000
'@
        Set-Content -LiteralPath $mpcPath -Value $mpc -Encoding ASCII
    }
}

function New-SoakStage {
    Require-Path $RuntimeSource "OpenJKDF2 runtime source"
    Require-Path $XbePath "Release default.xbe"
    Require-Path (Join-Path $RuntimeSource "Episode\JK1.GOB") "JK1 episode GOB"
    Require-Path (Join-Path $RuntimeSource "Episode\JK1MP.GOB") "JK1MP episode GOB"

    Remove-SafeBuildPath $StagePath
    Remove-SafeBuildPath $NewIsoPath
    Remove-SafeBuildPath $IsoPath

    New-Item -ItemType Directory -Force -Path $StagePath | Out-Null
    robocopy $RuntimeSource $StagePath /E /NFL /NDL /NJH /NJS /NP /XD Logs Screenshots /XF debug_openjkdf2.txt CxbxDebug.txt KrnlDebug.txt | Out-Null
    if ($LASTEXITCODE -gt 7) {
        throw "robocopy runtime staging failed with exit code $LASTEXITCODE"
    }

    Copy-Item -LiteralPath $XbePath -Destination (Join-Path $StagePath "default.xbe") -Force
    foreach ($dashboardAsset in @("TitleImage.xbx", "SaveImage.xbx", "TitleMeta.xbx")) {
        $dashboardAssetPath = Join-Path $ReleaseRoot $dashboardAsset
        if (Test-Path -LiteralPath $dashboardAssetPath) {
            Copy-Item -LiteralPath $dashboardAssetPath -Destination (Join-Path $StagePath $dashboardAsset) -Force
        }
    }
    Ensure-SmokePlayerProfile $StagePath

    if ($FmvLimitSeconds -gt 0) {
        Set-Content -LiteralPath (Join-Path $StagePath "xbox_smoke_fmv_seconds.txt") -Value ([string]$FmvLimitSeconds) -Encoding ASCII
    }
}

function New-SoakXiso {
    $xisoTool = Find-XisoTool
    & $xisoTool -Q -m -c $StagePath $NewIsoPath
    if ($LASTEXITCODE -ne 0) {
        throw "extract-xiso failed with exit code $LASTEXITCODE"
    }
    Move-Item -LiteralPath $NewIsoPath -Destination $IsoPath -Force
}

function Initialize-SingleXemuInstance {
    Require-Path $XemuExeSource "xemu.exe"
    Require-Path $BootRom "MCPX boot ROM"
    Require-Path $FlashRom "Xbox BIOS"
    Require-Path $EepromSource "EEPROM source"
    Require-Path $HddPath "XEMU HDD image"
    Require-Path $IsoPath "Prepared soak XISO"

    New-Item -ItemType Directory -Force -Path $InstanceDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $InstanceDir "EEPROM") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $InstanceDir "shaders") | Out-Null
    New-Item -ItemType Directory -Force -Path $ManualScreenshotDir | Out-Null

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
screenshot_dir = '$ManualScreenshotDir'
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

[display.debug.video]
advanced_tree_state = true

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

function Write-Manifest {
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("OpenJKDF2 Xbox XEMU soak readiness")
    $lines.Add("created=$((Get-Date).ToString('s'))")
    $lines.Add("repo=$RepoRoot")
    $lines.Add("xbe=$XbePath")
    $lines.Add("singleIso=$IsoPath")
    $lines.Add("singleConfig=$ConfigPath")
    $lines.Add("singleMonitor=127.0.0.1:$MonitorPort")
    $lines.Add("menuSeconds=$MenuSeconds")
    $lines.Add("mapSeconds=$MapSeconds")
    $lines.Add("splitSeconds=$SplitSeconds")
    $lines.Add("pollIntervalSeconds=$PollIntervalSeconds")
    $lines.Add("screenshotEverySeconds=$ScreenshotEverySeconds")
    $lines.Add("fmvLimitSeconds=$FmvLimitSeconds")
    $lines.Add("disableCutscenes=$([bool]$DisableCutscenes)")
    $lines.Add("singlePlayerMaps=$($SinglePlayerMaps -join ',')")
    $lines.Add("")
    $lines.Add("No XEMU process was launched by prepare-only mode.")
    $lines.Add("")
    $lines.Add("Manual single-instance launch:")
    $lines.Add("& '$XemuExe' -config_path '$ConfigPath' -monitor 'tcp:127.0.0.1:$MonitorPort,server,nowait'")
    $lines.Add("")
    $lines.Add("Supervised soak launch:")
    $lines.Add("powershell -NoProfile -ExecutionPolicy Bypass -File '$PSCommandPath' -Run")
    $lines.Add("")
    if (!$SkipSystemLinkPrep) {
        $lines.Add("Prepared 4P System Link host config=$SyslinkHostConfig")
        $lines.Add("Prepared 4P System Link client config=$SyslinkClientConfig")
        $lines.Add("Prepared 4P System Link hostIso=$SyslinkHostIso")
        $lines.Add("Prepared 4P System Link clientIso=$SyslinkClientIso")
        $lines.Add("")
        $lines.Add("Manual 4P System Link host launch:")
        $lines.Add("& '$SyslinkHostExe' -config_path '$SyslinkHostConfig' -monitor 'tcp:127.0.0.1:4488,server,nowait'")
        $lines.Add("Manual 4P System Link client launch:")
        $lines.Add("& '$SyslinkClientExe' -config_path '$SyslinkClientConfig' -monitor 'tcp:127.0.0.1:4489,server,nowait'")
        $lines.Add("")
        $lines.Add("Regenerate and launch 4P System Link through the existing runner:")
        $lines.Add("powershell -NoProfile -ExecutionPolicy Bypass -File '$SyslinkScript' -RebuildIso -Smoke -FourPlayerStress -UdpBackend -HostHdd '$HostHdd' -ClientHdd '$ClientHdd'")
    }
    Set-Content -LiteralPath $ManifestPath -Value $lines -Encoding UTF8
}

function Remove-SystemLinkBuildIntermediates {
    foreach ($path in @($SyslinkSourceIso, $SyslinkHostRoleIso, $SyslinkClientRoleIso)) {
        if (Test-Path -LiteralPath $path) {
            Assert-UnderPath $BuildRoot $path
            Remove-Item -LiteralPath $path -Force
        }
    }
}

function Invoke-SystemLinkSetupOnly {
    if ($SkipSystemLinkPrep) {
        return
    }
    Require-Path $SyslinkScript "System Link launcher"
    Require-Path $HostHdd "Host XEMU HDD"
    Require-Path $ClientHdd "Client XEMU HDD"
    & $SyslinkScript -SetupOnly -RebuildIso -Smoke -FourPlayerStress -UdpBackend -HostHdd $HostHdd -ClientHdd $ClientHdd
    Remove-SystemLinkBuildIntermediates
}

function Invoke-CleanArtifacts {
    Remove-SafeBuildPath $StagePath
    Remove-SafeBuildPath $NewIsoPath
    Remove-SafeBuildPath $IsoPath
    Remove-SafeBuildPath $ManualRunDir
    if (!$SkipSystemLinkPrep -and (Test-Path -LiteralPath $SyslinkScript)) {
        & $SyslinkScript -CleanArtifacts -HostHdd $HostHdd -ClientHdd $ClientHdd
    }
}

function Invoke-PrepareOnly {
    New-Item -ItemType Directory -Force -Path $ManualRunDir | Out-Null
    New-SoakStage
    New-SoakXiso
    Remove-SafeBuildPath $StagePath
    Initialize-SingleXemuInstance
    Invoke-SystemLinkSetupOnly
    Write-Manifest

    Write-Host "Prepared XEMU soak artifacts. No XEMU process was launched."
    Write-Host "Manifest:      $ManifestPath"
    Write-Host "Single config: $ConfigPath"
    Write-Host "Single XISO:   $IsoPath"
    Write-Host "Manual launch: & '$XemuExe' -config_path '$ConfigPath' -monitor 'tcp:127.0.0.1:$MonitorPort,server,nowait'"
    Write-Host "Soak launch:   powershell -NoProfile -ExecutionPolicy Bypass -File '$PSCommandPath' -Run"
}

function Invoke-SinglePhase([string]$Label, [string]$AutoStartLevel, [int]$DurationSeconds) {
    $phaseArgs = @{
        DurationSeconds = $DurationSeconds
        PollIntervalSeconds = $PollIntervalSeconds
        RunLabel = $Label
        MonitorPort = $MonitorPort
        FmvLimitSeconds = $FmvLimitSeconds
        ScreenshotEverySeconds = $ScreenshotEverySeconds
        RuntimeSource = $RuntimeSource
        XemuRoot = $XemuRoot
        HddPath = $HddPath
    }
    if ($DisableCutscenes) {
        $phaseArgs.DisableCutscenes = $true
    }
    if (![string]::IsNullOrWhiteSpace($AutoStartLevel)) {
        $phaseArgs.AutoStartLevel = $AutoStartLevel
    }

    & $SmokeScript @phaseArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Single-instance soak phase failed: $Label"
    }
}

function Get-XemuProcessForConfig([string]$Path) {
    $needle = Get-FullPath $Path
    Get-CimInstance Win32_Process -Filter "Name = 'xemu.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($needle, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 }
}

function Read-RamLogHeader([string]$Path) {
    $state = @{
        Writes = -1
        Heartbeats = -1
        Gameplay = $false
    }
    if (!(Test-Path -LiteralPath $Path)) {
        return $state
    }

    $text = Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue
    if ($null -eq $text) {
        return $state
    }

    if ($text -match '(?m)^writes=([0-9]+)') {
        $state.Writes = [int64]$Matches[1]
    }
    if ($text -match '(?m)^heartbeats=([0-9]+)') {
        $state.Heartbeats = [int64]$Matches[1]
    }
    $state.Gameplay = $text.Contains("MPLoadTrace: GameplayShow done")
    return $state
}

function Copy-SystemLinkRamLogs([string]$DestinationDir, [string]$Suffix) {
    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
    foreach ($entry in @(@{ Name = "host"; Port = 4488 }, @{ Name = "client"; Port = 4489 })) {
        $src = Join-Path $RamLogDir ("port{0}_openjkdf2_ram_log.txt" -f $entry.Port)
        if (Test-Path -LiteralPath $src) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $DestinationDir ("{0}_{1}.txt" -f $entry.Name, $Suffix)) -Force
        }
    }
}

function Invoke-SystemLinkPhase {
    if ($SkipSystemLinkPrep) {
        Write-Host "Skipping 4P System Link soak leg because -SkipSystemLinkPrep was supplied."
        return
    }

    Require-Path $PollScript "RAM log poller"
    Require-Path $VerifySyslinkScript "System Link log verifier"
    Require-Path $MapPath "Release map file"
    Require-Path $XbePath "Release default.xbe"

    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $phaseDir = Join-Path $OutRoot "$timestamp-syslink-4p"
    $phaseShotDir = Join-Path $phaseDir "screenshots"
    $phaseHeaderDir = Join-Path $phaseDir "header_polls"
    New-Item -ItemType Directory -Force -Path $phaseDir | Out-Null
    New-Item -ItemType Directory -Force -Path $phaseShotDir | Out-Null
    New-Item -ItemType Directory -Force -Path $phaseHeaderDir | Out-Null
    New-Item -ItemType Directory -Force -Path $RamLogDir | Out-Null

    $result = & $SyslinkScript -RebuildIso -Smoke -FourPlayerStress -UdpBackend -HostHdd $HostHdd -ClientHdd $ClientHdd
    $hostProc = @(Get-XemuProcessForConfig $SyslinkHostConfig | Select-Object -First 1)
    $clientProc = @(Get-XemuProcessForConfig $SyslinkClientConfig | Select-Object -First 1)
    $hostPid = if ($hostProc.Count -gt 0) { $hostProc[0].ProcessId } else { $null }
    $clientPid = if ($clientProc.Count -gt 0) { $clientProc[0].ProcessId } else { $null }

    $start = Get-Date
    $deadline = $start.AddSeconds($SplitSeconds)
    $nextScreenshot = if ($ScreenshotEverySeconds -gt 0) { $start.AddSeconds($ScreenshotEverySeconds) } else { [DateTime]::MaxValue }
    $pollIndex = 0
    $lastPollStates = @{}
    $stalePollCounts = @{}
    $gameplaySeen = @{}

    try {
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Seconds $PollIntervalSeconds
            $pollIndex++
            $elapsed = [int]((Get-Date) - $start).TotalSeconds
            $pollOut = Join-Path $phaseDir ("poll_{0:D3}_{1:D4}s.txt" -f $pollIndex, $elapsed)
            $pollOutDir = $RamLogDir
            $pollArgs = @($PollScript, "--ports", "4488,4489", "--map", $MapPath, "--xbe", $XbePath, "--out-dir", $pollOutDir, "--timeout", "4")
            if ($pollIndex -gt 1) {
                $pollOutDir = $phaseHeaderDir
                $pollArgs = @($PollScript, "--ports", "4488,4489", "--map", $MapPath, "--xbe", $XbePath, "--out-dir", $pollOutDir, "--timeout", "4", "--header-only")
            }
            & python @pollArgs *> $pollOut
            if ($LASTEXITCODE -ne 0) {
                throw "4P System Link RAM poll failed. See $pollOut"
            }
            if ($pollIndex -eq 1) {
                Copy-SystemLinkRamLogs $phaseDir ("poll_{0:D3}_{1:D4}s_full" -f $pollIndex, $elapsed)
            }

            foreach ($entry in @(@{ Name = "host"; Port = 4488 }, @{ Name = "client"; Port = 4489 })) {
                $ramPath = Join-Path $pollOutDir ("port{0}_openjkdf2_ram_log.txt" -f $entry.Port)
                $state = Read-RamLogHeader $ramPath
                if ($state.Gameplay) {
                    $gameplaySeen[$entry.Name] = $true
                }

                if ($gameplaySeen[$entry.Name] -and $lastPollStates.ContainsKey($entry.Name)) {
                    $last = $lastPollStates[$entry.Name]
                    if ($state.Writes -le $last.Writes -and $state.Heartbeats -le $last.Heartbeats) {
                        $stalePollCounts[$entry.Name] = 1 + $(if ($stalePollCounts.ContainsKey($entry.Name)) { $stalePollCounts[$entry.Name] } else { 0 })
                    }
                    else {
                        $stalePollCounts[$entry.Name] = 0
                    }

                    if ($stalePollCounts[$entry.Name] -ge 1) {
                        throw ("4P System Link {0} heartbeat stalled after gameplay. last writes/hb={1}/{2}, current writes/hb={3}/{4}. See {5}" -f
                            $entry.Name, $last.Writes, $last.Heartbeats, $state.Writes, $state.Heartbeats, $ramPath)
                    }
                }

                $lastPollStates[$entry.Name] = $state
            }

            if ((Get-Date) -ge $nextScreenshot -and (Test-Path -LiteralPath $NativeScreenshotScript)) {
                foreach ($entry in @(@{ Name = "host"; Pid = $hostPid }, @{ Name = "client"; Pid = $clientPid })) {
                    if ($entry.Pid) {
                        $stableShot = Join-Path $phaseShotDir ("{0}_{1:D4}s.png" -f $entry.Name, $elapsed)
                        $xemuExePath = Join-Path $XemuRoot ("OpenJKDF2Syslink{0}\xemu.exe" -f ($(if ($entry.Name -eq "host") { "Host" } else { "Client" })))
                        $configuredShotDir = Join-Path $SyslinkScreenshotRoot $entry.Name
                        & python $NativeScreenshotScript --pid ([string]$entry.Pid) --xemu-exe $xemuExePath --screenshot-dir $configuredShotDir --timeout 5 *> (Join-Path $phaseDir ("screenshot_{0}_{1:D4}s.txt" -f $entry.Name, $elapsed))
                        $latest = Get-ChildItem -LiteralPath $configuredShotDir -Filter "*.png" -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
                        if ($latest -and $latest.FullName -ne $stableShot) {
                            Copy-Item -LiteralPath $latest.FullName -Destination $stableShot -Force -ErrorAction SilentlyContinue
                        }
                    }
                }
                $nextScreenshot = (Get-Date).AddSeconds($ScreenshotEverySeconds)
            }
        }

        $finalPollOut = Join-Path $phaseDir "poll_final_full.txt"
        & python $PollScript --ports "4488,4489" --map $MapPath --xbe $XbePath --out-dir $RamLogDir --timeout 4 *> $finalPollOut
        if ($LASTEXITCODE -ne 0) {
            throw "4P System Link final RAM poll failed. See $finalPollOut"
        }
        Copy-SystemLinkRamLogs $phaseDir "final_full"

        & python $VerifySyslinkScript --mode harness --log-dir $RamLogDir *> (Join-Path $phaseDir "verify_syslink.txt")
        if ($LASTEXITCODE -ne 0) {
            throw "4P System Link verifier failed. See $phaseDir"
        }
    }
    finally {
        & $SyslinkScript -Stop -HostHdd $HostHdd -ClientHdd $ClientHdd | Out-Null
    }

    $result | Out-File -LiteralPath (Join-Path $phaseDir "launch_result.txt") -Encoding UTF8
    Write-Host "4P System Link soak leg complete: $phaseDir"
}

function Invoke-RunSoak {
    Require-Path $SmokeScript "XEMU smoke runner"
    Invoke-PrepareOnly
    Invoke-SinglePhase "soak-menu-boot" "" $MenuSeconds
    foreach ($map in $SinglePlayerMaps) {
        $label = "soak-sp-" + (($map -replace '\.jkl$', '') -replace '[^A-Za-z0-9_.-]', '_')
        Invoke-SinglePhase $label $map $MapSeconds
    }
    Invoke-SystemLinkPhase
}

if ($CleanArtifacts) {
    Invoke-CleanArtifacts
    Write-Host "Cleaned generated XEMU soak artifacts."
    return
}

if ($DoRun) {
    Invoke-RunSoak
    return
}

if ($SystemLinkOnly) {
    Invoke-SystemLinkPhase
    return
}

if ($DoPrepare) {
    Invoke-PrepareOnly
}
