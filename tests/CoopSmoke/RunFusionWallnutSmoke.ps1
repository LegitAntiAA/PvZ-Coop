#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateRange(90,3600)][int]$TimeoutSeconds = 240,
    [ValidateRange(0,65535)][int]$Port = 0,
    [ValidateSet(800,1066)][int]$HostWindowWidth = 1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth = 800,
    [uint32]$Seed = 424242
)

$ErrorActionPreference = 'Stop'
$wallnutRequestedHostWidth = $HostWindowWidth
$wallnutRequestedClientWidth = $ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed `
    -HostWindowWidth $wallnutRequestedHostWidth -ClientWindowWidth $wallnutRequestedClientWidth
if ($HostWindowWidth -ne $wallnutRequestedHostWidth -or $ClientWindowWidth -ne $wallnutRequestedClientWidth) {
    throw 'Shared smoke helper changed the requested WallNut window widths.'
}
$wallnutRepo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe = Join-Path $wallnutRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$wallnutGameDirectory = Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot = Join-Path $wallnutRepo 'work\fusion-wallnut' }
$wallnutRunId = [Guid]::NewGuid().ToString('N')
$wallnutRunRoot = Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_' + $wallnutRunId)
$wallnutHostRoot = Join-Path $wallnutRunRoot 'host'
$wallnutClientRoot = Join-Path $wallnutRunRoot 'client'
$wallnutHostLog = Join-Path $wallnutHostRoot 'logs'
$wallnutClientLog = Join-Path $wallnutClientRoot 'logs'
$wallnutHostProcess = $null; $wallnutClientProcess = $null
$wallnutPassed = $false; $wallnutError = ''; $wallnutCommon = @(); $wallnutPostPurchase = @(); $wallnutFrames = @()
$wallnutStarted = [DateTime]::UtcNow
$wallnutEnvironment = @{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY',
    'PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER',
    'PVZ_COOP_SMOKE_FUSION_ALMANAC','PVZ_COOP_SMOKE_FUSION_ALMANAC_MODE')) {
    $wallnutEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-WallnutFrames {
    param([string]$Directory,[string]$Role,[int]$Width)
    Add-Type -AssemblyName System.Drawing
    $names = @('chooser','picked','intro','planted','eating','crack1','crack2','resumed','complete')
    if ($Role -eq 'host') { $names += 'shovel_held' }
    foreach ($name in $names) {
        $path = Join-Path $Directory ('fusion_wallnut_' + $name + '.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual WallNut native frame: ' + $path) }
        $bitmap = [Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $Width -or $bitmap.Height -ne 600) { throw 'WallNut renderer dimensions differ from requested owned window.' }
            $colors = New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) {
                for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) }
            }
            if ($colors.Count -lt 8) { throw ('Blank/flat WallNut native frame: ' + $path) }
        } finally { $bitmap.Dispose() }
        [ordered]@{ role=$Role; frame=$name; path=$path; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $wallnutGameDirectory 'main.pak'),(Join-Path $wallnutGameDirectory 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('WallNut runtime input missing: ' + $path) }
    }
    foreach ($directory in @($wallnutHostLog,$wallnutClientLog,(Join-Path $wallnutHostRoot 'appdata'),(Join-Path $wallnutClientRoot 'appdata'))) { Ensure-Directory $directory }
    if ($Port -eq 0) { $Port = Get-FreeLoopbackPort }
    foreach ($name in $wallnutEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE = '1'; $env:PVZ_COOP_SMOKE_FUSION_WALLNUT = '1'
    foreach ($role in @('host','client')) {
        $root = if ($role -eq 'host') { $wallnutHostRoot } else { $wallnutClientRoot }
        $width = if ($role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth }
        $nickname = if ($role -eq 'host') { 'WallnutHost' } else { 'WallnutClient' }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') `
            -ResultPath (Join-Path $root 'result.json') -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native WallNut3 / stock animation adaptation; two owned windows: ' + $wallnutRunRoot)
    $wallnutHostProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $wallnutGameDirectory `
        -ManifestPath (Join-Path $wallnutHostRoot 'smoke.ini') -Role host -RunId $wallnutRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $wallnutRunRoot 'pids.json') -Executable $GameExe -HostProcess $wallnutHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $wallnutHostLog -Pattern ('(?im)host listening port=' + $Port + '\b') -Process $wallnutHostProcess -Role host -TimeoutSeconds 120
    $wallnutClientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $wallnutGameDirectory `
        -ManifestPath (Join-Path $wallnutClientRoot 'smoke.ini') -Role client -RunId $wallnutRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $wallnutRunRoot 'pids.json') -Executable $GameExe -HostProcess $wallnutHostProcess -ClientProcess $wallnutClientProcess
    $results = @(Wait-ForResults -HostResultPath (Join-Path $wallnutHostRoot 'result.json') -ClientResultPath (Join-Path $wallnutClientRoot 'result.json') `
        -HostProcess $wallnutHostProcess -ClientProcess $wallnutClientProcess -HostLogDirectory $wallnutHostLog -ClientLogDirectory $wallnutClientLog -TimeoutSeconds $TimeoutSeconds)
    foreach ($result in $results) {
        if ([string]$result.status -ne 'pass' -or [string]$result.mode -ne 'fusion_validation' -or [string]$result.scenario -ne 'fusion_validation' -or
            [int]$result.level -ne 1 -or [uint32]$result.seed -ne $Seed -or [bool]$result.skipIntro) { throw 'WallNut result is not the strict native manifest run.' }
        $samples = if ([string]$result.role -eq 'host') { [int]$result.snapshotsSent } else { [int]$result.snapshotsApplied }
        if ($samples -lt 8) { throw 'WallNut peer has fewer than eight actual native snapshots.' }
    }
    if ($results.Count -ne 2 -or $results[0].role -ne 'host' -or $results[1].role -ne 'client' -or $results[0].protocol -ne $results[1].protocol) { throw 'WallNut peer role/protocol identity mismatch.' }
    $hostText = Get-CombinedLogText $wallnutHostLog; $clientText = Get-CombinedLogText $wallnutClientLog
    if ($hostText -notmatch 'WALLNUT_AID purchased=1 isolatedSmokeProfile=1 fixtureControl=1' -or
        $hostText -notmatch 'WALLNUT_EATER id=\d+ row=2 x=\d+ healthNotWritten=1 naturalEatPlant=1 fixtureControl=spawnAndPositionOnly' -or
        $hostText -notmatch 'WALLNUT_MOVE id=\d+ x=900 HPnotWritten=1 fixtureControl=positionOnly') { throw 'WallNut native fixture controls/provenance are missing.' }
    foreach ($role in @('host','client')) {
        $text = if ($role -eq 'host') { $hostText } else { $clientText }
        if ($text -match 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') { throw ('WallNut run contains failure: ' + $role) }
        if ($text -notmatch ('WALLNUT_PLAYING role=' + $role + '[^\r\n]*nativeBoard=1 secondWorld=0 stockAnimation=1 sourceCampaignComplete=0') -or
            $text -notmatch ('WALLNUT_COMPLETE role=' + $role + '[^\r\n]*sun=350 HPmax=4000 sourceCD=3000 nativeBoard=1 secondWorld=0 stockAnimation=1 sourceSplashImplemented=0 sourceCampaignComplete=0')) { throw 'WallNut same-world/source-limit completion proof missing.' }
        $pool=[regex]::Match($text,('WALLNUT_POOL role='+$role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) ownFirst=1 nativeChooser=1'))
        if (-not $pool.Success -or [int]$pool.Groups[2].Value -ne [Math]::Min(10,[int]$pool.Groups[1].Value) -or
            [int]$pool.Groups[3].Value+[int]$pool.Groups[4].Value -ne [int]$pool.Groups[2].Value) { throw 'WallNut fixture truncated or invented live-pool quotas.' }
        foreach ($phase in @(0,1,2)) {
            if ($text -notmatch ('WALLNUT_BITE role='+$role+' phase='+$phase+' plant=\d+ health=\d+ max=4000 rate=0 actualCrack='+$phase+' naturalEatPlant=1')) { throw ('Actual WallNut natural bite/crack proof missing: '+$role+'/'+$phase) }
        }
        $nativeUses=if ($role -eq 'host') {1} else {0}
        if ($text -notmatch ('WALLNUT_COOLDOWN role='+$role+' normal=\d+ extra=\d+ ready=1 naturalCD=3000 nativeUses='+$nativeUses+' counterAuthority=host')) { throw 'WallNut actual native cooldown did not naturally complete.' }
        foreach ($buyer in @(0,1)) {
            if ($text -notmatch ('WALLNUT_REJECT role='+$role+' buyer='+$buyer+' target=\d+ packet=\d+ sourceRecipe=3\+3 result=255 unsupported=1 sun=350 actorHPunchanged=1 cooldownUnchanged=1 nativeSDL=1')) { throw 'WallNut unsupported duplicate recipe was not rejected atomically on both peers.' }
        }
    }
    $resume=[regex]::Match($hostText,'WALLNUT_RESUME role=host plant=\d+ health=(\d+) rate=([0-9.]+) stockAnimation=1 maximumTailTicks=50 observedHostTicks=(\d+) exactSourceCosmetic=0')
    if (-not $resume.Success -or [int]$resume.Groups[1].Value -ge 1333 -or [int]$resume.Groups[1].Value -le 0 -or
        [int]$resume.Groups[3].Value -gt 50) { throw 'WallNut stock pause/resume tail exceeds50 native ticks.' }
    $rate=[double]::Parse($resume.Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture)
    if ($rate -lt 10 -or $rate -gt 15) { throw 'WallNut did not restore the actual native random animation range.' }
    $clientResume=[regex]::Match($clientText,'WALLNUT_RESUME role=client plant=\d+ health=(\d+) rate=([0-9.]+) stockAnimation=1')
    if (-not $clientResume.Success -or $clientResume.Groups[1].Value -ne $resume.Groups[1].Value) { throw 'WallNut settled native health differs across peers.' }
    foreach ($buyer in @(0,1)) {
        $price=if ($buyer) {100} else {50}; $sun=if ($buyer) {350} else {450}
        $pattern='WALLNUT_PURCHASE role=\w+ plant=(\d+) packet=(\d+) kind=3 owner='+$buyer+' extra='+$buyer+' price='+$price+' sun='+$sun+' HP=4000 sourceCD=3000 nativeSDL=1'
        $a=[regex]::Match($hostText,$pattern); $b=[regex]::Match($clientText,$pattern)
        if (-not $a.Success -or -not $b.Success -or $a.Groups[1].Value -ne $b.Groups[1].Value -or $a.Groups[2].Value -ne $b.Groups[2].Value) { throw 'WallNut source/native packet and actor identities differ.' }
        if ($hostText -notmatch ('Fusion native command slot='+$buyer+' action=5 sequence=\d+ applied=0 column=7 row=2 packet='+$a.Groups[2].Value+'\b')) { throw 'WallNut negative SDL request did not reach real host command handling.' }
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX); $applied=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $wallnutCommon=@($applied | Where-Object { $tx -contains $_ } | Select-Object -Unique)
    if ($wallnutCommon.Count -lt 8) { throw 'WallNut has fewer than eight common actual native snapshot hashes.' }
    $postTx=@(Get-SnapshotHashes -Text $hostText.Substring($hostText.IndexOf('SMOKE_FUSION_WALLNUT_PURCHASE')) -Marker SMOKE_SNAPSHOT_TX)
    $postApplied=@(Get-SnapshotHashes -Text $clientText.Substring($clientText.IndexOf('SMOKE_FUSION_WALLNUT_PURCHASE')) -Marker SMOKE_SNAPSHOT_APPLIED)
    $wallnutPostPurchase=@($postApplied | Where-Object { $postTx -contains $_ } | Select-Object -Unique)
    if ($wallnutPostPurchase.Count -lt 2) { throw 'WallNut post-purchase native state has fewer than two common hashes.' }
    $wallnutFrames=@((Assert-WallnutFrames $wallnutHostRoot host $HostWindowWidth),(Assert-WallnutFrames $wallnutClientRoot client $ClientWindowWidth))
    $wallnutPassed=$true
} catch {
    $wallnutError=$_.Exception.Message; if ($_.ScriptStackTrace) { $wallnutError+=' | '+$_.ScriptStackTrace }
    Write-SmokeStatus ('FAIL native WallNut: '+$wallnutError)
} finally {
    foreach ($name in $wallnutEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$wallnutEnvironment[$name],'Process') }
    $clientStopped=Stop-OwnedProcess -Process $wallnutClientProcess -Role client
    $hostStopped=Stop-OwnedProcess -Process $wallnutHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) { $wallnutPassed=$false; $wallnutError+=' Owned process cleanup failed.' }
    Ensure-Directory $wallnutRunRoot
    $summary=[ordered]@{ status=$(if ($wallnutPassed) {'pass'} else {'fail'}); error=$wallnutError; fixture='FusionNativeWallnut'; run_id=$wallnutRunId;
        started_utc=$wallnutStarted.ToString('o'); finished_utc=[DateTime]::UtcNow.ToString('o'); executable=$GameExe;
        coop_difficulty=1; source_difficulty=2; native_grid='10x5'; source_kind=3; second_world=$false;
        native_cosmetic_adaptation='stockPauseResumeAndCracks'; source_splash_implemented=$false; source_campaign_complete=$false;
        requested_host_width=$wallnutRequestedHostWidth; requested_client_width=$wallnutRequestedClientWidth;
        common_snapshot_hashes=$wallnutCommon; post_purchase_common_hashes=$wallnutPostPurchase; frames=$wallnutFrames;
        owned_cleanup=[bool]($clientStopped -and $hostStopped); artifacts_root=$wallnutRunRoot }
    Write-Utf8NoBom -Path (Join-Path $wallnutRunRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($wallnutPassed) { exit 0 } else { exit 1 }
