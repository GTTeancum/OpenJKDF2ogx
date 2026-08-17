param(
    [int]$MenuSeconds = 300,
    [int]$LevelSeconds = 900,
    [int]$ModSeconds = 600,
    [int]$FmvLimitSeconds = 5,
    [switch]$DisableMusic,
    [switch]$ContinueOnFailure,
    [string]$CxbxRoot = "C:\Programming\GitHub\OpenJKDF2ogx\CXBXR",
    [string]$AppDir = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$BuildDir = "C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release",
    [string]$OutRoot = "C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\cxbx_soak_runs"
)

$ErrorActionPreference = "Stop"

$Repo = Split-Path -Parent $PSScriptRoot
$SmokeScript = Join-Path $Repo "tools\xbox_cxbx_smoke.ps1"
$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$RunRoot = Join-Path $OutRoot "$Stamp-long-soak"
$ManifestPath = Join-Path $RunRoot "manifest.txt"
$SummaryPath = Join-Path $RunRoot "summary.txt"
$PhaseOutRoot = Join-Path $RunRoot "phases"

function Require-Path([string]$Path, [string]$Name) {
    if (!(Test-Path -LiteralPath $Path)) {
        throw "$Name not found: $Path"
    }
}

function Add-Phase([System.Collections.Generic.List[object]]$List, [string]$Label, [string]$AutoStartLevel, [int]$Seconds, [string]$Kind) {
    foreach ($phase in $List) {
        if ($phase.AutoStartLevel -eq $AutoStartLevel -and $phase.Kind -eq $Kind) {
            return
        }
    }
    $List.Add([pscustomobject]@{
        Label = $Label
        AutoStartLevel = $AutoStartLevel
        Seconds = $Seconds
        Kind = $Kind
    })
}

function Read-GobPlayableJklEntries([string]$GobPath) {
    $script = @"
import pathlib, re, struct, sys
p = pathlib.Path(sys.argv[1])
with p.open('rb') as f:
    hdr = f.read(12)
    if len(hdr) < 12:
        sys.exit(0)
    magic, version, table = struct.unpack('<III', hdr)
    if magic != 0x20424f47 or version != 20:
        sys.exit(0)
    f.seek(table)
    count_data = f.read(4)
    if len(count_data) < 4:
        sys.exit(0)
    count = struct.unpack('<I', count_data)[0]
    entries = []
    for _ in range(count):
        entry = f.read(136)
        if len(entry) < 136:
            break
        off, size = struct.unpack('<II', entry[:8])
        name = entry[8:].split(b'\0', 1)[0].decode('latin1', errors='ignore')
        entries.append((name.replace('/', '\\'), off, size))
    for name, off, size in entries:
        if name.lower() == 'episode.jk':
            f.seek(off)
            text = f.read(size).decode('latin1', errors='ignore')
            for line in text.splitlines():
                stripped = line.strip()
                if not stripped or stripped.startswith('#') or ':' not in stripped:
                    continue
                parts = re.split(r'\s+', stripped)
                for idx, part in enumerate(parts):
                    if part.upper() == 'LEVEL' and idx + 1 < len(parts):
                        level = parts[idx + 1]
                        if level.lower().endswith('.jkl'):
                            print(level.replace('/', '\\'))
            sys.exit(0)
    for name, _off, _size in entries:
        if name.lower().endswith('.jkl'):
            print(name.replace('/', '\\'))
"@
    $output = & python -c $script $GobPath 2>$null
    if ($LASTEXITCODE -ne 0 -or $null -eq $output) {
        return @()
    }
    return @($output)
}

function Write-SoakLine([string]$Line) {
    Write-Output $Line
    Add-Content -LiteralPath $SummaryPath -Value $Line -Encoding ASCII
}

