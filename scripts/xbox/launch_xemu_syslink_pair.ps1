param(
    [switch]$SetupOnly,
    [switch]$Stop,
    [switch]$CleanArtifacts,
    [switch]$RebuildIso,
    [switch]$Smoke,
    [switch]$RealLobby,
    [switch]$FourPlayerStress,
    [switch]$UdpBackend,
    [switch]$McastBackend,
    [string]$PcapInterfaceName = '',
    [string]$InstanceRoot = '',
    [switch]$PreserveSourceIso,
    [int]$LaunchDelayMs = 1200,
    [switch]$ClientFirst,
    [string]$HostHdd = 'C:\Games\Emulators\Xemu\UT99Test\HDD\ut99_hdd.qcow2',
    [string]$ClientHdd = 'C:\Games\Emulators\Xemu\UT99Fresh\HDD\ut99_hdd.qcow2'
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$XemuRoot = 'C:\Games\Emulators\Xemu'
if( [string]::IsNullOrWhiteSpace($InstanceRoot) )
{
    $InstanceRoot = $XemuRoot
}
$InstanceRoot = [System.IO.Path]::GetFullPath($InstanceRoot)
$RuntimeSource = 'C:\Games\Emulators\CXBX\openJKDF2x'
$BuildRoot = Join-Path $RepoRoot 'build\xbox'
$ReleaseRoot = Join-Path $BuildRoot 'release'
$IsoPath = Join-Path $BuildRoot 'openjkdf2_xemu_current.iso'
$StagePath = Join-Path $BuildRoot 'xemu_syslink_stage_current'
$NewIsoPath = Join-Path $BuildRoot 'openjkdf2_xemu_current.new.iso'
$HostRoleIsoPath = Join-Path $BuildRoot 'openjkdf2_xemu_current_host_role.iso'
$ClientRoleIsoPath = Join-Path $BuildRoot 'openjkdf2_xemu_current_client_role.iso'
$HostRoleStagePath = Join-Path $BuildRoot 'xemu_syslink_stage_host_role'
$ClientRoleStagePath = Join-Path $BuildRoot 'xemu_syslink_stage_client_role'
$ScreenshotRoot = Join-Path $BuildRoot 'xemu_syslink_screenshots'
$SmokeMarkerMode = $Smoke.IsPresent -or $RealLobby.IsPresent
$SmokeRoleHarnessMode = $Smoke.IsPresent -and !$RealLobby.IsPresent

$XemuExeSource = Join-Path $XemuRoot 'xemu.exe'
$BootRom = Join-Path $XemuRoot 'MCPX\mcpx_1.0.bin'
$FlashRom = Join-Path $XemuRoot 'BIOS\xbox-4627_debug.bin'
$HostDir = Join-Path $InstanceRoot 'OpenJKDF2SyslinkHost'
$ClientDir = Join-Path $InstanceRoot 'OpenJKDF2SyslinkClient'
$EepromSource = Join-Path $XemuRoot 'EEPROM\eeprom.bin'
$XisoToolCandidates = @(
    (Join-Path $BuildRoot 'tools\extract-xiso\artifacts\extract-xiso.exe'),
    'C:\Programming\GitHub\Guitar Hero II\tools\artifacts\extract-xiso.exe',
    (Join-Path $RepoRoot '..\Guitar Hero II\tools\artifacts\extract-xiso.exe')
)

function Require-Path([string]$Path, [string]$Label)
{
    if( !(Test-Path $Path) )
    {
        throw "$Label not found: $Path"
    }
}

function Get-SafeChildPath([string]$ParentDir, [string]$LeafName)
{
    $parentFull = [System.IO.Path]::GetFullPath($ParentDir).TrimEnd('\')
    $childFull = [System.IO.Path]::GetFullPath((Join-Path $parentFull $LeafName))
    if( !$childFull.StartsWith($parentFull + '\', [System.StringComparison]::OrdinalIgnoreCase) )
    {
        throw "Refusing path outside $parentFull`: $childFull"
    }
    return $childFull
}

function Remove-KnownArtifacts
{
    foreach( $instance in @(
        @{ Dir = $HostDir; Leaf = 'openjkdf2_xemu_current_host.iso' },
        @{ Dir = $ClientDir; Leaf = 'openjkdf2_xemu_current_client.iso' }
    ))
    {
        $path = Get-SafeChildPath $instance.Dir $instance.Leaf
        if( Test-Path $path )
        {
            Remove-Item -LiteralPath $path -Force
        }
    }

    if( Test-Path $NewIsoPath )
    {
        Remove-Item -LiteralPath $NewIsoPath -Force
    }
    foreach( $path in @($HostRoleIsoPath, $ClientRoleIsoPath) )
    {
        if( Test-Path $path )
        {
            Remove-Item -LiteralPath $path -Force
        }
    }

    foreach( $stage in @($StagePath, $HostRoleStagePath, $ClientRoleStagePath) )
    {
        if( Test-Path $stage )
        {
            $buildFull = [System.IO.Path]::GetFullPath($BuildRoot).TrimEnd('\')
            $stageFull = [System.IO.Path]::GetFullPath($stage)
            if( !$stageFull.StartsWith($buildFull + '\', [System.StringComparison]::OrdinalIgnoreCase) )
            {
                throw "Refusing to remove staging path outside build root: $stageFull"
            }
            Remove-Item -LiteralPath $stage -Recurse -Force
        }
    }

    if( !$PreserveSourceIso -and (Test-Path $IsoPath) )
    {
        Remove-Item -LiteralPath $IsoPath -Force
    }
}

function Get-XemuProcessForConfig([string]$ConfigPath)
{
    $escaped = [System.IO.Path]::GetFullPath($ConfigPath)
    Get-CimInstance Win32_Process -Filter "Name = 'xemu.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($escaped, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 }
}

function Stop-XemuForConfig([string]$ConfigPath)
{
    $procs = @(Get-XemuProcessForConfig $ConfigPath)
    foreach( $proc in $procs )
    {
        Stop-Process -Id $proc.ProcessId -Force -ErrorAction SilentlyContinue
    }
}

function Read-MonitorAvailable([System.Net.Sockets.NetworkStream]$Stream)
{
    $bytes = New-Object byte[] 4096
    $text = ''
    while( $Stream.DataAvailable )
    {
        $read = $Stream.Read($bytes, 0, $bytes.Length)
        if( $read -le 0 )
        {
            break
        }
        $text += [System.Text.Encoding]::ASCII.GetString($bytes, 0, $read)
    }
    return $text
}

function Invoke-HmpCommands([int]$Port, [string[]]$Commands)
{
    $client = $null
    for( $attempt=0; $attempt -lt 40; $attempt++ )
    {
        try
        {
            $candidate = New-Object System.Net.Sockets.TcpClient
            $async = $candidate.BeginConnect('127.0.0.1', $Port, $null, $null)
            if( $async.AsyncWaitHandle.WaitOne(250) )
            {
                $candidate.EndConnect($async)
                $client = $candidate
                break
            }
            $candidate.Close()
        }
        catch
        {
            if( $candidate )
            {
                $candidate.Close()
            }
        }
        Start-Sleep -Milliseconds 250
    }

    if( !$client -or !$client.Connected )
    {
        throw "Could not connect to xemu monitor on port $Port"
    }

    $stream = $client.GetStream()
    $stream.ReadTimeout = 250
    Start-Sleep -Milliseconds 100
    $output = Read-MonitorAvailable $stream

    $writer = New-Object System.IO.StreamWriter($stream, [System.Text.Encoding]::ASCII)
    $writer.NewLine = "`n"
    $writer.AutoFlush = $true

    foreach( $command in $Commands )
    {
        $writer.WriteLine($command)
        Start-Sleep -Milliseconds 700
        $output += Read-MonitorAvailable $stream
    }

    $client.Close()
    return $output
}

function Get-PcapAdapter()
{
    $adapter = Get-NetAdapter -ErrorAction Stop |
        Where-Object { $_.Status -eq 'Up' -and $_.InterfaceGuid } |
        Sort-Object { if( $_.Name -eq 'Ethernet' ) { 0 } else { 1 } }, InterfaceMetric |
        Select-Object -First 1

    if( !$adapter )
    {
        throw "No active network adapter with an InterfaceGuid was found for XEMU pcap System Link."
    }

    return $adapter
}

function Get-PcapInterfaceName()
{
    $adapter = Get-PcapAdapter
    return "\Device\NPF_$($adapter.InterfaceGuid)"
}

function Find-XisoTool()
{
    foreach( $candidate in $XisoToolCandidates )
    {
        $expanded = [System.IO.Path]::GetFullPath($candidate)
        if( Test-Path $expanded )
        {
            return $expanded
        }
    }
    throw "extract-xiso.exe not found. Checked: $($XisoToolCandidates -join ', ')"
}

function Invoke-XisoCreate([string]$SourceDir, [string]$DestinationIso)
{
    $xisoTool = Find-XisoTool
    & $xisoTool -Q -m -c $SourceDir $DestinationIso
    if( $LASTEXITCODE -ne 0 )
    {
        throw "extract-xiso failed with exit code $LASTEXITCODE"
    }
}

function New-SmokeRoleIso([string]$BaseStage, [string]$RoleStage, [string]$DestinationIso, [string]$RoleMarker)
{
    if( Test-Path $RoleStage )
    {
        Remove-Item -LiteralPath $RoleStage -Recurse -Force
    }
    robocopy $BaseStage $RoleStage /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if( $LASTEXITCODE -gt 7 )
    {
        throw "robocopy smoke role staging failed with exit code $LASTEXITCODE"
    }

    Set-Content -LiteralPath (Join-Path $RoleStage $RoleMarker) -Value '' -Encoding ASCII
    Invoke-XisoCreate $RoleStage $DestinationIso
    Remove-Item -LiteralPath $RoleStage -Recurse -Force
}

function Rebuild-SourceIso()
{
    Require-Path $RuntimeSource 'OpenJKDF2 runtime source'
    Require-Path (Join-Path $RuntimeSource 'Episode\JK1MP.GOB') 'OpenJKDF2 JK1MP episode'
    Require-Path (Join-Path $RuntimeSource 'Resource\Res1hi.gob') 'OpenJKDF2 Res1hi.gob'
    Require-Path (Join-Path $RuntimeSource 'Resource\Res2.gob') 'OpenJKDF2 Res2.gob'
    Require-Path (Join-Path $ReleaseRoot 'default.xbe') 'Release default.xbe'

    Remove-KnownArtifacts
    New-Item -ItemType Directory -Force -Path $StagePath | Out-Null
    robocopy $RuntimeSource $StagePath /E /NFL /NDL /NJH /NJS /NP /XD Logs Screenshots | Out-Null
    if( $LASTEXITCODE -gt 7 )
    {
        throw "robocopy runtime staging failed with exit code $LASTEXITCODE"
    }

    Copy-Item -LiteralPath (Join-Path $ReleaseRoot 'default.xbe') -Destination (Join-Path $StagePath 'default.xbe') -Force
    Ensure-SmokePlayerProfile $StagePath

    $smokePath = Join-Path $StagePath 'XboxSystemLinkSmoke.ini'
    $fourPlayerStressPath = Join-Path $StagePath 'XboxSystemLink4PStress.ini'
    $fourPlayerStressShortPath = Join-Path $StagePath 'XSL4P.TXT'
    if( $SmokeMarkerMode )
    {
        Set-Content -LiteralPath $smokePath -Value '' -Encoding ASCII
    }
    elseif( Test-Path $smokePath )
    {
        Remove-Item -LiteralPath $smokePath -Force
    }
    if( $FourPlayerStress )
    {
        Set-Content -LiteralPath $fourPlayerStressPath -Value '' -Encoding ASCII
        Set-Content -LiteralPath $fourPlayerStressShortPath -Value '' -Encoding ASCII
    }
    else
    {
        if( Test-Path $fourPlayerStressPath )
        {
            Remove-Item -LiteralPath $fourPlayerStressPath -Force
        }
        if( Test-Path $fourPlayerStressShortPath )
        {
            Remove-Item -LiteralPath $fourPlayerStressShortPath -Force
        }
    }

    Invoke-XisoCreate $StagePath $NewIsoPath
    Move-Item -LiteralPath $NewIsoPath -Destination $IsoPath -Force

    if( $SmokeRoleHarnessMode )
    {
        New-SmokeRoleIso $StagePath $HostRoleStagePath $HostRoleIsoPath 'XboxSystemLinkSmokeHost.ini'
        New-SmokeRoleIso $StagePath $ClientRoleStagePath $ClientRoleIsoPath 'XboxSystemLinkSmokeClient.ini'
    }

    Remove-Item -LiteralPath $StagePath -Recurse -Force
    Write-Host "Rebuilt source XISO: $IsoPath"
    if( $SmokeRoleHarnessMode )
    {
        Write-Host "Rebuilt smoke role XISOs: $HostRoleIsoPath / $ClientRoleIsoPath"
    }
    elseif( $RealLobby )
    {
        Write-Host "Rebuilt real-lobby smoke XISO without forced host/client role markers."
    }
}

function Ensure-SmokePlayerProfile([string]$StageRoot)
{
    $stagePlayerDir = Join-Path $StageRoot 'player\Xbox'
    $runtimePlayerDir = Join-Path $RuntimeSource 'player\Xbox'
    $repoPlayerDir = Join-Path $RepoRoot 'player\Xbox'
    $plrPath = Join-Path $stagePlayerDir 'Xbox.plr'

    New-Item -ItemType Directory -Force -Path $stagePlayerDir | Out-Null

    if( Test-Path $runtimePlayerDir )
    {
        Copy-Item -Path (Join-Path $runtimePlayerDir '*') -Destination $stagePlayerDir -Force -ErrorAction SilentlyContinue
    }
    if( Test-Path $repoPlayerDir )
    {
        Copy-Item -Path (Join-Path $repoPlayerDir '*.mpc') -Destination $stagePlayerDir -Force -ErrorAction SilentlyContinue
    }

    if( !(Test-Path $plrPath) )
    {
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

    if( !(Get-ChildItem -LiteralPath $stagePlayerDir -Filter '*.mpc' -File -ErrorAction SilentlyContinue | Select-Object -First 1) )
    {
        $mpcPath = Join-Path $stagePlayerDir 'Katarn0.mpc'
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

    Write-Host "Smoke player profile: $stagePlayerDir"
}

function Enable-PcapNetwork([int]$MonitorPort, [string]$PcapIfName)
{
    $commands = @(
        "netdev_add pcap,id=xemu-netdev,ifname=$PcapIfName",
        'netdev_add hubport,id=xemu-netdev-hubport,hubid=0,netdev=xemu-netdev',
        'set_link nvnet.0 on',
        'info network'
    )
    return Invoke-HmpCommands $MonitorPort $commands
}

function Enable-McastNetwork([int]$MonitorPort, [string]$McastEndpoint)
{
    $commands = @(
        "netdev_add socket,id=xemu-netdev,mcast=$McastEndpoint",
        'netdev_add hubport,id=xemu-netdev-hubport,hubid=0,netdev=xemu-netdev',
        'set_link nvnet.0 on',
        'info network'
    )
    return Invoke-HmpCommands $MonitorPort $commands
}

function Get-DgramNetArgs([int]$LocalPort, [int]$RemotePort)
{
    return @(
        '-netdev',
        "dgram,id=xemu-netdev,local.type=inet,local.host=127.0.0.1,local.port=$LocalPort,remote.type=inet,remote.host=127.0.0.1,remote.port=$RemotePort",
        '-netdev',
        'hubport,id=xemu-netdev-hubport,hubid=0,netdev=xemu-netdev'
    )
}

function Get-NetworkInfo([int]$MonitorPort)
{
    return Invoke-HmpCommands $MonitorPort @('info network')
}

function Write-XemuConfig(
    [string]$InstanceDir,
    [string]$Name,
    [string]$HddPath,
    [string]$EepromPath,
    [string]$DvdPath,
    [string]$BindAddr,
    [string]$RemoteAddr,
    [bool]$UseUdpBackend
)
{
    $ScreenshotDir = Join-Path $ScreenshotRoot $Name
    New-Item -ItemType Directory -Force -Path $ScreenshotDir | Out-Null

    $ConfigPath = Join-Path $InstanceDir 'xemu.toml'
    $NetEnable = if( $UseUdpBackend ) { 'true' } else { 'false' }
    $content = @"
[general]
show_welcome = false
screenshot_dir = '$ScreenshotDir'
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
enable = $NetEnable
backend = 'udp'

[net.udp]
bind_addr = '$BindAddr'
remote_addr = '$RemoteAddr'

[sys.files]
bootrom_path = '$BootRom'
flashrom_path = '$FlashRom'
eeprom_path = '$EepromPath'
hdd_path = '$HddPath'
dvd_path = '$DvdPath'
"@
    Set-Content -LiteralPath $ConfigPath -Value $content -Encoding ASCII
    return $ConfigPath
}

function Ensure-InstanceIso([string]$InstanceDir, [string]$LeafName, [string]$SourceIso)
{
    $dest = Get-SafeChildPath $InstanceDir $LeafName
    if( Test-Path $dest )
    {
        Remove-Item -LiteralPath $dest -Force
    }
    Copy-Item -LiteralPath $SourceIso -Destination $dest -Force
    return $dest
}

function Remove-InstanceIso([string]$InstanceDir, [string]$LeafName)
{
    $dest = Get-SafeChildPath $InstanceDir $LeafName
    if( Test-Path $dest )
    {
        Remove-Item -LiteralPath $dest -Force
    }
}

function Get-XConfigChecksum([byte[]]$Data, [int]$Offset, [int]$Count)
{
    $mask = [uint64]4294967295
    $eax = [uint64]0
    $ebx = [uint64]0
    for( $i=0; $i -lt $Count; $i += 4 )
    {
        $word = [uint64][BitConverter]::ToUInt32($Data, $Offset + $i)
        $sum = $eax + $word
        if( $sum -gt $mask )
        {
            $ebx++
        }
        $eax = $sum -band $mask
    }
    $sum = $eax + $ebx
    if( $sum -gt $mask )
    {
        $sum = ($sum -band $mask) + 1
    }
    return [uint32]($sum -band $mask)
}

function Set-UInt32LE([byte[]]$Data, [int]$Offset, [uint32]$Value)
{
    $bytes = [BitConverter]::GetBytes($Value)
    [Array]::Copy($bytes, 0, $Data, $Offset, 4)
}

function Set-ClientEepromMac([string]$EepromPath)
{
    $bytes = [System.IO.File]::ReadAllBytes($EepromPath)
    if( $bytes.Length -ne 256 )
    {
        throw "Unexpected EEPROM size for $EepromPath"
    }

    $factoryOffset = 0x30
    $factorySize = 48
    $macOffset = $factoryOffset + 16

    $bytes[$macOffset + 5] = ($bytes[$macOffset + 5] + 1) -band 0xFF
    if( $bytes[$macOffset + 5] -eq $bytes[$macOffset + 4] )
    {
        $bytes[$macOffset + 5] = ($bytes[$macOffset + 5] + 1) -band 0xFF
    }

    Set-UInt32LE $bytes $factoryOffset ([uint32]0)
    $checksum = Get-XConfigChecksum $bytes $factoryOffset $factorySize
    Set-UInt32LE $bytes $factoryOffset ([uint32]([uint64]4294967295 - [uint64]$checksum))

    $verify = Get-XConfigChecksum $bytes $factoryOffset $factorySize
    if( $verify -ne [uint32]4294967295 )
    {
        throw ("Client EEPROM factory checksum failed: 0x{0:X8}" -f $verify)
    }

    [System.IO.File]::WriteAllBytes($EepromPath, $bytes)
}

function Initialize-Instance(
    [string]$InstanceDir,
    [string]$EepromName,
    [switch]$ClientMac
)
{
    New-Item -ItemType Directory -Force -Path $InstanceDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $InstanceDir 'EEPROM') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $InstanceDir 'shaders') | Out-Null

    $eepromPath = Join-Path $InstanceDir "EEPROM\$EepromName"
    Copy-Item -LiteralPath $EepromSource -Destination $eepromPath -Force

    if( $ClientMac )
    {
        Set-ClientEepromMac $eepromPath
    }
    return $eepromPath
}

function Ensure-InstanceExe([string]$InstanceDir)
{
    $dest = Join-Path $InstanceDir 'xemu.exe'
    if( Test-Path $dest )
    {
        return $dest
    }

    try
    {
        New-Item -ItemType HardLink -Path $dest -Target $XemuExeSource | Out-Null
    }
    catch
    {
        Copy-Item -LiteralPath $XemuExeSource -Destination $dest -Force
    }
    return $dest
}

$ExpectedHostConfig = Join-Path $HostDir 'xemu.toml'
$ExpectedClientConfig = Join-Path $ClientDir 'xemu.toml'

if( $Stop -or $CleanArtifacts )
{
    Stop-XemuForConfig $ExpectedHostConfig
    Stop-XemuForConfig $ExpectedClientConfig
    Start-Sleep -Milliseconds 500
    Remove-KnownArtifacts
    Write-Host "Stopped OpenJKDF2 System Link xemu pair and removed generated ISO/stage artifacts."
    return
}

Require-Path $XemuExeSource 'xemu.exe'
Require-Path $BootRom 'MCPX boot ROM'
Require-Path $FlashRom 'Xbox BIOS'
Require-Path $HostHdd 'Host HDD image'
Require-Path $ClientHdd 'Client HDD image'
Require-Path $EepromSource 'EEPROM source'
if( !$RebuildIso )
{
    Require-Path $IsoPath 'OpenJKDF2 XISO'
}

if( !$SetupOnly )
{
    Stop-XemuForConfig $ExpectedHostConfig
    Stop-XemuForConfig $ExpectedClientConfig
}

if( $UdpBackend -and $McastBackend )
{
    throw "Choose only one local XEMU network mode: -UdpBackend or -McastBackend."
}

if( $RebuildIso )
{
    Rebuild-SourceIso
}

$HostEeprom = Initialize-Instance $HostDir 'eeprom_host.bin'
$ClientEeprom = Initialize-Instance $ClientDir 'eeprom_client.bin' -ClientMac
$HostExe = Ensure-InstanceExe $HostDir
$ClientExe = Ensure-InstanceExe $ClientDir
$HostIsoSource = $IsoPath
$ClientIsoSource = $IsoPath
if( $SmokeRoleHarnessMode )
{
    if( (Test-Path $HostRoleIsoPath) -and (Test-Path $ClientRoleIsoPath) )
    {
        $HostIsoSource = $HostRoleIsoPath
        $ClientIsoSource = $ClientRoleIsoPath
    }
    elseif( $RebuildIso )
    {
        throw "Smoke role XISOs were not created."
    }
    else
    {
        Write-Warning "Smoke role XISOs are missing; rebuild with -RebuildIso for deterministic host/client harness roles."
    }
}
$HostIso = Ensure-InstanceIso $HostDir 'openjkdf2_xemu_current_host.iso' $HostIsoSource
$ClientIso = Ensure-InstanceIso $ClientDir 'openjkdf2_xemu_current_client.iso' $ClientIsoSource

$HostConfig = Write-XemuConfig $HostDir 'host' $HostHdd $HostEeprom $HostIso '127.0.0.1:9460' '127.0.0.1:9461' $UdpBackend.IsPresent
$ClientConfig = Write-XemuConfig $ClientDir 'client' $ClientHdd $ClientEeprom $ClientIso '127.0.0.1:9461' '127.0.0.1:9460' $UdpBackend.IsPresent

$HostHash = (Get-FileHash -Algorithm SHA1 $HostEeprom).Hash
$ClientHash = (Get-FileHash -Algorithm SHA1 $ClientEeprom).Hash
if( $HostHash -eq $ClientHash )
{
    throw "Host and client EEPROMs are identical; System Link needs unique MAC identities."
}

Write-Host "Host config:   $HostConfig"
Write-Host "Client config: $ClientConfig"
Write-Host "Host xemu:     $HostExe"
Write-Host "Client xemu:   $ClientExe"
Write-Host "Host HDD:      $HostHdd"
Write-Host "Client HDD:    $ClientHdd"
Write-Host "Source XISO:   $IsoPath"
Write-Host "Host XISO:     $HostIso"
Write-Host "Client XISO:   $ClientIso"
if( $SmokeRoleHarnessMode )
{
    Write-Host "Host source:   $HostIsoSource"
    Write-Host "Client source: $ClientIsoSource"
    Write-Host "Smoke mode:    deterministic role harness"
}
elseif( $RealLobby )
{
    Write-Host "Smoke mode:    real lobby discovery"
}
Write-Host "EEPROM SHA1:   host=$HostHash client=$ClientHash"
if( $UdpBackend )
{
    Write-Host "Network mode:  xemu udp backend"
}
elseif( $McastBackend )
{
    Write-Host "Network mode:  qemu multicast socket backend"
}
else
{
    $PcapIfName = if( [string]::IsNullOrWhiteSpace($PcapInterfaceName) ) { Get-PcapInterfaceName } else { $PcapInterfaceName }
    Write-Host "Pcap iface:    $PcapIfName"
}

if( $SetupOnly )
{
    return
}

Stop-XemuForConfig $HostConfig
Stop-XemuForConfig $ClientConfig

$hostArgs = @('-config_path', $HostConfig, '-monitor', 'tcp:127.0.0.1:4488,server,nowait')
$clientArgs = @('-config_path', $ClientConfig, '-monitor', 'tcp:127.0.0.1:4489,server,nowait')

if( $LaunchDelayMs -lt 0 )
{
    throw "LaunchDelayMs must be zero or greater."
}

if( $ClientFirst )
{
    $clientProc = Start-Process -FilePath $ClientExe -ArgumentList $clientArgs -WorkingDirectory $ClientDir -WindowStyle Hidden -PassThru
    if( $LaunchDelayMs -gt 0 )
    {
        Start-Sleep -Milliseconds $LaunchDelayMs
    }
    $hostProc = Start-Process -FilePath $HostExe -ArgumentList $hostArgs -WorkingDirectory $HostDir -WindowStyle Hidden -PassThru
}
else
{
    $hostProc = Start-Process -FilePath $HostExe -ArgumentList $hostArgs -WorkingDirectory $HostDir -WindowStyle Hidden -PassThru
    if( $LaunchDelayMs -gt 0 )
    {
        Start-Sleep -Milliseconds $LaunchDelayMs
    }
    $clientProc = Start-Process -FilePath $ClientExe -ArgumentList $clientArgs -WorkingDirectory $ClientDir -WindowStyle Hidden -PassThru
}
Start-Sleep -Seconds 2

if( $UdpBackend )
{
    $hostNetworkOk = $true
    $clientNetworkOk = $true
}
elseif( $McastBackend )
{
    $McastEndpoint = '230.0.0.1:9462'
    $hostNetwork = Enable-McastNetwork 4488 $McastEndpoint
    $clientNetwork = Enable-McastNetwork 4489 $McastEndpoint
    $hostNetworkOk = $hostNetwork.IndexOf('type=socket', [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -and $hostNetwork.IndexOf('mcast', [System.StringComparison]::OrdinalIgnoreCase) -ge 0
    $clientNetworkOk = $clientNetwork.IndexOf('type=socket', [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -and $clientNetwork.IndexOf('mcast', [System.StringComparison]::OrdinalIgnoreCase) -ge 0
}
else
{
    $hostNetwork = Enable-PcapNetwork 4488 $PcapIfName
    $clientNetwork = Enable-PcapNetwork 4489 $PcapIfName
    $hostNetworkOk = $hostNetwork.IndexOf('type=pcap', [System.StringComparison]::OrdinalIgnoreCase) -ge 0
    $clientNetworkOk = $clientNetwork.IndexOf('type=pcap', [System.StringComparison]::OrdinalIgnoreCase) -ge 0
}

[pscustomobject]@{
    HostPid = $hostProc.Id
    ClientPid = $clientProc.Id
    HostConfig = $HostConfig
    ClientConfig = $ClientConfig
    HostMonitor = '127.0.0.1:4488'
    ClientMonitor = '127.0.0.1:4489'
    HostNetwork = if( $hostNetworkOk ) { 'enabled' } else { 'monitor output did not confirm' }
    ClientNetwork = if( $clientNetworkOk ) { 'enabled' } else { 'monitor output did not confirm' }
}
