#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=480,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$coldHostWidth=$HostWindowWidth; $coldClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $coldHostWidth -ClientWindowWidth $coldClientWidth
if ($HostWindowWidth -ne $coldHostWidth -or $ClientWindowWidth -ne $coldClientWidth) { throw 'Shared helper changed requested widths.' }
$coldRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $coldRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $coldGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $coldRepo 'work\fusion-cold' }
$coldRunId=[Guid]::NewGuid().ToString('N'); $coldRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$coldRunId)
$coldHost=Join-Path $coldRoot 'host'; $coldClient=Join-Path $coldRoot 'client'
$coldHostLog=Join-Path $coldHost 'logs'; $coldClientLog=Join-Path $coldClient 'logs'
$coldHostProcess=$null; $coldClientProcess=$null; $coldPassed=$false; $coldError=''
$coldFrames=@(); $coldCommon=@(); $coldPost=@(); $coldExeSha=''; $coldStarted=[DateTime]::UtcNow; $coldEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_PRODUCER','PVZ_COOP_SMOKE_FUSION_COLD','PVZ_COOP_SMOKE_FUSION_SPLIT','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $coldEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Cold([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-ColdFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $required=@('chooser','picked','intro','playing','global','model1034','combat1034','model1105','combat1105','model1106','paused','combat1106','model1035','combat1035','model908','combat908','complete')
    if ($Role -eq 'client') { $required+=@('projectile1034','projectile1105','projectile1106','projectile1035','projectile908') }
    foreach ($name in $required) {
        $path=Join-Path $Directory ('fusion_cold_'+$name+'.bmp')
        Assert-Cold (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Cold ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Cold ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $coldGame 'main.pak'),(Join-Path $coldGame 'SDL3.dll'))) {
        Assert-Cold (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $coldExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($coldHostLog,$coldClientLog,(Join-Path $coldHost 'appdata'),(Join-Path $coldClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $coldEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_COLD='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$coldHost} else {$coldClient}
        $width=if ($role -eq 'host') {$coldHostWidth} else {$coldClientWidth}
        $nickname=if ($role -eq 'host') {'ColdHost'} else {'ColdClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native cold family batch: two hidden owned windows; '+$coldRoot)
    $coldHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $coldGame -ManifestPath (Join-Path $coldHost 'smoke.ini') -Role host -RunId $coldRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $coldRoot 'pids.json') -Executable $GameExe -HostProcess $coldHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $coldHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $coldHostProcess -Role host -TimeoutSeconds 120
    $coldClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $coldGame -ManifestPath (Join-Path $coldClient 'smoke.ini') -Role client -RunId $coldRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $coldRoot 'pids.json') -Executable $GameExe -HostProcess $coldHostProcess -ClientProcess $coldClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $coldHost 'result.json') -ClientResultPath (Join-Path $coldClient 'result.json') `
        -HostProcess $coldHostProcess -ClientProcess $coldClientProcess -HostLogDirectory $coldHostLog -ClientLogDirectory $coldClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Cold ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 44 -and $results[1].protocol -eq 44) 'Peer identity/protocol44 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Cold ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $coldHostLog;$clientText=Get-CombinedLogText $coldClientLog
    Assert-Cold ($hostText -match 'HELLO slot=1 protocol=44 expected=44' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol44/Fusion handshake missing.'
    $purchaseSets=@();$mixSets=@()
    $expectedKinds=@(10,0,1034,1105,0,1030,6,1018,1035,6,1018,1019,0,1030,908)
    $expectedSun=@(1850,1750,1675,1575,1475,1275,1275,1175,1025,1025,925,825,725,525,450)
    $expectedOwners=@(1,0,0,0,0,1,0,0,1,0,0,0,0,1,0)
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Cold ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') ('Runtime failure: '+$role)
        Assert-Cold ($text -match ('COLD_READY role='+$role+' nativeSDL=1 hoverObserved=1') -and $text -match ('COLD_POOL role='+$role+' eligible=20 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1 unfiltered=1')) 'Actual shared chooser/own-first/Ready missing.'
        Assert-Cold ($text -match ('COLD_BUDGET role='+$role+' campaignInitialSun=500 fixtureInitialSun=2000 purchaseTotal=1550 finalExpected=450 setupAmbientTimersOnly=1 sunWritesAfterSetup=0')) 'One initial fixture budget/golden missing.'
        $buys=@([regex]::Matches($text,('COLD_PURCHASE role='+$role+' step=(\d+) kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) extra=(\d+) parents=(\d+),(\d+) sun=(\d+) awake=1 HP=300 nativeSDL=1')))
        Assert-Cold ($buys.Count -eq 15) 'Expected exactly fifteen real card purchases.'
        for ($i=0;$i -lt 15;++$i) {
            Assert-Cold ([int]$buys[$i].Groups[2].Value -eq $expectedKinds[$i] -and [int]$buys[$i].Groups[10].Value -eq $expectedSun[$i] -and [int]$buys[$i].Groups[6].Value -eq $expectedOwners[$i] -and [uint32]$buys[$i].Groups[4].Value -gt 0) 'Actual directed purchase kind/owner/budget/token differs.'
        }
        $purchaseSets+=,@($buys)
        $mix=@([regex]::Matches($text,('COLD_MIX role='+$role+' kind=(\d+) id=(\d+) token=(\d+) parents=(\d+),(\d+) owner=1 packet=(\d+) sun=(\d+) actualSDL=1 noAdditionalCharge=1')))
        Assert-Cold ($mix.Count -eq 2 -and $mix[0].Groups[1].Value -eq '1106' -and $mix[0].Groups[4].Value -eq '1105' -and $mix[0].Groups[5].Value -eq '1030' -and $mix[0].Groups[7].Value -eq '1275' -and
            $mix[1].Groups[1].Value -eq '1065' -and $mix[1].Groups[4].Value -eq '1019' -and $mix[1].Groups[5].Value -eq '1030' -and $mix[1].Groups[7].Value -eq '525') 'Two actual directed glove recipes/provenance missing.'
        $mixSets+=,@($mix)
        Assert-Cold ($text -match ('COLD_GLOBAL role='+$role+' plant=\d+ normalTargets=3 normalAll20=1 globalAcrossRowsAndOffscreen=1 freeze400=1 slow1400=1 MCunchanged=1 boardAura=0') -and
            $text -match ('COLD_NOAURA role='+$role+' newActor=\d+ nativeCounters=0 sourceStateAbsent=1 boardAura=0 postEffectSpawn=1')) 'Source10 global/future-spawn distinction missing.'
        Assert-Cold ($text -match ('COLD_COMPLETE role='+$role+' sourceKinds=6 hybridShooters=5 initialSun=2000 totalCost=1550 finalSun=450 uses=7,2,2,2,2 sunWritesAfterSetup=0 plantHPWrites=0 plantTimerWrites=0 statusWrites=0 thresholdWrites=0 nativeBoard=1 secondWorld=0 sourceZombieAIUnported=1')) 'Native complete conservation/bounds missing.'
        foreach ($m in [regex]::Matches($text,'COLD_RENDER role=\w+ frame=\w+ freshDraws=(\d+)')) {Assert-Cold ([int]$m.Groups[1].Value -ge 2) 'Capture did not await real fresh renderer frames.'}
    }
    for ($i=0;$i -lt 15;++$i) {for ($g=1;$g -le 10;++$g) {Assert-Cold ($purchaseSets[0][$i].Groups[$g].Value -eq $purchaseSets[1][$i].Groups[$g].Value) 'Canonical purchase/packet/owner/ancestry differs between peers.'}}
    for ($i=0;$i -lt 2;++$i) {for ($g=1;$g -le 7;++$g) {Assert-Cold ($mixSets[0][$i].Groups[$g].Value -eq $mixSets[1][$i].Groups[$g].Value) 'Canonical glove recipe/provenance differs between peers.'}}
    $fuse=[regex]::Match($hostText,'COLD_FUSE role=host id=(\d+) token=(\d+) firstTick=(\d+) firstRemaining=(\d+) retireTick=(\d+) elapsed=(\d+) sourceFuse=151 biteSamples=(\d+) invulnerableHP300=1')
    Assert-Cold ($fuse.Success -and [int]$fuse.Groups[4].Value -gt 0 -and [int]$fuse.Groups[4].Value -le 151 -and [int]$fuse.Groups[6].Value -ge [int]$fuse.Groups[4].Value -and [int]$fuse.Groups[6].Value -le [int]$fuse.Groups[4].Value+2 -and [int]$fuse.Groups[7].Value -gt 0 -and
        $hostText -match ('Fusion native IceShroom init id=\d+ token='+$fuse.Groups[2].Value+' fuse=151 tick=\d+')) 'Natural151 fuse/actual bite/invulnerability source proof missing.'
    foreach ($kind in @(1034,1105,1106,1035,908)) {
        $events=@(if ($kind -eq 1034) {[single]0.6666667} elseif ($kind -eq 1105) {[single]0.53333336;[single]1.0666667} elseif ($kind -eq 1106) {[single]0.33333334;[single]0.5;[single]0.6666667;[single]0.8333333} elseif ($kind -eq 1035) {[single]0.38333333} else {[single]0.5;[single]0.7;[single]0.9;[single]1.1})
        $volleyCount=if ($kind -eq 1106) {4} else {2};$payload=if ($kind -eq 1035 -or $kind -eq 908) {18} else {15};$perShot=if ($payload -eq 18) {3} else {1}
        $stateSpeed=if ($kind -eq 1034 -or $kind -eq 1105) {2.0} elseif ($kind -eq 908) {[double][single]1.3} else {1.0};$visual=if ($kind -eq 908) {'shoot_g'} else {'shoot'}
        $triggers=@([regex]::Matches($hostText,('COLD_TRIGGER kind='+$kind+' volley=(\d+) tick=(\d+) initial=(\d+) deadlines=([0-9,]+) speed=([0-9.eE+\-]+) stateSpeed=([0-9.eE+\-]+) eventCount=(\d+) visualState='+$visual+' naturalClock=1')))
        $shots=@([regex]::Matches($hostText,('COLD_SHOT kind='+$kind+' id=(\d+) volley=(\d+) event=(\d+) fire=(\d+) trigger=(\d+) deadline=(\d+) payload='+$payload+' damage=20 special=0 motion=0 age=(\d+) vx=3.2 sourceBox=(\d+),(\d+) sun=0')))
        Assert-Cold ($triggers.Count -eq $volleyCount -and $shots.Count -eq $volleyCount*$events.Count -and @($shots|ForEach-Object {$_.Groups[1].Value}|Select-Object -Unique).Count -eq $shots.Count) 'Missing/duplicate natural cold physical emissions.'
        foreach ($trigger in $triggers) {
            $speed=[single]::Parse($trigger.Groups[5].Value,[Globalization.CultureInfo]::InvariantCulture)
            Assert-Cold ($speed -ge [single]0.9 -and $speed -le [single]1.1 -and [double]::Parse($trigger.Groups[6].Value,[Globalization.CultureInfo]::InvariantCulture) -eq $stateSpeed -and [int]$trigger.Groups[7].Value -eq $events.Count) 'Cold state speed/active-Speed/event count differs.'
            $deadlines=@($events|ForEach-Object {[int][Math]::Ceiling([double]$_/($stateSpeed*[double]$speed)*100)})
            Assert-Cold ($trigger.Groups[4].Value -eq ($deadlines -join ',') -and [int]$trigger.Groups[3].Value -eq $deadlines[-1]+1) 'Independent source event deadlines differ.'
            for ($e=0;$e -lt $events.Count;++$e) {
                $found=@($shots|Where-Object {$_.Groups[2].Value -eq $trigger.Groups[1].Value -and [int]$_.Groups[3].Value -eq $e+1})
                Assert-Cold ($found.Count -eq 1 -and [int]$found[0].Groups[4].Value -eq [int]$trigger.Groups[2].Value+$deadlines[$e] -and $found[0].Groups[5].Value -eq $trigger.Groups[2].Value -and [int]$found[0].Groups[6].Value -eq $deadlines[$e]) 'Cold shot is early/late/duplicated relative to native counter.'
            }
        }
        foreach ($shot in $shots) {
            $id=$shot.Groups[1].Value
            Assert-Cold ($clientText -match ('COLD_SEEN role=client kind='+$kind+' id='+$id+' payload='+$payload+' damage=20 special=0 motion=0 rect=\d+,\d+ hostAgeVelocityNotSerialized=1')) ('Client missed long-path canonical cold ID '+$id+' for kind '+$kind)
            if ($payload -eq 18) {
                $hits=@([regex]::Matches($hostText,('Fusion native cold pierce id='+$id+' target=(\d+) generation=(\d+) visited=(\d+) damage=(\d+)')))
                Assert-Cold ($hits.Count -eq 3 -and @($hits|ForEach-Object {$_.Groups[1].Value}|Select-Object -Unique).Count -eq 3 -and @($hits|ForEach-Object {$_.Groups[2].Value}|Select-Object -Unique).Count -eq 3 -and ((@($hits|ForEach-Object {$_.Groups[3].Value})) -join ',') -eq '1,2,3') 'IceSpark must hit exactly three distinct native collider generations.'
            }
        }
        $expectedHits=$shots.Count*$perShot
        Assert-Cold ([regex]::Matches($hostText,('COLD_HIT kind='+$kind+' target=\d+ payload='+$payload+' ')).Count -eq $expectedHits) 'Actual observed native hit ledger differs.'
        foreach ($role in @('host','client')) {
            $text=if ($role -eq 'host') {$hostText} else {$clientText}
            Assert-Cold ($text -match ('COLD_COMBAT role='+$role+' kind='+$kind+' volleys='+$volleyCount+' physical='+$shots.Count+' hits='+$expectedHits+' payload='+$payload+' uniqueLimit='+$perShot+' noFourth=1 controlsUnchanged=1 noSun=1 naturalAcquisition=1')) 'Actual cold kind did not finish bounded combat.'
        }
        $hostFinal=@([regex]::Matches($hostText,('COLD_FINAL role=host kind='+$kind+' index=(\d+) id=(\d+) body=(\d+) helm=(\d+) shield=(\d+) level=(\d+) threshold=(\d+)')))
        $clientFinal=@([regex]::Matches($clientText,('COLD_FINAL role=client kind='+$kind+' index=(\d+) id=(\d+) body=(\d+) helm=(\d+) shield=(\d+) level=(\d+) threshold=(\d+)')))
        Assert-Cold ($hostFinal.Count -eq 7 -and $clientFinal.Count -eq 7) 'Seven canonical cold controls absent.'
        for ($i=0;$i -lt 7;++$i) {for ($g=1;$g -le 7;++$g) {Assert-Cold ($hostFinal[$i].Groups[$g].Value -eq $clientFinal[$i].Groups[$g].Value) 'Peer cold identity/armor/level/threshold final differs.'}}
    }
    Assert-Cold ($hostText -match 'COLD_THRESHOLD kind=1106 target=\d+ level=0 threshold=200 slow=(?:1000|99[0-9]) freeze=(?:400|39[0-9]) damage=80 beforeDamageFreeze=1') 'Live source threshold-before-damage freeze/80 damage missing.'
    Assert-Cold ($hostText -match 'COLD_THAW role=host kind=1106 target=\d+ level=0 threshold=200 slow=\d+ naturalExpiry=1') 'Natural thaw did not retain threshold200/reset level.'
    $request=[regex]::Match($hostText,'COLD_PAUSE_REQUEST remaining=([1-5]) tick=\d+ ownedSDL=1 nativeOptions=1 counterWrites=0');Assert-Cold $request.Success 'Missing natural pre-thaw native options interaction.'
    $paused=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText};$m=[regex]::Match($text,('COLD_PAUSED role='+$role+' tick=(\d+) slow=(\d+) freeze=([1-5]) uiChecks=(\d+) elapsedMs=(\d+) exactStable=1 decoderLive=1'))
        Assert-Cold ($m.Success -and [int]$m.Groups[4].Value -ge 10 -and [int]$m.Groups[5].Value -ge 1200) 'Paused ghost/native counters were not stable across real UI/network updates.';$paused+=$m
    }
    Assert-Cold ($paused[0].Groups[2].Value -eq $paused[1].Groups[2].Value -and $paused[0].Groups[3].Value -eq $paused[1].Groups[3].Value) 'Paused host/client exact cold counters differ.'
    Assert-Cold ($clientText -match 'COLD_NEGATIVE role=client target=\d+ FNS10=1 crcValid=1 nativeCounterMismatch=1 rejected=1 wholeBoardBytesUnchanged=1 sourceOriginalUntouched=1' -and
        $clientText -match 'COLD_EXACT role=client target=\d+ level=\d+ threshold=\d+ slow=\d+ freeze=\d+ nativeCountersEqual=1 actualHostBytes=1 controlledReplay=1 lateJoinTested=0') 'Actual Board preflight rollback/exact FNS10 restoration proof missing.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$coldCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_FUSION_COLD_COMBAT role=host kind=908');$ci=$clientText.IndexOf('SMOKE_FUSION_COLD_COMBAT role=client kind=908');Assert-Cold ($hi -ge 0 -and $ci -ge 0) 'Final cold combat barrier missing.'
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$apPost=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED);$coldPost=@($apPost|Where-Object {$txPost -contains $_}|Select-Object -Unique)
    Assert-Cold ($coldCommon.Count -ge 8 -and $coldPost.Count -ge 2) 'Insufficient shared/postaction authoritative hashes.'
    $coldFrames=@((Assert-ColdFrames $coldHost host $coldHostWidth),(Assert-ColdFrames $coldClient client $coldClientWidth))
    Assert-Cold ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $coldExeSha) 'Executable changed during owned run.';$coldPassed=$true
} catch {$coldError=$_.Exception.Message;if ($_.ScriptStackTrace) {$coldError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Cold: '+$coldError)}
finally {
    foreach ($name in $coldEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$coldEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $coldClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $coldHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$coldPassed=$false;$coldError+=' Owned cleanup failed.'}
    Ensure-Directory $coldRoot;$summary=[ordered]@{status=$(if ($coldPassed) {'pass'} else {'fail'});error=$coldError;fixture='FusionNativeCold';source_kinds=@(10,1034,1105,1106,1035,908);
        run_id=$coldRunId;started_utc=$coldStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$coldExeSha;
        common_snapshot_hashes=$coldCommon;postaction_common_hashes=$coldPost;frames=$coldFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$coldRoot;
        campaign_initial_sun=500;fixture_initial_sun=2000;purchase_total=1550;final_sun=450;sun_writes_after_setup=0;native_board=$true;second_world=$false;
        limits='Controlled native NORMAL actors, stationary native ground pose; no source zombie AI/movement parity. Host proves precise velocity/age/event/cold impacts. Client proves canonical projectile IDs/outcomes and exact FNS counters via one controlled real-host-byte paused replay, not a real late join. Travel/IceBean/fire/aura/particle families deferred.';
        visual_review='Root reviews actual frames; nonblank BMP alone is not visual acceptance'}
    Write-Utf8NoBom -Path (Join-Path $coldRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($coldPassed) {exit 0} else {exit 1}
