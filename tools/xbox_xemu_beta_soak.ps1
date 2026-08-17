param(
    [int]$MenuSeconds = 300,
    [int]$MapSeconds = 600,
    [int]$SplitSeconds = 1800,
    [int]$MonitorPort = 4577,
    [int]$PollIntervalSeconds = 30,
    [int]$ScreenshotEverySeconds = 180,
    [int]$FmvLimitSeconds = 3,
    [switch]$AllowCutscenes,
    [string]$RuntimeSource = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$XemuRoot = "C:\Games\Emulators\Xemu",
    [string]$HddPath = "C:\Games\Emulators\Xemu\HDD\xbox_hdd.qcow2",
    [string]$HostHdd = "C:\Games\Emulators\Xemu\UT99Test\HDD\ut99_hdd.qcow2",
    [string]$ClientHdd = "C:\Games\Emulators\Xemu\UT99Fresh\HDD\ut99_hdd.qcow2"
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$SoakScript = Join-Path $RepoRoot "tools\xbox_xemu_soak.ps1"
$MinimumSeconds = 7200

function Require-Path([string]$Path, [string]$Label) {
    if (!(Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Add-Map([System.Collections.Generic.List[string]]$Maps, [string]$EpisodeArchive, [string]$LevelName, [string]$RequiredPath) {
    if (![string]::IsNullOrWhiteSpace($RequiredPath) -and !(Test-Path -LiteralPath $RequiredPath)) {
        return
    }
    $spec = "$EpisodeArchive|$LevelName"
    if (!$Maps.Contains($spec)) {
        $Maps.Add($spec)
    }
}

Require-Path $SoakScript "XEMU soak script"
Require-Path $RuntimeSource "OpenJKDF2 runtime source"
Require-Path (Join-Path $RuntimeSource "Episode\JK1.GOB") "JK SP episode"
Require-Path (Join-Path $RuntimeSource "Episode\JK1MP.GOB") "JK MP episode"
Require-Path (Join-Path $RuntimeSource "Episode\JK1CTF.GOB") "JK CTF episode"
Require-Path (Join-Path $RuntimeSource "Episode\JKM.GOO") "MotS SP episode"
Require-Path (Join-Path $RuntimeSource "Episode\JKM_MP.GOO") "MotS MP episode"
Require-Path (Join-Path $RuntimeSource "Episode\JKM_KFY.GOO") "MotS KFY episode"
Require-Path (Join-Path $RuntimeSource "Episode\JKM_SABER.GOO") "MotS Saber episode"

$maps = New-Object "System.Collections.Generic.List[string]"
$episodeDir = Join-Path $RuntimeSource "Episode"

Add-Map $maps "JK1" "01narshadda.jkl" (Join-Path $episodeDir "JK1.GOB")
Add-Map $maps "JK1" "06abarons.jkl" (Join-Path $episodeDir "JK1.GOB")
Add-Map $maps "JK1" "15maw.jkl" (Join-Path $episodeDir "JK1.GOB")
Add-Map $maps "JKM" "s1l1_rebelbase.jkl" (Join-Path $episodeDir "JKM.GOO")
Add-Map $maps "JKM" "s2l1_palace.jkl" (Join-Path $episodeDir "JKM.GOO")
Add-Map $maps "JKM" "s5l4_lowersith.jkl" (Join-Path $episodeDir "JKM.GOO")

Add-Map $maps "JK1MP" "m2.jkl" (Join-Path $episodeDir "JK1MP.GOB")
Add-Map $maps "JK1MP" "m5.jkl" (Join-Path $episodeDir "JK1MP.GOB")
Add-Map $maps "JK1CTF" "c1.jkl" (Join-Path $episodeDir "JK1CTF.GOB")
Add-Map $maps "JK1CTF" "c3.jkl" (Join-Path $episodeDir "JK1CTF.GOB")
Add-Map $maps "JKM_MP" "mdm02_freezer.jkl" (Join-Path $episodeDir "JKM_MP.GOO")
Add-Map $maps "JKM_MP" "mdm17_bespin.jkl" (Join-Path $episodeDir "JKM_MP.GOO")
Add-Map $maps "JKM_KFY" "k1.jkl" (Join-Path $episodeDir "JKM_KFY.GOO")
Add-Map $maps "JKM_SABER" "msb1_home.jkl" (Join-Path $episodeDir "JKM_SABER.GOO")

Add-Map $maps "aphc" "fire-control.jkl" (Join-Path $episodeDir "aphc.gob")
Add-Map $maps "DuelOfFates_SE" "dueloffates_se.jkl" (Join-Path $episodeDir "DuelOfFates_SE.gob")
Add-Map $maps "GEMPFAC" "gempfac.jkl" (Join-Path $episodeDir "GEMPFAC.gob")
Add-Map $maps "q3dm5" "q3dm5.jkl" (Join-Path $episodeDir "q3dm5.gob")

$estimatedSeconds = $MenuSeconds + ($maps.Count * $MapSeconds) + $SplitSeconds
if ($estimatedSeconds -lt $MinimumSeconds) {
    throw "Refusing to start a short soak: estimated $estimatedSeconds seconds is below two-hour floor $MinimumSeconds."
}

Write-Host "Starting XEMU beta soak"
Write-Host "maps=$($maps.Count)"
Write-Host "estimatedSeconds=$estimatedSeconds"
Write-Host "estimatedHours=$([math]::Round($estimatedSeconds / 3600.0, 2))"

& $SoakScript `
    -Run `
    -MenuSeconds $MenuSeconds `
    -MapSeconds $MapSeconds `
    -SplitSeconds $SplitSeconds `
    -MonitorPort $MonitorPort `
    -PollIntervalSeconds $PollIntervalSeconds `
    -ScreenshotEverySeconds $ScreenshotEverySeconds `
    -FmvLimitSeconds $FmvLimitSeconds `
    -DisableCutscenes:(!$AllowCutscenes) `
    -SinglePlayerMaps $maps.ToArray() `
    -RuntimeSource $RuntimeSource `
    -XemuRoot $XemuRoot `
    -HddPath $HddPath `
    -HostHdd $HostHdd `
    -ClientHdd $ClientHdd

exit $LASTEXITCODE
