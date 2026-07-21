param(
    [int]$WatchdogSeconds = 300,
    [string]$RunLabel = "normal-boot",
    [string]$CxbxRoot = "C:\Programming\GitHub\OpenJKDF2ogx\CXBXR",
    [string]$AppDir = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$BuildDir = "C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release",
    [string]$OutRoot = "C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\smoke_runs",
    [int]$FmvLimitSeconds = 0,
    [string]$AutoStartLevel = "",
    [string]$AutoStartArgs = "",
    [int]$BotCount = 0,
    [int]$BotMatchSeconds = 0,
    [switch]$DisableMusic
)

$ErrorActionPreference = "Stop"

$xbeSrc = Join-Path $BuildDir "default.xbe"
$xbeDst = Join-Path $AppDir "default.xbe"
$loader = Join-Path $CxbxRoot "cxbxr-ldr-project1.exe"
$gameLog = Join-Path $AppDir "debug_openjkdf2.txt"
$cxbxDebugLog = Join-Path $AppDir "CxbxDebug.txt"
$krnlDebugLog = Join-Path $AppDir "KrnlDebug.txt"
$fallbackGameLog = Join-Path $CxbxRoot "EmuDisk\Partition1\debug_openjkdf2.txt"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$safeLabel = ($RunLabel -replace '[^A-Za-z0-9_.-]', '_')
$runDir = Join-Path $OutRoot "$timestamp-$safeLabel"
$stdoutPath = Join-Path $runDir "cxbxr-stdout.txt"
$stderrPath = Join-Path $runDir "cxbxr-stderr.txt"
$summaryPath = Join-Path $runDir "summary.txt"
$copiedGameLog = Join-Path $runDir "debug_openjkdf2.txt"
$copiedCxbxDebugLog = Join-Path $runDir "CxbxDebug.txt"
$copiedKrnlDebugLog = Join-Path $runDir "KrnlDebug.txt"
$fmvLimitPath = Join-Path $AppDir "xbox_smoke_fmv_seconds.txt"
$autoStartPath = Join-Path $AppDir "xbox_smoke_autostart_level.txt"
$autoStartArgsPath = Join-Path $AppDir "xbox_smoke_autostart_args.txt"
$disableMusicPath = Join-Path $AppDir "xbox_smoke_disable_music.txt"

function Get-CxbxProcesses {
    $root = try { (Resolve-Path -LiteralPath $CxbxRoot -ErrorAction Stop).Path } catch { $CxbxRoot }
    $allowedNames = @(
        "cxbx-project1.exe",
        "cxbxr-ldr-project1.exe"
    )

    Get-CimInstance Win32_Process |
        Where-Object {
            $allowedNames -contains $_.Name -and
            $_.ExecutablePath -and
            $_.ExecutablePath.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)
        }
}

function Stop-CxbxProcesses {
    Get-CxbxProcesses | ForEach-Object {
        try {
            Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
        }
        catch {
        }
    }
}

function Wait-ForCxbxFree {
    param([int]$WaitSeconds = 180)

    $running = @(Get-CxbxProcesses)
    if ($running.Count -eq 0) {
        return $true
    }

    Write-Output "cxbxBusy=True"
    Write-Output "cxbxBusyProcessIds=$($running.ProcessId -join ',')"
    Write-Output "waitingSeconds=$WaitSeconds"
    Start-Sleep -Seconds $WaitSeconds

    $running = @(Get-CxbxProcesses)
    if ($running.Count -eq 0) {
        Write-Output "cxbxBusyAfterWait=False"
        return $true
    }

    Write-Output "cxbxBusyAfterWait=True"
    Write-Output "cxbxBusyProcessIdsAfterWait=$($running.ProcessId -join ',')"
    return $false
}

function Count-Matches($Path, $Patterns) {
    if (!(Test-Path -LiteralPath $Path)) { return 0 }
    return @(
        Select-String -Path $Path -Pattern $Patterns -ErrorAction SilentlyContinue
    ).Count
}

function Test-LogPattern($Path, $Pattern) {
    if (!(Test-Path -LiteralPath $Path)) { return $false }
    return [bool](Select-String -Path $Path -Pattern $Pattern -Quiet -ErrorAction SilentlyContinue)
}

