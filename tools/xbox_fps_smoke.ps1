param(
    [int]$Iterations = 1,
    [int]$WatchdogSeconds = 240
)

$ErrorActionPreference = 'Continue'

$repo = 'C:\Programming\GitHub\OpenJKDF2ogx'
$cxbx = Join-Path $repo 'CXBXR'
$game = 'C:\Games\Emulators\CXBX\openJKDF2x'
$xbeSource = Join-Path $repo 'build\xbox\release\default.xbe'
$xbeDest = Join-Path $game 'default.xbe'
$loader = Join-Path $cxbx 'cxbxr-ldr-project1.exe'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$outRoot = Join-Path $repo "build\xbox\smoke_logs\fps_smoke_$stamp"

New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

function Get-IsolatedCxbxProcesses {
    Get-CimInstance Win32_Process | Where-Object {
        ($_.Name -in @('cxbxr-ldr-project1.exe', 'cxbx-project1.exe')) -or
        ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($cxbx, [StringComparison]::OrdinalIgnoreCase) -and
         ($_.ExecutablePath -match '(cxbxr-ldr-project1|cxbx-project1)\.exe$'))
    }
}

function Stop-IsolatedCxbxProcesses {
    Get-IsolatedCxbxProcesses | ForEach-Object {
        try { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } catch {}
    }
}

function Copy-IfExists($path, $destDir) {
    if (Test-Path $path) {
        Copy-Item $path (Join-Path $destDir (Split-Path $path -Leaf)) -Force -ErrorAction SilentlyContinue
    }
}

for ($i = 1; $i -le $Iterations; $i++) {
    $runDir = Join-Path $outRoot ('run_{0:D2}' -f $i)
    New-Item -ItemType Directory -Force -Path $runDir | Out-Null
    $summary = Join-Path $runDir 'summary.txt'
    $gameLog = Join-Path $game 'debug_openjkdf2.txt'
    $cxbxDebugGame = Join-Path $game 'CxbxDebug.txt'
    $krnlDebugGame = Join-Path $game 'KrnlDebug.txt'
    $cxbxDebugEmu = Join-Path $cxbx 'CxbxDebug.txt'
    $krnlDebugEmu = Join-Path $cxbx 'KrnlDebug.txt'

    Stop-IsolatedCxbxProcesses
    Start-Sleep -Seconds 2

    Copy-Item $xbeSource $xbeDest -Force
    Remove-Item $gameLog, $cxbxDebugGame, $krnlDebugGame, $cxbxDebugEmu, $krnlDebugEmu -Force -ErrorAction SilentlyContinue

    $start = Get-Date
    $p = Start-Process -FilePath $loader -ArgumentList "/load `"$xbeDest`"" -WorkingDirectory $cxbx -PassThru -WindowStyle Hidden
    $deadline = $start.AddSeconds($WatchdogSeconds)

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if ($p.HasExited) { break }
    }

    $aliveAtEnd = -not $p.HasExited
    Stop-IsolatedCxbxProcesses
    Start-Sleep -Seconds 1

    Copy-IfExists $gameLog $runDir
    Copy-IfExists $cxbxDebugGame $runDir
    Copy-IfExists $krnlDebugGame $runDir
    Copy-IfExists $cxbxDebugEmu $runDir
    Copy-IfExists $krnlDebugEmu $runDir

    $heartbeats = 0
    $fatalCount = 0
    $perfLines = @()
    $tail = @()
    if (Test-Path $gameLog) {
        $heartbeats = @(Select-String -Path $gameLog -Pattern '^Perf:').Count
        $fatalCount = @(Select-String -Path $gameLog -Pattern 'Received Exception|FATAL|Out of memory|E_OUTOFMEMORY|D3D Error|Unhandled|Could not load level').Count
        $perfLines = @(Select-String -Path $gameLog -Pattern '^Perf:' | Select-Object -Last 12 | ForEach-Object { $_.Line })
        $tail = @(Get-Content $gameLog -Tail 80)
    }

    $emuFatal = 0
    foreach ($emuLog in @($cxbxDebugGame, $krnlDebugGame, $cxbxDebugEmu, $krnlDebugEmu)) {
        if (Test-Path $emuLog) {
            $emuFatal += @(Select-String -Path $emuLog -Pattern 'Received Exception|Exception|EIP|FATAL|Unhandled|D3D Error|E_OUTOFMEMORY').Count
        }
    }

    @(
        "run=$i"
        "started=$start"
        "watchdogSeconds=$WatchdogSeconds"
        "aliveAtEnd=$aliveAtEnd"
        "heartbeatCount=$heartbeats"
        "gameFatalCount=$fatalCount"
        "emuFatalCount=$emuFatal"
        "perfTail:"
        $perfLines
        "logTail:"
        $tail
    ) | Set-Content -Path $summary

    Get-Content $summary
}

"smokeLogRoot=$outRoot"