function Get-ModPhases {
    $phases = New-Object "System.Collections.Generic.List[object]"
    $episodeDir = Join-Path $AppDir "Episode"
    if (!(Test-Path -LiteralPath $episodeDir)) {
        return $phases
    }

    $stockArchives = @(
        "JK1.GOB",
        "JK1MP.GOB",
        "JK1CTF.GOB",
        "JKM.GOO",
        "JKM_MP.GOO",
        "JKM_KFY.GOO",
        "JKM_SABER.GOO"
    )

    Get-ChildItem -LiteralPath $episodeDir -File -Include *.gob,*.goo -ErrorAction SilentlyContinue |
        Where-Object { $stockArchives -notcontains $_.Name } |
        ForEach-Object {
            $episodeName = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
            $jkls = @(Read-GobPlayableJklEntries $_.FullName)
            foreach ($jkl in $jkls) {
                $levelName = Split-Path $jkl -Leaf
                Add-Phase $phases ("mod-{0}-{1}" -f $episodeName.ToLowerInvariant(), ([System.IO.Path]::GetFileNameWithoutExtension($levelName)).ToLowerInvariant()) "$episodeName|$levelName" $ModSeconds "mod"
            }
        }

    Get-ChildItem -LiteralPath $episodeDir -Directory -ErrorAction SilentlyContinue |
        Where-Object { $stockArchives -notcontains ($_.Name + ".GOB") -and $stockArchives -notcontains ($_.Name + ".GOO") } |
        ForEach-Object {
            $episodeName = $_.Name
            Get-ChildItem -LiteralPath (Join-Path $_.FullName "jkl") -File -Filter *.jkl -ErrorAction SilentlyContinue |
                ForEach-Object {
                    Add-Phase $phases ("mod-{0}-{1}" -f $episodeName.ToLowerInvariant(), ([System.IO.Path]::GetFileNameWithoutExtension($_.Name)).ToLowerInvariant()) "$episodeName|$($_.Name)" $ModSeconds "mod"
                }
        }

    return $phases
}

function Invoke-SoakPhase($Phase, [int]$Index, [int]$Total) {
    $stdout = Join-Path $RunRoot ("phase_{0:D2}_{1}_stdout.txt" -f $Index, $Phase.Label)
    $stderr = Join-Path $RunRoot ("phase_{0:D2}_{1}_stderr.txt" -f $Index, $Phase.Label)
    $args = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $SmokeScript,
        "-WatchdogSeconds", $Phase.Seconds,
        "-RunLabel", $Phase.Label,
        "-CxbxRoot", $CxbxRoot,
        "-AppDir", $AppDir,
        "-BuildDir", $BuildDir,
        "-OutRoot", $PhaseOutRoot,
        "-FmvLimitSeconds", $FmvLimitSeconds
    )

    if (![string]::IsNullOrWhiteSpace($Phase.AutoStartLevel)) {
        $args += @("-AutoStartLevel", $Phase.AutoStartLevel)
    }
    if ($DisableMusic) {
        $args += "-DisableMusic"
    }

    $start = Get-Date
    Write-SoakLine "[$($start.ToString('s'))] phase $Index/$Total start label=$($Phase.Label) kind=$($Phase.Kind) seconds=$($Phase.Seconds) autostart=$($Phase.AutoStartLevel)"

    $p = Start-Process -FilePath "powershell.exe" -ArgumentList $args -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $end = Get-Date

    Write-SoakLine "[$($end.ToString('s'))] phase $Index/$Total end label=$($Phase.Label) exit=$($p.ExitCode) elapsedSeconds=$([int]($end - $start).TotalSeconds) stdout=$stdout stderr=$stderr"
    if ($p.ExitCode -ne 0 -and !$ContinueOnFailure) {
        throw "Soak phase failed: $($Phase.Label) exit=$($p.ExitCode)"
    }
}

Require-Path $SmokeScript "CXBX-R smoke script"
Require-Path (Join-Path $CxbxRoot "cxbxr-ldr-project1.exe") "CXBX-R loader"
Require-Path (Join-Path $BuildDir "default.xbe") "Xbox release XBE"
Require-Path (Join-Path $AppDir "Episode\JK1.GOB") "JK single-player episode"
Require-Path (Join-Path $AppDir "Episode\JK1MP.GOB") "JK multiplayer episode"
Require-Path (Join-Path $AppDir "Episode\JK1CTF.GOB") "JK CTF episode"
Require-Path (Join-Path $AppDir "Episode\JKM.GOO") "MotS single-player episode"
Require-Path (Join-Path $AppDir "Episode\JKM_MP.GOO") "MotS multiplayer episode"

New-Item -ItemType Directory -Force -Path $RunRoot | Out-Null
New-Item -ItemType Directory -Force -Path $PhaseOutRoot | Out-Null