function Get-ReachedStates($Path) {
    $states = New-Object System.Collections.Generic.List[string]
    if (Test-LogPattern $Path "sithWorld_Load: opened OK 'jkl\\static\.jkl'") { $states.Add("static") }
    if (Test-LogPattern $Path "CutsceneTrace:|XmvDbg|XMV") { $states.Add("fmv") }
    if (Test-LogPattern $Path "jkGuiMain_Show: smoke autostart") { $states.Add("autostart") }
    if (Test-LogPattern $Path "GameplayShow:.*loading level|sithWorld_Load: opened OK 'jkl\\01narshadda\.jkl'") { $states.Add("level-load") }
    if (Test-LogPattern $Path "sithWorld_Load: section end things") { $states.Add("level-things") }
    if (Test-LogPattern $Path "GameplayShow: done") { $states.Add("gameplay-show-done") }
    if (Test-LogPattern $Path "GameplayTick: enter|sithTick: enter") { $states.Add("first-tick") }
    if (Test-LogPattern $Path "XboxFrame: begin n=") { $states.Add("xbox-frame") }
    if (Test-LogPattern $Path "BotNav: generated") { $states.Add("botnav") }
    if (Test-LogPattern $Path "BotMatch: scoreboard reason=timed-final") { $states.Add("botmatch-final") }
    if ($states.Count -eq 0) { return "none" }
    return ($states -join ",")
}

if (!(Test-Path -LiteralPath $xbeSrc)) { throw "Missing built XBE: $xbeSrc" }
if (!(Test-Path -LiteralPath $loader)) { throw "Missing CXBX-R loader: $loader" }
if (!(Test-Path -LiteralPath $AppDir)) { throw "Missing CXBX-R app directory: $AppDir" }

New-Item -ItemType Directory -Path $runDir -Force | Out-Null

if (!(Wait-ForCxbxFree -WaitSeconds 180)) {
    "aborted=Existing CXBX-R process still running after wait" |
        Set-Content -LiteralPath $summaryPath -Encoding ASCII
    exit 3
}

Stop-CxbxProcesses
Start-Sleep -Seconds 2

Copy-Item -LiteralPath $xbeSrc -Destination $xbeDst -Force
Remove-Item -LiteralPath $gameLog -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $cxbxDebugLog -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $krnlDebugLog -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $fallbackGameLog -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $fmvLimitPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $autoStartPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $autoStartArgsPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $disableMusicPath -Force -ErrorAction SilentlyContinue
if ($FmvLimitSeconds -gt 0) {
    Set-Content -LiteralPath $fmvLimitPath -Value ([string]$FmvLimitSeconds) -Encoding ASCII
}
if ($AutoStartLevel.Length -gt 0) {
    Set-Content -LiteralPath $autoStartPath -Value $AutoStartLevel -Encoding ASCII
}
if ($AutoStartArgs.Length -eq 0 -and $BotCount -gt 0) {
    $AutoStartArgs = "-autostart -mp -episode JK1MP -map m2.jkl -bots $BotCount"
    if ($BotMatchSeconds -gt 0) {
        $AutoStartArgs += " -botmatch-seconds $BotMatchSeconds"
    }
}
if ($AutoStartArgs.Length -gt 0) {
    Set-Content -LiteralPath $autoStartArgsPath -Value $AutoStartArgs -Encoding ASCII -NoNewline
}
if ($DisableMusic) {
    Set-Content -LiteralPath $disableMusicPath -Value "1" -Encoding ASCII
}

$start = Get-Date
$proc = Start-Process `
    -FilePath $loader `
    -ArgumentList "/load `"$xbeDst`"" `
    -WorkingDirectory $CxbxRoot `
    -PassThru `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath

$deadline = $start.AddSeconds($WatchdogSeconds)
$lastLogLength = 0
$lastLogWrite = $null

while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2

    $activeLog = if (Test-Path -LiteralPath $gameLog) { $gameLog } elseif (Test-Path -LiteralPath $fallbackGameLog) { $fallbackGameLog } else { $null }
    if ($activeLog) {
        $item = Get-Item -LiteralPath $activeLog
        $lastLogLength = $item.Length
        $lastLogWrite = $item.LastWriteTime
    }

    $aliveProcesses = @(Get-CxbxProcesses)
    if ($aliveProcesses.Count -eq 0 -and $proc.HasExited) {
        break
    }
}

$aliveAtEnd = @(Get-CxbxProcesses).Count -gt 0
$end = Get-Date

Stop-CxbxProcesses
Start-Sleep -Seconds 1
Remove-Item -LiteralPath $fmvLimitPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $autoStartPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $autoStartArgsPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $disableMusicPath -Force -ErrorAction SilentlyContinue

$activeLogAfter = if (Test-Path -LiteralPath $gameLog) { $gameLog } elseif (Test-Path -LiteralPath $fallbackGameLog) { $fallbackGameLog } else { $null }
if ($activeLogAfter) {
    Copy-Item -LiteralPath $activeLogAfter -Destination $copiedGameLog -Force
}
if (Test-Path -LiteralPath $cxbxDebugLog) {
    Copy-Item -LiteralPath $cxbxDebugLog -Destination $copiedCxbxDebugLog -Force
}
if (Test-Path -LiteralPath $krnlDebugLog) {
    Copy-Item -LiteralPath $krnlDebugLog -Destination $copiedKrnlDebugLog -Force
}

