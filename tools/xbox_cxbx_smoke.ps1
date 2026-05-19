param(
    [int]$Seconds = 45,
    [string]$CxbxRoot = "C:\Games\Emulators\CXBX",
    [string]$AppDir = "C:\Games\Emulators\CXBX\openJKDF2x",
    [string]$BuildDir = "C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release"
)

$ErrorActionPreference = "Stop"

$xbeSrc = Join-Path $BuildDir "default.xbe"
$xbeDst = Join-Path $AppDir "default.xbe"
$emu = Join-Path $CxbxRoot "cxbx.exe"
$log = Join-Path $CxbxRoot "EmuDisk\Partition1\debug_openjkdf2.txt"

if (!(Test-Path -LiteralPath $xbeSrc)) { throw "Missing built XBE: $xbeSrc" }
if (!(Test-Path -LiteralPath $emu)) { throw "Missing CXBX-R executable: $emu" }
if (!(Test-Path -LiteralPath $AppDir)) { throw "Missing CXBX-R app directory: $AppDir" }

Copy-Item -LiteralPath $xbeSrc -Destination $xbeDst -Force
Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $emu -ArgumentList "`"$xbeDst`"" -WorkingDirectory $CxbxRoot -PassThru
try {
    Start-Sleep -Seconds $Seconds
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
        Wait-Process -Id $proc.Id -ErrorAction SilentlyContinue
    }
}

if (!(Test-Path -LiteralPath $log)) {
    Write-Output "SMOKE: no debug_openjkdf2.txt produced at $log"
    exit 2
}

$patterns = @(
    "sithWorld_Load: opened OK",
    "sithWorld_Load: section end",
    "stdMci:",
    "stdSound_XboxCreateBuffer:",
    "GameplayShow:",
    "GameplayTick:",
    "sithTick:",
    "TickAll:",
    "SECTION PARSE FAILED",
    "Memory alloc failure",
    "E_OUTOFMEMORY",
    "failed 0x"
)

Write-Output "SMOKE: log=$log"
Write-Output "SMOKE: last-write=$((Get-Item -LiteralPath $log).LastWriteTime) length=$((Get-Item -LiteralPath $log).Length)"
Select-String -Path $log -Pattern $patterns | Select-Object -Last 180
