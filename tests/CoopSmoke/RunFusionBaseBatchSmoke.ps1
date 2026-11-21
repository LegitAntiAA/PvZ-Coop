#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '', [string]$ArtifactsRoot = '',
    [ValidateRange(60,3600)][int]$TimeoutSeconds = 180,
    [ValidateRange(0,65535)][int]$Port = 0,
    [ValidateSet(800,1066)][int]$HostWindowWidth = 1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth = 800,
    [uint32]$Seed = 424242
)
$ErrorActionPreference = 'Stop'
$batchHostWidth=$HostWindowWidth; $batchClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed `
    -HostWindowWidth $batchHostWidth -ClientWindowWidth $batchClientWidth
if ($HostWindowWidth -ne $batchHostWidth -or $ClientWindowWidth -ne $batchClientWidth) { throw 'Shared helper changed requested batch window widths.' }
$batchRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $batchRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $batchGameDirectory=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $batchRepo 'work\fusion-base-batch' }
$batchRunId=[Guid]::NewGuid().ToString('N'); $batchRunRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$batchRunId)
$batchHostRoot=Join-Path $batchRunRoot 'host'; $batchClientRoot=Join-Path $batchRunRoot 'client'
$batchHostLog=Join-Path $batchHostRoot 'logs'; $batchClientLog=Join-Path $batchClientRoot 'logs'
$batchHostProcess=$null; $batchClientProcess=$null
$batchPassed=$false; $batchError=''; $batchCommon=@(); $batchPostPurchase=@(); $batchFrames=@(); $batchStarted=[DateTime]::UtcNow
$batchEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_WALLNUT',
    'PVZ_COOP_SMOKE_FUSION_CHERRY','PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES',
    'PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_FUSION_ALMANAC','PVZ_COOP_SMOKE_FUSION_ALMANAC_MODE')) {
    $batchEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-BatchFrames {
    param([string]$Directory,[string]$Role,[int]$Width)
    Add-Type -AssemblyName System.Drawing
    foreach ($name in @('chooser','picked','intro','playing','planted','combat','complete')) {
        $path=Join-Path $Directory ('fusion_base_'+$name+'.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual native batch frame: '+$path) }
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $Width -or $bitmap.Height -ne 600) { throw 'Batch frame dimensions differ from the owned window.' }
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            if ($colors.Count -lt 8) { throw ('Blank/flat native batch frame: '+$path) }
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $batchGameDirectory 'main.pak'),(Join-Path $batchGameDirectory 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Batch runtime input missing: '+$path) }
    }
    foreach ($dir in @($batchHostLog,$batchClientLog,(Join-Path $batchHostRoot 'appdata'),(Join-Path $batchClientRoot 'appdata'))) { Ensure-Directory $dir }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $batchEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_BASE_BATCH='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') { $batchHostRoot } else { $batchClientRoot }
        $width=if ($role -eq 'host') { $batchHostWidth } else { $batchClientWidth }
        $nick=if ($role -eq 'host') {'BaseHost'} else {'BaseClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nick -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nick `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') `
            -ResultPath (Join-Path $root 'result.json') -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native base5/6/7, natural30s digest, source combat / native art adaptation: '+$batchRunRoot)
    $batchHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $batchGameDirectory `
        -ManifestPath (Join-Path $batchHostRoot 'smoke.ini') -Role host -RunId $batchRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $batchRunRoot 'pids.json') -Executable $GameExe -HostProcess $batchHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $batchHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $batchHostProcess -Role host -TimeoutSeconds 120
    $batchClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $batchGameDirectory `
        -ManifestPath (Join-Path $batchClientRoot 'smoke.ini') -Role client -RunId $batchRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $batchRunRoot 'pids.json') -Executable $GameExe -HostProcess $batchHostProcess -ClientProcess $batchClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $batchHostRoot 'result.json') -ClientResultPath (Join-Path $batchClientRoot 'result.json') `
        -HostProcess $batchHostProcess -ClientProcess $batchClientProcess -HostLogDirectory $batchHostLog -ClientLogDirectory $batchClientLog -TimeoutSeconds $TimeoutSeconds)
    foreach ($result in $results) {
        if ([string]$result.status -ne 'pass' -or [string]$result.mode -ne 'fusion_validation' -or [string]$result.scenario -ne 'fusion_validation' -or
            [int]$result.level -ne 1 -or [uint32]$result.seed -ne $Seed -or [bool]$result.skipIntro) { throw 'Batch result does not match strict native manifest.' }
        $samples=if ([string]$result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        if ($samples -lt 8) { throw 'Batch peer has fewer than8 actual snapshots.' }
    }
    if ($results.Count -ne 2 -or $results[0].role -ne 'host' -or $results[1].role -ne 'client' -or $results[0].protocol -ne $results[1].protocol) { throw 'Batch role/protocol mismatch.' }
    $hostText=Get-CombinedLogText $batchHostLog; $clientText=Get-CombinedLogText $batchClientLog
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        if ($text -match 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') { throw ('Batch run contains failure: '+$role) }
        if ($text -notmatch ('BASE_PLAYING role='+$role+'[^\r\n]*nativeBoard=1 secondWorld=0 daytime=1 coopDifficulty=1 sourceDifficulty=2 nativeArtAdaptation=1') -or
            $text -notmatch ('BASE_COMPLETE role='+$role+' sun=250 kinds=5,6,7 HPmax=300 CD=750 nativeBoard=1 secondWorld=0 naturalDigest=30 stockArtAdaptation=1 sourceCampaignComplete=0 healthWritesAfterSetup=0 timerWrites=0') -or
            $text -notmatch ('BASE_COMBAT role='+$role+' swallowedA=1 challenger=270 puff=250 fume=250 shieldBody=250 shield=80 adjacent=270 outside=270 snapshotAuthority=1')) { throw ('Batch actual native combat/completion proof missing: '+$role) }
        $pool=[regex]::Match($text,('BASE_POOL role='+$role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) priorities=5normal,6extra,7normal nativeChooser=1 ownFirst=1'))
        if (-not $pool.Success -or [int]$pool.Groups[2].Value -lt 3 -or [int]$pool.Groups[2].Value -ne [Math]::Min(10,[int]$pool.Groups[1].Value) -or
            [int]$pool.Groups[3].Value -lt 2 -or [int]$pool.Groups[4].Value -lt 1 -or
            [int]$pool.Groups[3].Value+[int]$pool.Groups[4].Value -ne [int]$pool.Groups[2].Value) { throw 'Batch fixture invented/truncated actual eligible pool quotas.' }
    }
    $kinds=@(5,6,7); $owners=@(0,1,0); $extras=@(0,1,0); $prices=@(150,25,75); $sun=@(350,325,250)
    for ($i=0;$i -lt 3;$i++) {
        $pattern='BASE_PURCHASE role=\w+ kind='+$kinds[$i]+' plant=(\d+) packet=(\d+) owner='+$owners[$i]+' extra='+$extras[$i]+' price='+$prices[$i]+' sun='+$sun[$i]+' HP=300 sourceCD=750 awake=1 nativeSDL=1'
        $a=[regex]::Match($hostText,$pattern);$b=[regex]::Match($clientText,$pattern)
        if (-not $a.Success -or -not $b.Success -or $a.Groups[1].Value -ne $b.Groups[1].Value -or $a.Groups[2].Value -ne $b.Groups[2].Value) { throw ('Batch purchase identity differs forkind'+$kinds[$i]) }
        foreach ($role in @('host','client')) {
            $text=if ($role -eq 'host') {$hostText} else {$clientText}
            if ($text -notmatch ('BASE_CD role='+$role+' kind='+$kinds[$i]+' packet='+$a.Groups[2].Value+' ready=1 refreshTime=750 uses=\d+ conserved=1 hostCounterAuthority=1')) { throw 'Batch native packet cooldown/use conservation proof missing.' }
        }
    }
    if ($clientText -notmatch 'BASE_HELD role=client kind=6 packet=\d+ price=25 sun=350 snapshotEcho=1 nativeSDL=1') { throw 'Actual client source-price pickup/snapshot echo not proven.' }
    foreach ($proof in @(
        'BASE_CHOMP_LOCK role=host plant=\d+ target=\d+ challenger=\d+ challengerMovedCloser=1 nativeTargetGeneration=1 timerWrites=0',
        'BASE_CHOMP_BITE role=host plant=\d+ swallowed=\d+ challenger=\d+ challengerHP=270 digestRemaining=\d+ tick=\d+ naturalKill=1 healthWrites=0 timerWrites=0',
        'BASE_FUME role=host body=250 shieldBody=250 shield=80 adjacent=270 outside=270 damage=20 nativeFlags=2 sameEvent=1 nativeShieldVisible=1 healthWrites=0',
        'BASE_PUFF_HIT role=host id=\d+ prey=\d+ before=270 after=250 hits=1 retired=1 damage=20 healthWrites=0',
        'BASE_PUFF_MISS role=host id=\d+ realNaturalShot=1 targetMovedAfterTrigger=1 age=\d+ velocity=3\.200000',
        'BASE_PUFF_75 role=host id=\d+ age=75 alive=1 expired=0 actualNativeProjectile=1',
        'BASE_PUFF_EXPIRE role=host id=\d+ lastAlive=75 removedAge=76 velocity=3\.2 health=250 nativeClock=1 timerWrites=0')) {
        if ($hostText -notmatch $proof) { throw ('Batch natural native behavior proof missing: '+$proof) }
    }
    $digest=[regex]::Match($hostText,'BASE_CHOMP_DIGEST role=host plant=\d+ elapsed=(\d+) initialRemaining=(\d+) sourceSeconds=30 naturalClock=1 timerWrites=0')
    if (-not $digest.Success -or [int]$digest.Groups[2].Value -lt 2991 -or [int]$digest.Groups[2].Value -gt 3000 -or
        [Math]::Abs([int]$digest.Groups[1].Value-[int]$digest.Groups[2].Value) -gt 1) { throw 'Actual Chomper source30s natural digest not proven.' }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$applied=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $batchCommon=@($applied|Where-Object {$tx -contains $_}|Select-Object -Unique)
    if ($batchCommon.Count -lt 8) { throw 'Batch has fewer than8 common actual snapshot hashes.' }
    $txAfter=@(Get-SnapshotHashes -Text $hostText.Substring($hostText.IndexOf('SMOKE_FUSION_BASE_COMBAT')) -Marker SMOKE_SNAPSHOT_TX)
    $rxAfter=@(Get-SnapshotHashes -Text $clientText.Substring($clientText.IndexOf('SMOKE_FUSION_BASE_COMBAT')) -Marker SMOKE_SNAPSHOT_APPLIED)
    $batchPostPurchase=@($rxAfter|Where-Object {$txAfter -contains $_}|Select-Object -Unique)
    if ($batchPostPurchase.Count -lt 2) { throw 'Batch has fewer than2 common post-combat snapshot hashes.' }
    $batchFrames=@((Assert-BatchFrames $batchHostRoot host $batchHostWidth),(Assert-BatchFrames $batchClientRoot client $batchClientWidth));$batchPassed=$true
} catch {
    $batchError=$_.Exception.Message;if ($_.ScriptStackTrace) {$batchError+=' | '+$_.ScriptStackTrace}
    Write-SmokeStatus ('FAIL native base batch: '+$batchError)
} finally {
    foreach ($name in $batchEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$batchEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $batchClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $batchHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$batchPassed=$false;$batchError+=' Owned process cleanup failed.'}
    Ensure-Directory $batchRunRoot
    $summary=[ordered]@{status=$(if ($batchPassed) {'pass'} else {'fail'});error=$batchError;fixture='FusionNativeBaseBatch';run_id=$batchRunId;
        started_utc=$batchStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;source_kinds=@(5,6,7);
        coop_difficulty=1;source_difficulty=2;native_grid='10x5';second_world=$false;native_art_adaptation=$true;source_campaign_complete=$false;
        health_writes_after_setup=$false;plant_timer_shortcuts=$false;requested_host_width=$batchHostWidth;requested_client_width=$batchClientWidth;
        common_snapshot_hashes=$batchCommon;post_combat_common_hashes=$batchPostPurchase;frames=$batchFrames;
        owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$batchRunRoot}
    Write-Utf8NoBom -Path (Join-Path $batchRunRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($batchPassed) {exit 0} else {exit 1}