$phases = New-Object "System.Collections.Generic.List[object]"
Add-Phase $phases "menu-boot" "" $MenuSeconds "menu"

Add-Phase $phases "jk-sp-01narshadda" "JK1|01narshadda.jkl" $LevelSeconds "jk-sp"
Add-Phase $phases "jk-sp-06abarons" "JK1|06abarons.jkl" $LevelSeconds "jk-sp"
Add-Phase $phases "jk-sp-15maw" "JK1|15maw.jkl" $LevelSeconds "jk-sp"

Add-Phase $phases "mots-sp-s1l1-rebelbase" "JKM|s1l1_rebelbase.jkl" $LevelSeconds "mots-sp"
Add-Phase $phases "mots-sp-s2l1-palace" "JKM|s2l1_palace.jkl" $LevelSeconds "mots-sp"
Add-Phase $phases "mots-sp-s5l4-lowersith" "JKM|s5l4_lowersith.jkl" $LevelSeconds "mots-sp"

Add-Phase $phases "jk-mp-m2" "JK1MP|m2.jkl" $LevelSeconds "jk-mp"
Add-Phase $phases "jk-mp-m5" "JK1MP|m5.jkl" $LevelSeconds "jk-mp"
Add-Phase $phases "jk-ctf-c1" "JK1CTF|c1.jkl" $LevelSeconds "jk-mp"
Add-Phase $phases "jk-ctf-c3" "JK1CTF|c3.jkl" $LevelSeconds "jk-mp"

Add-Phase $phases "mots-mp-freezer" "JKM_MP|mdm02_freezer.jkl" $LevelSeconds "mots-mp"
Add-Phase $phases "mots-mp-bespin" "JKM_MP|mdm17_bespin.jkl" $LevelSeconds "mots-mp"
Add-Phase $phases "mots-kfy-k1" "JKM_KFY|k1.jkl" $LevelSeconds "mots-mp"
Add-Phase $phases "mots-saber-home" "JKM_SABER|msb1_home.jkl" $LevelSeconds "mots-mp"

Get-ModPhases | ForEach-Object { $phases.Add($_) }

$manifest = New-Object "System.Collections.Generic.List[string]"
$manifest.Add("runRoot=$RunRoot")
$manifest.Add("started=$((Get-Date).ToString('s'))")
$manifest.Add("cxbxRoot=$CxbxRoot")
$manifest.Add("appDir=$AppDir")
$manifest.Add("buildDir=$BuildDir")
$manifest.Add("phaseOutRoot=$PhaseOutRoot")
$manifest.Add("menuSeconds=$MenuSeconds")
$manifest.Add("levelSeconds=$LevelSeconds")
$manifest.Add("modSeconds=$ModSeconds")
$manifest.Add("fmvLimitSeconds=$FmvLimitSeconds")
$manifest.Add("disableMusic=$([bool]$DisableMusic)")
$manifest.Add("continueOnFailure=$([bool]$ContinueOnFailure)")
$manifest.Add("note=Plain CXBX-R soak cannot drive controller-only split-screen ready-up; MP episode autostarts exercise local host creation and MP level survival.")
$manifest.Add("")
$manifest.Add("phases:")
for ($i = 0; $i -lt $phases.Count; $i++) {
    $p = $phases[$i]
    $manifest.Add(("{0:D2} kind={1} seconds={2} label={3} autostart={4}" -f ($i + 1), $p.Kind, $p.Seconds, $p.Label, $p.AutoStartLevel))
}
$manifest | Set-Content -LiteralPath $ManifestPath -Encoding ASCII

"CXBX-R long soak started $(Get-Date -Format s)" | Set-Content -LiteralPath $SummaryPath -Encoding ASCII
"manifest=$ManifestPath" | Add-Content -LiteralPath $SummaryPath -Encoding ASCII

try {
    for ($i = 0; $i -lt $phases.Count; $i++) {
        Invoke-SoakPhase $phases[$i] ($i + 1) $phases.Count
    }
    Write-SoakLine "[$((Get-Date).ToString('s'))] soak complete phases=$($phases.Count)"
    exit 0
}
catch {
    Write-SoakLine "[$((Get-Date).ToString('s'))] soak failed: $($_.Exception.Message)"
    exit 1
}
