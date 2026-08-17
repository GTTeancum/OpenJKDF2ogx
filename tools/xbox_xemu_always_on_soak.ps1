param(
    [int]$DurationSeconds = 7200,
    [int]$PollIntervalSeconds = 30,
    [int]$ScreenshotEverySeconds = 180,
    [int]$MonitorPort = 4577,
    [int]$DefaultLevelSeconds = 600,
    [switch]$DisableMusic,
    [switch]$DisableCutscenes,
    [switch]$KeepIso,
    [string]$RuntimeSource = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$XemuRoot = "C:\Games\Emulators\Xemu",
    [string]$HddPath = "C:\Games\Emulators\Xemu\OpenJKDF2Soak\HDD\openjkdf2_soak_hdd.qcow2",
    [string]$WorkRoot = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildRoot = Join-Path $RepoRoot "build\xbox"
$SmokeScript = Join-Path $RepoRoot "tools\xbox_xemu_smoke.ps1"
$PlanDir = Join-Path $BuildRoot "xemu_always_on_soak"
$PlanPath = Join-Path $PlanDir "xbox_soak_always_on.txt"

function Require-Path([string]$Path, [string]$Label) {
    if (!(Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Ensure-DedicatedHdd {
    $hddDir = Split-Path -Parent $HddPath
    if (!(Test-Path -LiteralPath $hddDir)) {
        New-Item -ItemType Directory -Force -Path $hddDir | Out-Null
    }
    if (!(Test-Path -LiteralPath $HddPath)) {
        $source = Join-Path $XemuRoot "HDD\xbox_hdd.qcow2"
        Require-Path $source "Source XEMU HDD"
        Copy-Item -LiteralPath $source -Destination $HddPath -Force
    }
}

function New-AlwaysOnPlan {
    New-Item -ItemType Directory -Force -Path $PlanDir | Out-Null

    $level = [Math]::Max(60, $DefaultLevelSeconds)
    $short = [Math]::Max(60, [int]($level * 0.75))
    $runtimeEpisodeDir = Join-Path $RuntimeSource "Episode"

    $lines = @(
        "# OpenJKDF2 Xbox always-on XEMU soak",
        "# Format: LEVEL|seconds|localPlayers|episode|jkl",
        "# MENU entries force a level->menu->level cleanup path without restarting XEMU.",
        "MENU|0",
        "LEVEL|$level|1|JK1|01narshadda.jkl",
        "LEVEL|$level|1|JK1|06abarons.jkl",
        "LEVEL|$level|1|JK1|15maw.jkl",
        "MENU|0",
        "LEVEL|$level|1|JKM|s1l1_rebelbase.jkl",
        "LEVEL|$level|1|JKM|s2l1_palace.jkl",
        "LEVEL|$level|1|JKM|s5l4_lowersith.jkl",
        "MENU|0",
        "LEVEL|$short|1|JK1MP|m2.jkl",
        "LEVEL|$short|1|JK1MP|m5.jkl",
        "LEVEL|$short|4|JK1CTF|c1.jkl",
        "LEVEL|$short|4|JK1CTF|c3.jkl",
        "MENU|0",
        "LEVEL|$short|1|JKM_MP|mdm02_freezer.jkl",
        "LEVEL|$short|1|JKM_MP|mdm17_bespin.jkl",
        "LEVEL|$short|4|JKM_KFY|k1.jkl",
        "LEVEL|$short|4|JKM_SABER|msb1_home.jkl",
        "MENU|0",
        "LEVEL|$short|1|aphc|fire-control.jkl",
        "LEVEL|$short|1|DuelOfFates_SE|dueloffates_se.jkl",
        "LEVEL|$short|1|GEMPFAC|gempfac.jkl",
        "LEVEL|$short|1|q3dm5|q3dm5.jkl"
    )
    if (Test-Path -LiteralPath (Join-Path $runtimeEpisodeDir "impsiege.gob")) {
        $lines += "LEVEL|$short|1|impsiege|impsiege.jkl"
    }
    Set-Content -LiteralPath $PlanPath -Value $lines -Encoding ASCII
}

Require-Path $SmokeScript "XEMU smoke runner"
Require-Path (Join-Path $BuildRoot "release\default.xbe") "Release default.xbe"
Require-Path (Join-Path $BuildRoot "release\openjkdf2_xbox.exe.map") "Release map"
Require-Path $RuntimeSource "OpenJKDF2 runtime source"

Ensure-DedicatedHdd
New-AlwaysOnPlan

$args = @{
    DurationSeconds = $DurationSeconds
    PollIntervalSeconds = $PollIntervalSeconds
    ScreenshotEverySeconds = $ScreenshotEverySeconds
    MonitorPort = $MonitorPort
    RunLabel = "xemu-always-on-soak"
    AlwaysOnSoakPlanPath = $PlanPath
    RuntimeSource = $RuntimeSource
    XemuRoot = $XemuRoot
    HddPath = $HddPath
}

if (![string]::IsNullOrWhiteSpace($WorkRoot)) {
    $args.WorkRoot = $WorkRoot
}

if ($DisableMusic) {
    $args.DisableMusic = $true
}
if ($DisableCutscenes) {
    $args.DisableCutscenes = $true
}
if ($KeepIso) {
    $args.KeepIso = $true
}

Write-Host "Starting one-process XEMU always-on soak"
Write-Host "Plan: $PlanPath"
Write-Host "Duration: $DurationSeconds seconds"
Write-Host "HDD: $HddPath"

& $SmokeScript @args
exit $LASTEXITCODE
