param(
    [string]$GameRoot = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$OxaniumZip = "C:\Users\smmel\Downloads\Oxanium.zip",
    [string]$WorkDir = "build\sft_oxanium"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Tool = Join-Path $RepoRoot "scripts\assets\sft_tool.py"
$ResourceDir = Join-Path $GameRoot "Resource"
$Res1Hi = Join-Path $ResourceDir "Res1hi.gob"
$OutDir = Join-Path $ResourceDir "ui\sft"
$WorkRoot = Join-Path $RepoRoot $WorkDir
$FontDir = Join-Path $WorkRoot "fonts"
$ExtractDir = Join-Path $WorkRoot "extract"
$PreviewDir = Join-Path $WorkRoot "preview"

if (!(Test-Path -LiteralPath $Res1Hi)) {
    throw "Could not find $Res1Hi"
}
if (!(Test-Path -LiteralPath $OxaniumZip)) {
    throw "Could not find $OxaniumZip"
}

New-Item -ItemType Directory -Force -Path $FontDir, $ExtractDir, $PreviewDir, $OutDir | Out-Null
tar -xf $OxaniumZip -C $FontDir
python $Tool extract-gob $Res1Hi $ExtractDir --pattern "*.sft"

$TemplateDir = Join-Path $ExtractDir "ui\sft"
$Bold = Join-Path $FontDir "static\Oxanium-Bold.ttf"
$Semi = Join-Path $FontDir "static\Oxanium-SemiBold.ttf"

$Jobs = @(
    @("helthnum.sft", "HelthNum.sft", $Bold),
    @("helthnum16.sft", "HelthNum16.sft", $Bold),
    @("armornum.sft", "ArmorNum.sft", $Bold),
    @("armornum16.sft", "ArmorNum16.sft", $Bold),
    @("armornumssuper.sft", "ArmorNumsSuper.sft", $Bold),
    @("armornumssuper16.sft", "ArmorNumsSuper16.sft", $Bold),
    @("amonums.sft", "AmoNums.sft", $Bold),
    @("amonums16.sft", "AmoNums16.sft", $Bold),
    @("amonumssuper.sft", "AmoNumsSuper.sft", $Bold),
    @("amonumssuper16.sft", "AmoNumsSuper16.sft", $Bold),
    @("msgfont.sft", "msgFont.sft", $Semi),
    @("msgfont16.sft", "msgFont16.sft", $Semi)
)

foreach ($Job in $Jobs) {
    $Template = Join-Path $TemplateDir $Job[0]
    $Output = Join-Path $OutDir $Job[1]
    $Preview = Join-Path $PreviewDir ($Job[1] + ".png")
    python $Tool render-ttf $Template $Job[2] $Output --preview $Preview
    if ($LASTEXITCODE -ne 0) {
        throw "Font render failed for $($Job[1])"
    }
}

Get-ChildItem -LiteralPath $OutDir -Filter "*.sft" | Sort-Object Name | Select-Object Length, Name, FullName
