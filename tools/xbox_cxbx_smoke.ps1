param(
    [int]$WatchdogSeconds = 300,
    [string]$RunLabel = "normal-boot",
    [string]$CxbxRoot = "C:\Games\Emulators\CXBX",
    [string]$AppDir = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$BuildDir = "C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release",
    [string]$OutRoot = "C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\smoke_runs"
)

$ErrorActionPreference = "Stop"

$xbeSrc = Join-Path $BuildDir "default.xbe"
$xbeDst = Join-Path $AppDir "default.xbe"
$loader = Join-Path $CxbxRoot "cxbxr-ldr.exe"
$gameLog = Join-Path $CxbxRoot "EmuDisk\Partition1\debug_openjkdf2.txt"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$safeLabel = ($RunLabel -replace '[^A-Za-z0-9_.-]', '_')
$runDir = Join-Path $OutRoot "$timestamp-$safeLabel"
$stdoutPath = Join-Path $runDir "cxbxr-stdout.txt"
$stderrPath = Join-Path $runDir "cxbxr-stderr.txt"
$summaryPath = Join-Path $runDir "summary.txt"
$copiedGameLog = Join-Path $runDir "debug_openjkdf2.txt"

function Get-CxbxProcesses {
    Get-CimInstance Win32_Process |
        Where-Object {
            $_.Name -match '^(cxbx|cxbxr)' -or
            ($_.ExecutablePath -and $_.ExecutablePath -match '\\CXBX\\.*(cxbx|cxbxr)')
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
    if (Test-LogPattern $Path "GameplayShow:.*loading level|sithWorld_Load: opened OK 'jkl\\01narshadda\.jkl'") { $states.Add("level-load") }
    if (Test-LogPattern $Path "sithWorld_Load: section end things") { $states.Add("level-things") }
    if (Test-LogPattern $Path "GameplayShow: done") { $states.Add("gameplay-show-done") }
    if (Test-LogPattern $Path "GameplayTick: enter|sithTick: enter") { $states.Add("first-tick") }
    if (Test-LogPattern $Path "XboxFrame: begin n=") { $states.Add("xbox-frame") }
    if ($states.Count -eq 0) { return "none" }
    return ($states -join ",")
}

if (!(Test-Path -LiteralPath $xbeSrc)) { throw "Missing built XBE: $xbeSrc" }
if (!(Test-Path -LiteralPath $loader)) { throw "Missing CXBX-R loader: $loader" }
if (!(Test-Path -LiteralPath $AppDir)) { throw "Missing CXBX-R app directory: $AppDir" }

New-Item -ItemType Directory -Path $runDir -Force | Out-Null

Stop-CxbxProcesses
Start-Sleep -Seconds 2

Copy-Item -LiteralPath $xbeSrc -Destination $xbeDst -Force
Remove-Item -LiteralPath $gameLog -Force -ErrorAction SilentlyContinue

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

    if (Test-Path -LiteralPath $gameLog) {
        $item = Get-Item -LiteralPath $gameLog
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

if (Test-Path -LiteralPath $gameLog) {
    Copy-Item -LiteralPath $gameLog -Destination $copiedGameLog -Force
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
$reached = Get-ReachedStates $parsePath
$duration = [int]($end - $start).TotalSeconds
$exitCode = if ($proc.HasExited) { $proc.ExitCode } else { "still-running" }

$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("runLabel=$RunLabel")
$summary.Add("runDir=$runDir")
$summary.Add("start=$($start.ToString('s'))")
$summary.Add("durationSeconds=$duration")
$summary.Add("watchdogSeconds=$WatchdogSeconds")
$summary.Add("loader=$loader")
$summary.Add("xbeSource=$xbeSrc")
$summary.Add("xbeDest=$xbeDst")
$summary.Add("loaderExitCode=$exitCode")
$summary.Add("aliveAtEnd=$aliveAtEnd")
$summary.Add("gameLog=$gameLog")
$summary.Add("gameLogCopied=$([bool](Test-Path -LiteralPath $copiedGameLog))")
$summary.Add("lastLogLength=$lastLogLength")
$summary.Add("lastLogWrite=$lastLogWrite")
$summary.Add("heartbeatCount=$heartbeatCount")
$summary.Add("fatalCount=$fatalCount")
$summary.Add("reached=$reached")
$summary.Add("")
$summary.Add("lastUsefulLogLines:")

if (Test-Path -LiteralPath $parsePath) {
    $usefulPatterns = $heartbeatPatterns + $fatalPatterns + @(
        "sithWorld_Load:",
        "stdMci:",
        "stdSound_XboxCreateBuffer:",
        "GameplayShow:",
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

$summary | Set-Content -LiteralPath $summaryPath -Encoding ASCII
$summary | ForEach-Object { Write-Output $_ }

if (!(Test-Path -LiteralPath $parsePath)) {
    exit 2
}
if ($fatalCount -gt 0) {
    exit 1
}
exit 0
