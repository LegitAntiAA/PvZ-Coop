#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateRange(60,3600)][int]$TimeoutSeconds = 180,
    [ValidateRange(0,65535)][int]$Port = 0,
    [ValidateSet(800,1066)][int]$HostWindowWidth = 1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth = 1066,
    [uint32]$Seed = 424242
)

$ErrorActionPreference = 'Stop'
$potatoRequestedHostWidth = $HostWindowWidth
$potatoRequestedClientWidth = $ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed `
    -HostWindowWidth $potatoRequestedHostWidth -ClientWindowWidth $potatoRequestedClientWidth
if ($HostWindowWidth -ne $potatoRequestedHostWidth -or $ClientWindowWidth -ne $potatoRequestedClientWidth) {
    throw 'Shared smoke helper changed the requested Potato window widths.'
}
$potatoRepo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe = Join-Path $potatoRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$potatoGameDirectory = Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot = Join-Path $potatoRepo 'work\fusion-potato' }
$potatoRunId = [Guid]::NewGuid().ToString('N')
$potatoRunRoot = Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_' + $potatoRunId)
$potatoHostRoot = Join-Path $potatoRunRoot 'host'
$potatoClientRoot = Join-Path $potatoRunRoot 'client'
$potatoHostLog = Join-Path $potatoHostRoot 'logs'
$potatoClientLog = Join-Path $potatoClientRoot 'logs'
$potatoHostProcess = $null; $potatoClientProcess = $null
$potatoPassed = $false; $potatoError = ''; $potatoCommon = @(); $potatoPostPurchase = @(); $potatoFrames = @()
$potatoStarted = [DateTime]::UtcNow
$potatoEnvironment = @{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_CHERRY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES',
    'PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER')) {
    $potatoEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}

function Assert-PotatoFrames {
    param([string]$Directory,[string]$Role,[int]$Width)
    Add-Type -AssemblyName System.Drawing
    $names = @('chooser','picked','intro','planted','preparing','rising','armed','targets','after','complete')
    if ($Role -eq 'client') { $names += 'held' }
    foreach ($name in $names) {
        $path = Join-Path $Directory ('fusion_potato_' + $name + '.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual Potato native frame: ' + $path) }
        $bitmap = [Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $Width -or $bitmap.Height -ne 600) { throw 'Potato native renderer dimensions do not match the requested window.' }
            $colors = New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) {
                for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) }
            }
            if ($colors.Count -lt 8) { throw ('Blank/flat Potato native frame: ' + $path) }
        } finally { $bitmap.Dispose() }
        [ordered]@{ role=$Role; frame=$name; path=$path; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    }
}

try {
    foreach ($path in @($GameExe,(Join-Path $potatoGameDirectory 'main.pak'),(Join-Path $potatoGameDirectory 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Potato runtime input missing: ' + $path) }
    }
    foreach ($directory in @($potatoHostLog,$potatoClientLog,(Join-Path $potatoHostRoot 'appdata'),(Join-Path $potatoClientRoot 'appdata'))) { Ensure-Directory $directory }
    if ($Port -eq 0) { $Port = Get-FreeLoopbackPort }
    foreach ($name in $potatoEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE = '1'; $env:PVZ_COOP_SMOKE_FUSION_POTATO = '1'
    foreach ($role in @('host','client')) {
        $root = if ($role -eq 'host') { $potatoHostRoot } else { $potatoClientRoot }
        $width = if ($role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth }
        $nickname = if ($role -eq 'host') { 'PotatoHost' } else { 'PotatoClient' }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') `
            -ResultPath (Join-Path $root 'result.json') -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native Potato: source-normal2 / co-op10, actual14-16s preparation, two owned windows; ' + $potatoRunRoot)
    $potatoHostProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $potatoGameDirectory `
        -ManifestPath (Join-Path $potatoHostRoot 'smoke.ini') -Role host -RunId $potatoRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $potatoRunRoot 'pids.json') -Executable $GameExe -HostProcess $potatoHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $potatoHostLog -Pattern ('(?im)host listening port=' + $Port + '\b') -Process $potatoHostProcess -Role host -TimeoutSeconds 120
    $potatoClientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $potatoGameDirectory `
        -ManifestPath (Join-Path $potatoClientRoot 'smoke.ini') -Role client -RunId $potatoRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $potatoRunRoot 'pids.json') -Executable $GameExe -HostProcess $potatoHostProcess -ClientProcess $potatoClientProcess
    $results = @(Wait-ForResults -HostResultPath (Join-Path $potatoHostRoot 'result.json') -ClientResultPath (Join-Path $potatoClientRoot 'result.json') `
        -HostProcess $potatoHostProcess -ClientProcess $potatoClientProcess -HostLogDirectory $potatoHostLog -ClientLogDirectory $potatoClientLog -TimeoutSeconds $TimeoutSeconds)
    foreach ($result in $results) {
        if ([string]$result.status -ne 'pass' -or [string]$result.mode -ne 'fusion_validation' -or [string]$result.scenario -ne 'fusion_validation' -or
            [int]$result.level -ne 1 -or [uint32]$result.seed -ne $Seed -or [bool]$result.skipIntro) { throw 'Potato result is not the strict native manifest run.' }
        $samples = if ([string]$result.role -eq 'host') { [int]$result.snapshotsSent } else { [int]$result.snapshotsApplied }
        if ($samples -lt 8) { throw 'Potato peer has fewer than eight actual native snapshots.' }
    }
    if ($results.Count -ne 2 -or $results[0].role -ne 'host' -or $results[1].role -ne 'client' -or $results[0].protocol -ne $results[1].protocol) { throw 'Potato peer result/protocol identity mismatch.' }
    $hostText = Get-CombinedLogText $potatoHostLog; $clientText = Get-CombinedLogText $potatoClientLog
    foreach ($role in @('host','client')) {
        $text = if ($role -eq 'host') { $hostText } else { $clientText }
        if ($text -match 'SMOKE_FAIL|SMOKE_STOPPED|rejected|seq-gap|invalid snapshot|fatal|secondWorld=1') { throw ('Potato run contains a rejection/failure: '+$role) }
        if ($text -notmatch ('POTATO_PLAYING role='+$role+'[^\r\n]*coopDifficulty=10 sourceDifficulty=2 nativeBoard=1 secondWorld=0 target=\d+') -or
            $text -notmatch ('POTATO_COMPLETE role='+$role+'[^\r\n]*sun=450 sourceCD=3000 cases=8 nativeBoard=1 secondWorld=0 collider=nativeRectAdaptation sourceCampaignComplete=0 lateJoinTested=0')) {
            throw ('Missing same native world/source difficulty completion evidence: '+$role)
        }
        $bite=[regex]::Match($text,('POTATO_UNARMED role='+$role+' plant=(\d+) hp=(\d+) maxHP=300 armed=0 exploded=0 actualNativeBite=1'))
        if (-not $bite.Success -or [int]$bite.Groups[2].Value -le 0 -or [int]$bite.Groups[2].Value -ge 300) { throw ('Actual unarmed native bite was not observed: '+$role) }
    }
    $purchase='POTATO_PURCHASE role=\w+ plant=(\d+) packet=(\d+) kind=4 owner=1 extra=1 column=9 row=2 price=50 sun=450 sourceCD=3000 nativeActor=1 nativeSDL=1'
    $a=[regex]::Match($hostText,$purchase);$b=[regex]::Match($clientText,$purchase)
    if (-not $a.Success -or -not $b.Success -or $a.Groups[1].Value -ne $b.Groups[1].Value -or $a.Groups[2].Value -ne $b.Groups[2].Value -or
        $clientText -notmatch 'POTATO_HELD role=client[^\r\n]*charge=0 cooldown=0 snapshotEcho=1 nativeSDL=1') { throw 'Potato real pickup/purchase identity or no-premature-charge proof missing.' }
    $mineId=$a.Groups[1].Value
    $initial=[regex]::Match($hostText,'Fusion native Potato prepared id=\d+ ticks=(\d+) sourceSecondsMin=14 sourceSecondsMax=16 checkBeforeDecrement=1')
    $preparation=[regex]::Match($hostText,'POTATO_PREPARATION role=host initial=(\d+) elapsed=(\d+) sourceRange=14,16 checkBeforeDecrement=1 nativeClock=1')
    $rise=[regex]::Match($hostText,'POTATO_RISE role=host elapsed=(\d+) expected=84 sourceEvent=\.8333333 nativeClock=1')
    if (-not $initial.Success -or [int]$initial.Groups[1].Value -lt 1401 -or [int]$initial.Groups[1].Value -gt 1601 -or
        -not $preparation.Success -or [Math]::Abs([int]$preparation.Groups[1].Value-[int]$preparation.Groups[2].Value) -gt 1 -or
        -not $rise.Success -or [Math]::Abs([int]$rise.Groups[1].Value-84) -gt 1) { throw 'Potato actual source preparation/rise clock proof missing or outside bounds.' }
    $rising=[regex]::Match($hostText,('Fusion native Potato rising id='+$mineId+' tick=(\d+) eventTicks=84'))
    $armed=[regex]::Match($hostText,('Fusion native Potato armed id='+$mineId+' tick=(\d+)'))
    $explosion=[regex]::Matches($hostText,('Fusion native Potato explosion id='+$mineId+' tick=(\d+) delayTicks=10 sourceDifficulty=2'))
    $retired=[regex]::Match($hostText,('Fusion native Potato retired id='+$mineId+' tick=(\d+)'))
    if (-not $rising.Success -or -not $armed.Success -or [int]$armed.Groups[1].Value-[int]$rising.Groups[1].Value -ne 84 -or
        $explosion.Count -ne 1 -or -not $retired.Success -or [int]$retired.Groups[1].Value-[int]$explosion[0].Groups[1].Value -ne 10 -or
        $hostText -notmatch ('POTATO_DOINGSPECIAL role=host plant='+$mineId+' remaining=\d+ nativeActorRetained=1 actualSnapshotForced=1 sourceInvincible=1')) {
        throw 'Potato repeated/exact84tick rise/10tick retained-actor explosion proof failed.'
    }
    if ($clientText -notmatch ('Fusion native Potato effect id='+$mineId+' fxOnly=1 awaitHostRetirement=1') -or
        $clientText -notmatch ('POTATO_LATE_REPLAY role=client plant='+$mineId+' freshNativeGeneration=1 metadataKind=4 light=1 state=DOINGSPECIAL controlledPendingHostSnapshotReplay=1 lateJoinTested=0 queueDrop=0 hostClockJump=0')) {
        throw 'Potato client FX-only delayed retirement or controlled fresh-ghost pending-host-snapshot adoption proof missing.'
    }
    $cases=[ordered]@{contact=@(1,-1,-1,-1);blast_armor=@(0,270,3200,3200);outside=@(0,270,0,0);row_minus=@(0,270,0,0);
        row_plus=@(0,270,0,0);mindcontrol=@(0,270,0,0);highbody=@(0,3200,0,0);shield=@(0,1200,0,2200)}
    foreach ($case in $cases.Keys) {
        $pattern='POTATO_RESULT role=\w+ case='+$case+' id=(\d+) dead=([01]) body=(\d+) first=(\d+) second=(\d+) mind=([01]) armorImageAndTrackMatch=1 nativeActor=1'
        $left=[regex]::Match($hostText,$pattern);$right=[regex]::Match($clientText,$pattern);$expected=$cases[$case]
        if (-not $left.Success -or -not $right.Success -or $left.Groups[1].Value -ne $right.Groups[1].Value) { throw ('Potato target identity/image proof missing: '+$case) }
        foreach ($row in @($left,$right)) {
            if ([int]$row.Groups[2].Value -ne $expected[0] -or [int]$row.Groups[6].Value -ne $(if($case -eq 'mindcontrol'){1}else{0})) { throw ('Potato target death/mind-control mismatch: '+$case) }
            if (-not $expected[0]) {for($i=1;$i -le 3;++$i){if([int]$row.Groups[$i+2].Value -ne $expected[$i]){throw ('Potato body/armor mismatch: '+$case)}}}
        }
    }
    if ([regex]::Matches($hostText,'Fusion native Potato impact[^\r\n]*damage=1800 sourceDifficulty=2').Count -ne 4 -or
        $hostText -notmatch 'Potato impact[^\r\n]*body=270 first=5000 second=5000 afterBody=270 afterFirst=3200 afterSecond=3200 dead=0' -or
        $hostText -notmatch 'Potato impact[^\r\n]*body=2000 first=1000 second=4000 afterBody=1200 afterFirst=0 afterSecond=2200 dead=0') {
        throw 'Potato source Explode1800 armor path did not run on all four native targets.'
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$applied=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $potatoCommon=@($applied|Where-Object{$tx -contains $_}|Select-Object -Unique)
    if($potatoCommon.Count -lt 8){throw 'Potato has fewer than eight matching native snapshot hashes.'}
    $postTx=@(Get-SnapshotHashes -Text $hostText.Substring($hostText.IndexOf('SMOKE_FUSION_POTATO_PURCHASE')) -Marker SMOKE_SNAPSHOT_TX)
    $postApplied=@(Get-SnapshotHashes -Text $clientText.Substring($clientText.IndexOf('SMOKE_FUSION_POTATO_PURCHASE')) -Marker SMOKE_SNAPSHOT_APPLIED)
    $potatoPostPurchase=@($postApplied|Where-Object{$postTx -contains $_}|Select-Object -Unique)
    if($potatoPostPurchase.Count -lt 2){throw 'Potato has fewer than two common post-purchase hashes.'}
    $potatoFrames=@((Assert-PotatoFrames $potatoHostRoot host $HostWindowWidth),(Assert-PotatoFrames $potatoClientRoot client $ClientWindowWidth))
    $potatoPassed=$true
} catch {$potatoError=$_.Exception.Message;if($_.ScriptStackTrace){$potatoError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL native Potato: '+$potatoError)}
finally {
    foreach($name in $potatoEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$potatoEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $potatoClientProcess -Role client
    $hostStopped=Stop-OwnedProcess -Process $potatoHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$potatoPassed=$false;$potatoError+=' Owned process cleanup failed.'}
    Ensure-Directory $potatoRunRoot
    $summary=[ordered]@{status=$(if($potatoPassed){'pass'}else{'fail'});error=$potatoError;fixture='FusionNativePotato';run_id=$potatoRunId;
        started_utc=$potatoStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;
        coop_difficulty=10;source_difficulty=2;native_grid='10x5';source_kind=4;second_world=$false;source_campaign_complete=$false;
        collider='nativeRectAdaptation';latejoin_tested=$false;controlled_fresh_ghost_replay=$true;
        common_snapshot_hashes=$potatoCommon;post_purchase_common_hashes=$potatoPostPurchase;frames=$potatoFrames;
        owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$potatoRunRoot}
    Write-Utf8NoBom -Path (Join-Path $potatoRunRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if($potatoPassed){exit 0}else{exit 1}
