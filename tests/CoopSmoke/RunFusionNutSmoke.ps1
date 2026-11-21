#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(60,3600)][int]$TimeoutSeconds=180,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$nutHostWidth=$HostWindowWidth; $nutClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $nutHostWidth -ClientWindowWidth $nutClientWidth
if ($HostWindowWidth -ne $nutHostWidth -or $ClientWindowWidth -ne $nutClientWidth) { throw 'Shared helper changed requested widths.' }
$nutRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $nutRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $nutGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $nutRepo 'work\fusion-nut' }
$nutRunId=[Guid]::NewGuid().ToString('N'); $nutRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$nutRunId)
$nutHost=Join-Path $nutRoot 'host'; $nutClient=Join-Path $nutRoot 'client'
$nutHostLog=Join-Path $nutHost 'logs'; $nutClientLog=Join-Path $nutClient 'logs'
$nutHostProcess=$null; $nutClientProcess=$null; $nutPassed=$false; $nutError=''; $nutFrames=@(); $nutCommon=@(); $nutPost=@(); $nutExeSha=''
$nutStarted=[DateTime]::UtcNow; $nutEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_NUT','PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH',
    'PVZ_COOP_SMOKE_FUSION_CHERRY','PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER')) {
    $nutEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-NutFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $names=@('chooser','picked','intro','playing','fused','projectile','triple','crack1','crack2','complete')
    if ($Role -eq 'client') { $names+='held' }
    foreach ($name in $names) {
        $path=Join-Path $Directory ('fusion_nut_'+$name+'.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual renderer frame: '+$path) }
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $Width -or $bitmap.Height -ne 600) { throw 'Actual viewport dimensions mismatch.' }
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            if ($colors.Count -lt 8) { throw ('Blank renderer frame: '+$path) }
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $nutGame 'main.pak'),(Join-Path $nutGame 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Runtime input missing: '+$path) }
    }
    $nutExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($nutHostLog,$nutClientLog,(Join-Path $nutHost 'appdata'),(Join-Path $nutClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $nutEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_NUT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$nutHost} else {$nutClient}
        $width=if ($role -eq 'host') {$nutHostWidth} else {$nutClientWidth}
        $nickname=if ($role -eq 'host') {'NutHost'} else {'NutClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native Nut1004: two hidden owned windows; '+$nutRoot)
    $nutHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $nutGame -ManifestPath (Join-Path $nutHost 'smoke.ini') -Role host -RunId $nutRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $nutRoot 'pids.json') -Executable $GameExe -HostProcess $nutHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $nutHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $nutHostProcess -Role host -TimeoutSeconds 120
    $nutClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $nutGame -ManifestPath (Join-Path $nutClient 'smoke.ini') -Role client -RunId $nutRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $nutRoot 'pids.json') -Executable $GameExe -HostProcess $nutHostProcess -ClientProcess $nutClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $nutHost 'result.json') -ClientResultPath (Join-Path $nutClient 'result.json') `
        -HostProcess $nutHostProcess -ClientProcess $nutClientProcess -HostLogDirectory $nutHostLog -ClientLogDirectory $nutClientLog -TimeoutSeconds $TimeoutSeconds)
    if ($results.Count -ne 2 -or $results[0].role -ne 'host' -or $results[1].role -ne 'client' -or $results[0].protocol -ne $results[1].protocol) { throw 'Peer identity/protocol mismatch.' }
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        if ($result.status -ne 'pass' -or $result.mode -ne 'fusion_validation' -or $result.scenario -ne 'fusion_validation' -or
            [int]$result.level -ne 1 -or [uint32]$result.seed -ne $Seed -or [bool]$result.skipIntro -or $samples -lt 8) { throw 'Not a strict full-intro native manifest run.' }
    }

    $hostText=Get-CombinedLogText $nutHostLog; $clientText=Get-CombinedLogText $nutClientLog
    $hello=[regex]::Match($hostText,'HELLO slot=1 protocol=(\d+) expected=(\d+)')
    if (-not $hello.Success -or $hello.Groups[1].Value -ne $hello.Groups[2].Value -or
        [int]$hello.Groups[1].Value -ne [int]$results[0].protocol -or $clientText -notmatch 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') { throw 'Strict HELLO/WELCOME Fusion negotiation missing.' }
    $purchases=@(); $complete=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        if ($text -match 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') { throw ('Fixture/runtime failure: '+$role) }
        if ($text -notmatch ('NUT_PLAYING role='+$role+' sun=500 nativeBoard=1 secondWorld=0') -or
            $text -notmatch ('NUT_READY role='+$role+' nativeSDL=1 hoverObserved=1')) { throw 'Real native Ready/full-intro proof missing.' }
        $pool=[regex]::Match($text,('NUT_POOL role='+$role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) normal0Host=1 normal3Client=1 unfiltered=1'))
        if (-not $pool.Success -or [int]$pool.Groups[1].Value -lt 16 -or [int]$pool.Groups[2].Value -ne [Math]::Min(10,[int]$pool.Groups[1].Value) -or
            [int]$pool.Groups[3].Value+[int]$pool.Groups[4].Value -ne [int]$pool.Groups[2].Value) { throw 'Real unfiltered native quota/chooser mismatch.' }
        $purchase=[regex]::Match($text,('NUT_PURCHASE role='+$role+' old=(\d+) id=(\d+) packet=(\d+) parents=0,3 owner=1 extra=0 usedPrice=50 sun=350 HP=4000 sourceCD=3000 nativeSDL=1'))
        if (-not $purchase.Success -or $purchase.Groups[1].Value -eq $purchase.Groups[2].Value) { throw 'Native normal0+normal3 purchase/provenance/CD missing.' }; $purchases+=$purchase
        foreach ($stage in @(1,2)) {
            $crack=[regex]::Match($text,('NUT_CRACK role='+$role+' stage='+$stage+' HP=(\d+) sourceMax=4000 nativeHealth=1'))
            if (-not $crack.Success) { throw ('Actual health-stage frame missing: '+$role+'/'+$stage) }
            $hp=[int]$crack.Groups[1].Value
            if (($stage -eq 1 -and ($hp -le 1333 -or $hp -ge 2666)) -or ($stage -eq 2 -and ($hp -le 0 -or $hp -ge 1333))) { throw 'Crack health violates exact source strict thresholds.' }
        }
        $done=[regex]::Match($text,('NUT_COMPLETE role='+$role+' id=(\d+) primary=(\d+) HP=(\d+) maxHP=4000 stage=2 sun=350 sourceCD=3000 threePierce=1 MC=5000 adjacent=5000 flying=5000 shootingWhileBitten=1 nativeBoard=1 secondWorld=0 plantHealthWrites=0 plantTimerWrites=0'))
        if (-not $done.Success -or $done.Groups[1].Value -ne $purchase.Groups[2].Value -or [int]$done.Groups[3].Value -le 0 -or [int]$done.Groups[3].Value -ge 1333) { throw 'Native health/controls/completion proof missing.' }; $complete+=$done
        foreach ($frame in [regex]::Matches($text,'NUT_RENDER role=\w+ frame=\w+ freshDraws=(\d+)')) { if ([int]$frame.Groups[1].Value -lt 2) { throw 'A frame did not await actual draws.' } }
    }
    for ($i=1;$i -le 3;++$i) { if ($purchases[0].Groups[$i].Value -ne $purchases[1].Groups[$i].Value) { throw 'Peer plant/packet IDs differ.' } }
    if ($complete[0].Groups[2].Value -ne $complete[1].Groups[2].Value) { throw 'Client did not observe the same primary native projectile.' }
    if ([regex]::Matches($hostText,'NUT_SETUP index=\d+ id=\d+ row=\d+ type=\d+ HP=5000[^\r\n]*staged=1 fixtureControl=1').Count -ne 6 -or
        $hostText -notmatch 'NUT_FILTER mindControlled=0 adjacentRow=0 flying=0 actualCanHit=1 nativeBalloonControlOnly=1') { throw 'Real normal/MC/adjacent/flying controls missing.' }
    $triple=[regex]::Match($hostText,'NUT_TRIPLE projectile=(\d+) targets=(\d+),(\d+),(\d+) visited=3 births=1 HP=4980,4980,4980 alive=1 damage=20 perGeneration=1')
    if (-not $triple.Success -or $triple.Groups[1].Value -ne $complete[0].Groups[2].Value -or
        @($triple.Groups[2].Value,$triple.Groups[3].Value,$triple.Groups[4].Value | Select-Object -Unique).Count -ne 3) { throw 'One real projectile did not pierce three distinct actors.' }
    for ($i=2;$i -le 4;++$i) {
        $pattern='Fusion native rolling hit id='+$triple.Groups[1].Value+' target='+$triple.Groups[$i].Value+' generation=\d+ damage=20 visited=\d+ deadProjectile=0'
        if ([regex]::Matches($hostText,$pattern).Count -ne 1) { throw 'Primary projectile repeated/omitted an actual native impact.' }
    }
    $boundary=[regex]::Match($hostText,'NUT_BOUNDARY projectile=(\d+) lastAge=(\d+) lastX=([0-9.\-]+) right=([0-9.\-]+) nextOutside=1 visitedRetired=1 overlapTicks=(\d+) nativeClock=1')
    if (-not $boundary.Success -or $boundary.Groups[1].Value -ne $triple.Groups[1].Value -or [int]$boundary.Groups[2].Value -ge 2001 -or
        [int]$boundary.Groups[5].Value -lt 3) { throw 'Natural boundary retirement/repeated-contact/history cleanup missing.' }
    $lastX=[double]::Parse($boundary.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture)
    $right=[double]::Parse($boundary.Groups[4].Value,[Globalization.CultureInfo]::InvariantCulture)
    if ($lastX -gt $right+0.02 -or $lastX+1.28 -le $right-0.02) { throw 'Primary retired before/after the source boundary.' }
    foreach ($age in @(32,33)) {
        $physics=[regex]::Match($hostText,('NUT_PHYSICS id='+$triple.Groups[1].Value+' age='+$age+' x=[0-9.\-]+ Z=([0-9.eE+\-]+) Vz=([0-9.eE+\-]+) rotation=([0-9.eE+\-]+) vx=1.28 rootColliderUnrotated=1 sourceMetricAdapter=1'))
        if (-not $physics.Success) { throw ('Actual gravity/clamp sample missing at age'+$age) }
        $z=[double]::Parse($physics.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture)
        $vz=[double]::Parse($physics.Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture)
        $rotation=[double]::Parse($physics.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture)
        if (($age -eq 32 -and ([Math]::Abs($z-(-1.12095379829)) -gt 0.03 -or [Math]::Abs($vz-1.99521017075) -gt 0.003)) -or
            ($age -eq 33 -and ($z -ne 0 -or $vz -ne 0)) -or [Math]::Abs($rotation-(-0.1884955592153876*$age)) -gt 0.01) { throw 'Native motion diverged from approved source-metric adapter.' }
    }
    $biting=[regex]::Match($hostText,'NUT_BITING_DONE HP=(\d+) stage1Shots=(\d+) stage2Shots=(\d+) positionOnlyPark=1 healthWrites=0 timerWrites=0')
    if (-not $biting.Success -or [int]$biting.Groups[1].Value -le 0 -or [int]$biting.Groups[1].Value -ge 1333 -or
        [int]$biting.Groups[2].Value -lt 1 -or [int]$biting.Groups[3].Value -lt 1 -or
        $hostText -notmatch 'NUT_EATER id=\d+[^\r\n]*newActorInitialHPOnly=1 naturalEatPlant=1 plantHP=4000 noPlantHealthWrites=1') { throw 'Real natural biter/continued shooting at both crack stages missing.' }
    $triggers=@([regex]::Matches($hostText,'NUT_TRIGGER tick=(\d+) expectedFire=(\d+) counter=(\d+) sourceDelay=75 speed=([0-9.eE+\-]+) SpeedInactive=1 nativeClock=1'))
    $shots=@([regex]::Matches($hostText,'NUT_SHOT id=(\d+) born=(\d+) age=(\d+) bullet=2 damage=20 sun=0 stage=(\d+) biting=(\d+) sourceDelay=75 nativeActor=1'))
    if ($triggers.Count -lt 3 -or $shots.Count -lt 3 -or @($shots | ForEach-Object {$_.Groups[1].Value} | Select-Object -Unique).Count -ne $shots.Count) { throw 'Unique natural native shot sequence missing.' }
    foreach ($trigger in $triggers) { if ([int]$trigger.Groups[2].Value - [int]$trigger.Groups[1].Value -ne 75 -or [int]$trigger.Groups[3].Value -gt 76) { throw 'Speed-inactive source event deadline changed.' } }
    foreach ($shot in $shots) {
        $matchedTriggers=@($triggers | Where-Object {$_.Groups[2].Value -eq $shot.Groups[2].Value})
        if ($matchedTriggers.Count -ne 1) { throw 'Actual projectile birth missed its source event.' }
    }
    foreach ($stage in @(1,2)) { if (-not @($shots | Where-Object {[int]$_.Groups[4].Value -eq $stage -and [int]$_.Groups[5].Value -eq 1}).Count) { throw 'Nut inherited an incorrect WallNut idle-stop while bitten.' } }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX); $ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $nutCommon=@($ap | Where-Object {$tx -contains $_} | Select-Object -Unique)
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hostText.IndexOf('SMOKE_FUSION_NUT_CRACK role=host stage=2')) -Marker SMOKE_SNAPSHOT_TX)
    $apPost=@(Get-SnapshotHashes -Text $clientText.Substring($clientText.IndexOf('SMOKE_FUSION_NUT_CRACK role=client stage=2')) -Marker SMOKE_SNAPSHOT_APPLIED)
    $nutPost=@($apPost | Where-Object {$txPost -contains $_} | Select-Object -Unique)
    if ($nutCommon.Count -lt 8 -or $nutPost.Count -lt 2) { throw 'Insufficient real common/post-crack snapshot hashes.' }
    $nutFrames=@((Assert-NutFrames $nutHost host $nutHostWidth),(Assert-NutFrames $nutClient client $nutClientWidth))
    if ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -ne $nutExeSha) { throw 'Executable changed during run.' }
    $nutPassed=$true
} catch { $nutError=$_.Exception.Message; if ($_.ScriptStackTrace) {$nutError+=' | '+$_.ScriptStackTrace}; Write-SmokeStatus ('FAIL Nut: '+$nutError) }
finally {
    foreach ($name in $nutEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$nutEnvironment[$name],'Process') }
    $clientStopped=Stop-OwnedProcess -Process $nutClientProcess -Role client; $hostStopped=Stop-OwnedProcess -Process $nutHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$nutPassed=$false;$nutError+=' Owned cleanup failed.'}
    Ensure-Directory $nutRoot
    $summary=[ordered]@{status=$(if ($nutPassed) {'pass'} else {'fail'});error=$nutError;fixture='FusionNativeNut';source_kind=1004;
        run_id=$nutRunId;started_utc=$nutStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$nutExeSha;
        common_snapshot_hashes=$nutCommon;post_combat_common_hashes=$nutPost;frames=$nutFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);
        native_board=$true;second_world=$false;artifacts_root=$nutRoot;visual_review='Required by Root; not inferred from nonblank BMP'}
    Write-Utf8NoBom -Path (Join-Path $nutRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($nutPassed) {exit 0} else {exit 1}