$heartbeatPatterns = @(
    "HEARTBEAT",
    "XboxFrame: begin",
    "CutsceneTrace: Present ok",
    "XmvDbg:",
    "GameplayTick:",
    "sithTick:",
    "TickAll:",
    "main: tick",
    "GameplayShow: done"
)
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

$parsePath = if (Test-Path -LiteralPath $copiedGameLog) { $copiedGameLog } else { $gameLog }
$heartbeatCount = Count-Matches $parsePath $heartbeatPatterns
$fatalCount = Count-Matches $parsePath $fatalPatterns
$emulatorFatalCount = 0
if (Test-Path -LiteralPath $copiedCxbxDebugLog) {
    $emulatorFatalCount += Count-Matches $copiedCxbxDebugLog $fatalPatterns
}
if (Test-Path -LiteralPath $copiedKrnlDebugLog) {
    $emulatorFatalCount += Count-Matches $copiedKrnlDebugLog $fatalPatterns
}
$reached = Get-ReachedStates $parsePath
$duration = [int]($end - $start).TotalSeconds
$exitCode = if ($proc.HasExited) { $proc.ExitCode } else { "still-running" }

$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("runLabel=$RunLabel")
$summary.Add("runDir=$runDir")
$summary.Add("start=$($start.ToString('s'))")
$summary.Add("durationSeconds=$duration")
$summary.Add("watchdogSeconds=$WatchdogSeconds")
$summary.Add("fmvLimitSeconds=$FmvLimitSeconds")
$summary.Add("autoStartLevel=$AutoStartLevel")
$summary.Add("autoStartArgs=$AutoStartArgs")
$summary.Add("botCount=$BotCount")
$summary.Add("botMatchSeconds=$BotMatchSeconds")
$summary.Add("disableMusic=$([bool]$DisableMusic)")
$summary.Add("loader=$loader")
$summary.Add("managedProcessNames=cxbx-project1.exe,cxbxr-ldr-project1.exe")
$summary.Add("xbeSource=$xbeSrc")
$summary.Add("xbeDest=$xbeDst")
$summary.Add("loaderExitCode=$exitCode")
$summary.Add("aliveAtEnd=$aliveAtEnd")
$summary.Add("gameLog=$gameLog")
$summary.Add("cxbxDebugLog=$cxbxDebugLog")
$summary.Add("krnlDebugLog=$krnlDebugLog")
$summary.Add("fallbackGameLog=$fallbackGameLog")
$summary.Add("activeGameLog=$activeLogAfter")
$summary.Add("gameLogCopied=$([bool](Test-Path -LiteralPath $copiedGameLog))")
$summary.Add("cxbxDebugLogCopied=$([bool](Test-Path -LiteralPath $copiedCxbxDebugLog))")
$summary.Add("krnlDebugLogCopied=$([bool](Test-Path -LiteralPath $copiedKrnlDebugLog))")
$summary.Add("lastLogLength=$lastLogLength")
$summary.Add("lastLogWrite=$lastLogWrite")
$summary.Add("heartbeatCount=$heartbeatCount")
$summary.Add("fatalCount=$fatalCount")
$summary.Add("emulatorFatalCount=$emulatorFatalCount")
$summary.Add("reached=$reached")
$summary.Add("")
$summary.Add("lastUsefulLogLines:")

if (Test-Path -LiteralPath $parsePath) {
    $usefulPatterns = $heartbeatPatterns + $fatalPatterns + @(
        "sithWorld_Load:",
        "stdMci:",
        "stdSound_XboxCreateBuffer:",
        "GameplayShow:",
        "BotNav:",
        "BotMatch:",
        "CutsceneTrace:",
        "XmvDbg"
    )
    $tail = Select-String -Path $parsePath -Pattern $usefulPatterns -ErrorAction SilentlyContinue |
        Select-Object -Last 120 |
        ForEach-Object { $_.Line }
    $tail | ForEach-Object { $summary.Add($_) }
}
else {
    $summary.Add("logMissing=True")
}

foreach ($emuLog in @($copiedCxbxDebugLog, $copiedKrnlDebugLog)) {
    if (Test-Path -LiteralPath $emuLog) {
        $summary.Add("")
        $summary.Add("lastEmulatorLogLines:$([System.IO.Path]::GetFileName($emuLog))")
        Get-Content -LiteralPath $emuLog -Tail 80 | ForEach-Object { $summary.Add($_) }
    }
}

$summary | Set-Content -LiteralPath $summaryPath -Encoding ASCII
$summary | ForEach-Object { Write-Output $_ }

if (!(Test-Path -LiteralPath $parsePath)) {
    exit 2
}
if ($fatalCount -gt 0 -or $emulatorFatalCount -gt 0) {
    exit 1
}
exit 0
