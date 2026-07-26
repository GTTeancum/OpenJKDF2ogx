param(
    [Parameter(Mandatory = $true)]
    [string[]]$RunDir,
    [double]$MaxRouteStallsPerBotMinute = 0.90,
    [int]$MinDistinctRangedWeapons = 2,
    [switch]$AllowUnmuted
)

$ErrorActionPreference = "Stop"
$allPassed = $true

function Get-SummaryValues([string]$path) {
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $path) {
        if ($line -notmatch '^([^=]+)=(.*)$') {
            continue
        }
        $values[$Matches[1]] = $Matches[2]
    }
    return $values
}

function Get-LastMatchingLine([string[]]$lines, [string]$pattern) {
    return $lines | Where-Object { $_ -match $pattern } | Select-Object -Last 1
}

foreach ($requestedDir in $RunDir) {
    $resolvedDir = (Resolve-Path -LiteralPath $requestedDir).Path
    $summaryPath = Join-Path $resolvedDir "summary.txt"
    $logPath = Join-Path $resolvedDir "debug_openjkdf2.txt"
    if (!(Test-Path -LiteralPath $summaryPath) -or !(Test-Path -LiteralPath $logPath)) {
        throw "Run directory must contain summary.txt and debug_openjkdf2.txt: $resolvedDir"
    }

    $summary = Get-SummaryValues $summaryPath
    $logLines = Get-Content -LiteralPath $logPath
    $failures = New-Object System.Collections.Generic.List[string]

    $scoreboardLine = Get-LastMatchingLine $logLines '^BotMatch: scoreboard reason=timed-final '
    $qualityLine = Get-LastMatchingLine $logLines '^BotMatch: quality '
    $weaponLine = Get-LastMatchingLine $logLines '^BotMatch: weapon-shots '
    if (!$scoreboardLine) { $failures.Add("missing timed-final scoreboard") }
    if (!$qualityLine) { $failures.Add("missing final quality counters") }
    if (!$weaponLine) { $failures.Add("missing final weapon counters") }

    $elapsedMs = 0
    $playerCount = 0
    if ($scoreboardLine -and
        $scoreboardLine -match 'elapsedMs=(\d+) players=(\d+)') {
        $elapsedMs = [int64]$Matches[1]
        $playerCount = [int]$Matches[2]
    }

    $jumpDetected = 0
    $jumpLanded = 0
    $jumpRetry = 0
    $jumpFailed = 0
    $jumpTimeout = 0
    $routeStalls = 0
    $noLosFireAttempts = 0
    if ($qualityLine -and
        $qualityLine -match 'jumpDetected=(\d+) jumpLanded=(\d+) jumpRetry=(\d+) jumpFailed=(\d+) jumpTimeout=(\d+).*routeStalls=(\d+) noLosFireAttempts=(\d+)') {
        $jumpDetected = [int]$Matches[1]
        $jumpLanded = [int]$Matches[2]
        $jumpRetry = [int]$Matches[3]
        $jumpFailed = [int]$Matches[4]
        $jumpTimeout = [int]$Matches[5]
        $routeStalls = [int]$Matches[6]
        $noLosFireAttempts = [int]$Matches[7]
    }

    $scoreboardIndex = -1
    for ($i = $logLines.Count - 1; $i -ge 0; $i--) {
        if ($logLines[$i] -match '^BotMatch: scoreboard reason=timed-final ') {
            $scoreboardIndex = $i
            break
        }
    }
    $finalLines = if ($scoreboardIndex -ge 0) {
        $logLines[$scoreboardIndex..($logLines.Count - 1)]
    } else {
        @()
    }

    $botScores = @()
    foreach ($line in $finalLines | Where-Object { $_ -match '^BotMatch: score slot=' }) {
        if ($line -match "slot=(\d+) bot=1 name='([^']*)' kills=(-?\d+) deaths=(\d+) suicides=(\d+)") {
            $botScores += [pscustomobject]@{
                Slot = [int]$Matches[1]
                Name = $Matches[2]
                Kills = [int]$Matches[3]
                Deaths = [int]$Matches[4]
                Suicides = [int]$Matches[5]
            }
        }
    }
    $botScores = $botScores | Sort-Object Slot -Unique
    $botCount = @($botScores).Count
    $totalKills = ($botScores | Measure-Object -Property Kills -Sum).Sum
    $totalSuicides = ($botScores | Measure-Object -Property Suicides -Sum).Sum
    if ($null -eq $totalKills) { $totalKills = 0 }
    if ($null -eq $totalSuicides) { $totalSuicides = 0 }

    $loadedSlots = @{}
    $models = @{}
    foreach ($line in $logLines | Where-Object { $_ -match '^BotMatch: spawn-loadout ' }) {
        if ($line -match "slot=(\d+) weapon=2 available=(\d+) carries=(\d+) ammo=([0-9.]+) model='([^']+)'") {
            $slot = [int]$Matches[1]
            if ([int]$Matches[2] -eq 1 -and [int]$Matches[3] -eq 1 -and [double]$Matches[4] -gt 0) {
                $loadedSlots[$slot] = $true
            }
            $models[$Matches[5]] = $true
        }
    }

    $weaponCounts = @{}
    if ($weaponLine) {
        foreach ($match in [regex]::Matches($weaponLine, '(bryar|strifle|crossbow|repeater|rail|concussion|scope|blastech)=(\d+)')) {
            $weaponCounts[$match.Groups[1].Value] = [int]$match.Groups[2].Value
        }
    }
    $distinctRangedWeapons = @($weaponCounts.GetEnumerator() |
        Where-Object { $_.Value -gt 0 }).Count
    $forceActions = @($logLines | Where-Object { $_ -match '^BotMatch: force-(heal|push|lightning) ' }).Count
    $pickupActions = @($logLines | Where-Object {
        $_ -match '^BotMatch: pickup(-health|-fallback)? '
    }).Count

    $minutes = if ($elapsedMs -gt 0) { $elapsedMs / 60000.0 } else { 0.0 }
    $stallRate = if ($botCount -gt 0 -and $minutes -gt 0) {
        $routeStalls / ($botCount * $minutes)
    } else {
        [double]::PositiveInfinity
    }

    foreach ($requiredKey in @("fatalCount", "emulatorFatalCount", "reached", "muteAudio")) {
        if (!$summary.ContainsKey($requiredKey)) {
            $failures.Add("summary is missing $requiredKey")
        }
    }
    if ([int]$summary["fatalCount"] -ne 0) { $failures.Add("game fatal count is not zero") }
    if ([int]$summary["emulatorFatalCount"] -ne 0) { $failures.Add("emulator fatal count is not zero") }
    if ($summary["reached"] -notmatch 'botmatch-final') { $failures.Add("run did not reach botmatch-final") }
    if (!$AllowUnmuted -and $summary["muteAudio"] -ne "True") { $failures.Add("run was not muted") }
    if ($summary.ContainsKey("botCount") -and [int]$summary["botCount"] -ne $botCount) {
        $failures.Add("requested bots=$($summary["botCount"]), final bots=$botCount")
    }
    if ($botCount -lt 1) { $failures.Add("no final bot scores") }
    if ($totalKills -lt 1) { $failures.Add("bots recorded no kills") }
    if ($totalSuicides -ne 0) { $failures.Add("bot suicides=$totalSuicides") }
    if ($noLosFireAttempts -ne 0) { $failures.Add("no-LOS fire attempts=$noLosFireAttempts") }
    if ($jumpRetry -ne 0 -or $jumpFailed -ne 0 -or $jumpTimeout -ne 0) {
        $failures.Add("jump retries/failures/timeouts=$jumpRetry/$jumpFailed/$jumpTimeout")
    }
    if ($jumpDetected -ne $jumpLanded) {
        $failures.Add("jump launches/landings=$jumpDetected/$jumpLanded")
    }
    if ($stallRate -gt $MaxRouteStallsPerBotMinute) {
        $failures.Add(("route stalls per bot-minute={0:N2} exceeds {1:N2}" -f
            $stallRate, $MaxRouteStallsPerBotMinute))
    }
    if ($loadedSlots.Count -lt $botCount) {
        $failures.Add("Bryar loadouts=$($loadedSlots.Count), bots=$botCount")
    }
    if ($models.Count -lt $botCount) {
        $failures.Add("distinct models=$($models.Count), bots=$botCount")
    }
    if ($distinctRangedWeapons -lt $MinDistinctRangedWeapons) {
        $failures.Add("active ranged weapons=$distinctRangedWeapons")
    }
    if ($forceActions -lt 1) { $failures.Add("no successful Force actions") }
    if ($pickupActions -lt 1) { $failures.Add("no successful pickup actions") }

    $passed = $failures.Count -eq 0
    if (!$passed) { $allPassed = $false }
    $status = if ($passed) { "PASS" } else { "FAIL" }
    $report = New-Object System.Collections.Generic.List[string]
    $report.Add("status=$status")
    $report.Add("runDir=$resolvedDir")
    $report.Add("elapsedMs=$elapsedMs")
    $report.Add("players=$playerCount")
    $report.Add("bots=$botCount")
    $report.Add("kills=$totalKills")
    $report.Add("suicides=$totalSuicides")
    $report.Add("jumpDetected=$jumpDetected")
    $report.Add("jumpLanded=$jumpLanded")
    $report.Add("routeStalls=$routeStalls")
    $report.Add(("routeStallsPerBotMinute={0:N3}" -f $stallRate))
    $report.Add("noLosFireAttempts=$noLosFireAttempts")
    $report.Add("BryarLoadouts=$($loadedSlots.Count)")
    $report.Add("distinctModels=$($models.Count)")
    $report.Add("distinctRangedWeapons=$distinctRangedWeapons")
    $report.Add("forceActions=$forceActions")
    $report.Add("pickupActions=$pickupActions")
    foreach ($failure in $failures) {
        $report.Add("failure=$failure")
    }
    $report | Set-Content -LiteralPath (Join-Path $resolvedDir "bot-quality.txt") -Encoding ASCII
    Write-Output "[$status] $resolvedDir"
    $report | Where-Object { $_ -match '^(kills|suicides|jumpDetected|jumpLanded|routeStallsPerBotMinute|noLosFireAttempts|distinctModels|distinctRangedWeapons|forceActions|pickupActions|failure)=' } |
        ForEach-Object { Write-Output "  $_" }
}

if (!$allPassed) {
    exit 1
}
