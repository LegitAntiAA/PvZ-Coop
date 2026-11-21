#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=240,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$gloveHostWidth=$HostWindowWidth; $gloveClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $gloveHostWidth -ClientWindowWidth $gloveClientWidth
if ($HostWindowWidth -ne $gloveHostWidth -or $ClientWindowWidth -ne $gloveClientWidth) { throw 'Shared helper changed requested widths.' }
$gloveRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $gloveRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $gloveGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $gloveRepo 'work\fusion-glove' }
$gloveRunId=[Guid]::NewGuid().ToString('N'); $gloveRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$gloveRunId)
$gloveHost=Join-Path $gloveRoot 'host'; $gloveClient=Join-Path $gloveRoot 'client'
$gloveHostLog=Join-Path $gloveHost 'logs'; $gloveClientLog=Join-Path $gloveClient 'logs'
$gloveHostProcess=$null; $gloveClientProcess=$null; $glovePassed=$false; $gloveError=''
$gloveFrames=@(); $gloveCommon=@(); $glovePost=@(); $gloveExeSha=''; $gloveStarted=[DateTime]::UtcNow; $gloveEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$gloveEnvironment[$entry.Name]=$entry.Value}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO')) {
    $gloveEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Glove([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-GloveFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    foreach ($name in @('chooser','picked','intro','held','moved','twohands','gatling','cherrygatling','quad1032','quad1017','combat1032','combat1017','complete','layer_stack','layer_body','layer_cover','layer_under','layers_complete')) {
        $path=Join-Path $Directory ('fusion_glove_'+$name+'.bmp')
        Assert-Glove (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Glove ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Glove ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $gloveGame 'main.pak'),(Join-Path $gloveGame 'SDL3.dll'))) {
        Assert-Glove (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $gloveExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($gloveHostLog,$gloveClientLog,(Join-Path $gloveHost 'appdata'),(Join-Path $gloveClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $gloveEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_GLOVE='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$gloveHost} else {$gloveClient}
        $width=if ($role -eq 'host') {$gloveHostWidth} else {$gloveClientWidth}
        $nickname=if ($role -eq 'host') {'GloveHost'} else {'GloveClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native glove and quad batch: two hidden owned windows; '+$gloveRoot)
    $gloveHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gloveGame -ManifestPath (Join-Path $gloveHost 'smoke.ini') -Role host -RunId $gloveRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $gloveRoot 'pids.json') -Executable $GameExe -HostProcess $gloveHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $gloveHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $gloveHostProcess -Role host -TimeoutSeconds 120
    $gloveClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gloveGame -ManifestPath (Join-Path $gloveClient 'smoke.ini') -Role client -RunId $gloveRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $gloveRoot 'pids.json') -Executable $GameExe -HostProcess $gloveHostProcess -ClientProcess $gloveClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $gloveHost 'result.json') -ClientResultPath (Join-Path $gloveClient 'result.json') `
        -HostProcess $gloveHostProcess -ClientProcess $gloveClientProcess -HostLogDirectory $gloveHostLog -ClientLogDirectory $gloveClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Glove ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 48 -and $results[1].protocol -eq 48) 'Peer identity/protocol48 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Glove ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $gloveHostLog; $clientText=Get-CombinedLogText $gloveClientLog
    Assert-Glove ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol48 Fusion handshake missing.'
    $allPurchases=@(); $allMoves=@(); $allCrafts=@(); $allComplete=@();$allLayers=@();$allPools=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Glove ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') ('Fixture/runtime failure: '+$role)
        Assert-Glove ($text -match ('GLOVE_READY role='+$role+' nativeSDL=1 hoverObserved=1') -and
            $text -match ('GLOVE_PLAYING role='+$role+' initialGameSun=500 nativeBoard=1 secondWorld=0') -and
            $text -match ('GLOVE_BUDGET role='+$role+' fixtureInitialSun=1000 sunWritesAfterSetup=0 wavesAndSkyOnlyDeferred=1 campaignInitialSunUnchanged=500')) 'Native chooser/full intro/explicit controlled budget proof missing.'
        $pool=[regex]::Match($text,('GLOVE_POOL role='+$role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) ownFirst=1 nativeChooser=1 unfiltered=1 mappedBases=(\d+)'))
        Assert-Glove ($pool.Success -and [int]$pool.Groups[1].Value -eq 2*[int]$pool.Groups[5].Value -and [int]$pool.Groups[5].Value -ge 3 -and [int]$pool.Groups[2].Value -eq [Math]::Min(10,[int]$pool.Groups[1].Value) -and
            [int]$pool.Groups[3].Value+[int]$pool.Groups[4].Value -eq [int]$pool.Groups[2].Value) 'Canonical mapped eligible pool/capacity/own-first quota proof missing.'
        $allPools+=,$pool
        $purchases=@();$seenPurchases=New-Object 'System.Collections.Generic.HashSet[string]'
        foreach ($purchaseMatch in [regex]::Matches($text,('GLOVE_PURCHASE role='+$role+' kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) extra=(\d+) sun=(\d+) nativeSDL=1'))) {
            if ($seenPurchases.Add($purchaseMatch.Value)) { $purchases+=$purchaseMatch }
        }
        Assert-Glove ($purchases.Count -eq 5) 'Five real packet purchases were not observed by each peer.'
        $expected=@(@(0,0,0,900),@(1030,1,1,700),@(0,0,0,600),@(1030,1,1,400),@(1017,0,0,250))
        for ($i=0;$i -lt 5;++$i) {
            $p=$purchases[$i];$e=$expected[$i]
            Assert-Glove ([int]$p.Groups[1].Value -eq $e[0] -and [uint32]$p.Groups[2].Value -gt 0 -and [uint32]$p.Groups[3].Value -gt 0 -and
                [int]$p.Groups[5].Value -eq $e[1] -and [int]$p.Groups[6].Value -eq $e[2] -and [int]$p.Groups[7].Value -eq $e[3]) 'Actual purchase/owner/token/currency sequence differs.'
        }
        $allPurchases+=,@($purchases)
        $source=$purchases[1].Groups[2].Value;$token=$purchases[1].Groups[3].Value
        Assert-Glove ($text -match ('GLOVE_HELD role='+$role+' source='+$source+' token='+$token+' holder=0 owner=1 originalAlive=1 idleAdvanced=1 canonicalPreview=1 CD=0') -and
            $text -match ('GLOVE_CANCEL role='+$role+' source='+$source+' token='+$token+' actorUnchanged=1 CD=0 nativeRightClick=1')) 'Cross-owner active original/remote preview/right-click cancel proof missing.'
        foreach ($label in @('other_source','self','generation','source','target_identity','held_target')) {
            Assert-Glove ($text -match ('GLOVE_REJECT role='+$role+' case='+$label+' sender=[01] queuedAuthenticated=1 noCharge=1 holdsPreserved=1 CD=0')) ('Authenticated rejection proof missing: '+$label)
        }
        $move=[regex]::Match($text,('GLOVE_MOVED role='+$role+' id=(\d+) token=(\d+) col=3 row=2 HP=300 owner=1 parents=0,0 sameInstance=1 noInit=1 CDStarted=1 observedCD=(\d+) deltaTicks=(\d+)'))
        Assert-Glove ($move.Success -and $move.Groups[1].Value -eq $source -and $move.Groups[2].Value -eq $token -and [int]$move.Groups[3].Value -gt 0 -and [int]$move.Groups[3].Value -le 1000) 'Same-instance native move/counter preservation/cooldown proof missing.'
        $allMoves+=$move
        Assert-Glove ($text -match ('GLOVE_RECHARGED role='+$role+' normalCD=750 extraCD=750 actualReady=1 noTimerWrites=1') -and
            $text -match ('GLOVE_TWOHANDS role='+$role+' first='+$source+'/'+$token+' second='+$purchases[3].Groups[2].Value+'/'+$purchases[3].Groups[3].Value+' originalsAlive=1 globalCD=0')) 'Natural card recharge/two-player independent reservation proof missing.'
        $craft=[regex]::Match($text,('GLOVE_CRAFT role='+$role+' kind=1032 id=(\d+) token=(\d+) incomingOwner=1 operator=0 parents=1030,1030 sun=400 noCardUse=1 actualGloveUI=1'))
        Assert-Glove ($craft.Success -and $craft.Groups[1].Value -ne $source -and $craft.Groups[1].Value -ne $purchases[3].Groups[2].Value -and [uint32]$craft.Groups[2].Value -gt 0) 'Real glove fusion/ancestry/incoming-owner/native UI proof missing.'
        $allCrafts+=$craft
        foreach ($kind in @(1032,1017)) {
            $damage=if ($kind -eq 1032) {20} else {60};$front=if ($kind -eq 1032) {4840} else {4360}
            Assert-Glove ($text -match ('GLOVE_COMBAT role='+$role+' kind='+$kind+' hits=8 damage='+$damage+' front='+$front+' rear=5000 adjacent=5000')) 'Peer eight-hit/nonpierce/noAoE combat proof missing.'
        }
        $complete=[regex]::Match($text,('GLOVE_COMPLETE role='+$role+' kind=1017 id=(\d+) token=(\d+) sun=250 normalUses=2 extraUses=2 cherryUses=1 sourceCDs=750,750,5000 gloveCD=0 twoQuadKinds=1 noPlantHealthWrites=1 noPlantTimerWrites=1 nativeBoard=1 secondWorld=0'))
        Assert-Glove ($complete.Success -and $complete.Groups[1].Value -eq $purchases[4].Groups[2].Value -and $complete.Groups[2].Value -eq $purchases[4].Groups[3].Value) 'Completion/card conservation/natural global cooldown proof missing.'
        $allComplete+=$complete
        $layers=@([regex]::Matches($text,('GLOVE_LAYER_MOVED role='+$role+' index=(\d+) kind=(\d+) id=(\d+) token=(\d+) column=(\d+) row=0 HP=(\d+) owner=255 packet=0 parents=65535,65535 sameInstance=1 otherLayersIntact=1 noCharge=1 observedCD=(\d+) hostCounterProof=(\d+) nativeSDL=1')))
        Assert-Glove ($layers.Count -eq 3 -and @($layers|ForEach-Object {$_.Groups[3].Value}|Select-Object -Unique).Count -eq 3 -and @($layers|ForEach-Object {$_.Groups[4].Value}|Select-Object -Unique).Count -eq 3) 'Three actual layer actor identities missing/duplicated.'
        for($i=0;$i -lt 3;++$i){
            $m=$layers[$i];$kind=@(0,24,27)[$i];$hp=@(300,4000,300)[$i];$cap=@(1000,1000,334)[$i]
            Assert-Glove ([int]$m.Groups[1].Value -eq $i -and [int]$m.Groups[2].Value -eq $kind -and [uint32]$m.Groups[3].Value -gt 0 -and [uint32]$m.Groups[4].Value -gt 0 -and [int]$m.Groups[5].Value -eq 4+2*$i -and [int]$m.Groups[6].Value -eq $hp -and [int]$m.Groups[7].Value -gt 0 -and [int]$m.Groups[7].Value -le $cap -and [int]$m.Groups[8].Value -eq $(if($role -eq 'host'){1}else{0})) 'Native body/cover/under actor preservation/cooldown/proof mismatch.'
        }
        $allLayers+=,@($layers)
        $layerPickups=@([regex]::Matches($text,('GLOVE_LAYER_PICKUP role='+$role+' index=(\d+) kind=(\d+) id=(\d+) token=(\d+) holder=(\d+) canonicalObserved=1 snapshots=(\d+)')))
        Assert-Glove ($layerPickups.Count -eq 3) 'Each peer must observe all three canonical hands before their drops.'
        for($i=0;$i -lt 3;++$i){
            $p=$layerPickups[$i];$m=$layers[$i]
            Assert-Glove ([int]$p.Groups[1].Value -eq $i -and $p.Groups[2].Value -eq $m.Groups[2].Value -and $p.Groups[3].Value -eq $m.Groups[3].Value -and $p.Groups[4].Value -eq $m.Groups[4].Value -and [int]$p.Groups[5].Value -eq $(if($i -eq 1){1}else{0}) -and [uint32]$p.Groups[6].Value -ge 8) 'Latched canonical pickup identity/holder/snapshot proof differs from the real moved actor.'
        }
        Assert-Glove ($text -match ('GLOVE_LAYERS_COMPLETE role='+$role+' actors=3 moves=3 layers=2 bodyControl=1 sourceKinds=0,24,27 sameGenerations=1 cooldown=0 paidCrafts=0 postSetupHPOrTimerWrites=0')) 'Natural layer cooldown/full peer completion missing.'
        foreach ($m in [regex]::Matches($text,'GLOVE_RENDER role=\w+ frame=\w+ freshDraws=(\d+)')) { Assert-Glove ([int]$m.Groups[1].Value -ge 2) 'Capture did not wait for two real renderer frames.' }
    }
    for ($i=0;$i -lt 5;++$i) { for ($g=1;$g -le 7;++$g) { Assert-Glove ($allPurchases[0][$i].Groups[$g].Value -eq $allPurchases[1][$i].Groups[$g].Value) 'Peers disagree on canonical purchase identity/packet/provenance.' } }
    foreach ($pair in @(@($allMoves),@($allCrafts),@($allComplete))) { for ($g=1;$g -le 2;++$g) { Assert-Glove ($pair[0].Groups[$g].Value -eq $pair[1].Groups[$g].Value) 'Peers disagree on moved/crafted/final canonical identity.' } }
    for($g=1;$g -le 5;++$g){Assert-Glove ($allPools[0].Groups[$g].Value -eq $allPools[1].Groups[$g].Value) 'Peers disagree on actual mapped picker pool/quotas.'}
    for($i=0;$i -lt 3;++$i){foreach($g in 1..6){Assert-Glove ($allLayers[0][$i].Groups[$g].Value -eq $allLayers[1][$i].Groups[$g].Value) 'Peers disagree on moved native layer actor/token/HP/cell.'}}
    Assert-Glove ([regex]::Matches($hostText,'GLOVE_LAYER_SETUP actors=3 kinds=0,24,27 initialFactoryOnly=1 postSetupHPOrTimerWrites=0').Count -eq 1) 'Exactly one disclosed native layer fixture setup required.'
    Assert-Glove ([regex]::Matches($hostText,'GLOVE_TARGET index=\d+ id=\d+ HP=5000 type=0 boundedNativeTarget=1 initialHPOnly=1 fixtureControl=1').Count -eq 3) 'Native target setup proof missing.'
    foreach ($kind in @(1032,1017)) {
        $damage=if ($kind -eq 1032) {20} else {60};$front=if ($kind -eq 1032) {4840} else {4360}
        $triggers=@([regex]::Matches($hostText,('GLOVE_QUAD_TRIGGER kind='+$kind+' volley=(\d+) tick=(\d+) initial=(\d+) delays=(\d+),(\d+),(\d+),(\d+) speed=([0-9.eE+\-]+) stateSpeed=1 nativeClock=1')))
        $shots=@([regex]::Matches($hostText,('GLOVE_QUAD_SHOT kind='+$kind+' id=(\d+) volley=(\d+) event=(\d+) born=(\d+) trigger=(\d+) delay=(\d+) damage='+$damage+' sun=0 vx=3.2 nativeActor=1')))
        Assert-Glove ($triggers.Count -eq 3 -and $shots.Count -eq 12 -and @($shots | ForEach-Object {$_.Groups[1].Value} | Select-Object -Unique).Count -eq 12) 'Expected one target-gone plus two damaging four-shot volleys.'
        for ($v=1;$v -le 3;++$v) {
            $trigger=@($triggers | Where-Object {[int]$_.Groups[1].Value -eq $v}); Assert-Glove ($trigger.Count -eq 1) 'Duplicate/missing natural quad trigger.'; $trigger=$trigger[0]
            $speed=[single]::Parse($trigger.Groups[8].Value,[Globalization.CultureInfo]::InvariantCulture)
            Assert-Glove (-not [single]::IsNaN($speed) -and -not [single]::IsInfinity($speed) -and $speed -ge [single]0.9 -and $speed -le [single]1.1) 'Invalid source animation speed.'
            $events=@([single]0.33333334,[single]0.5,[single]0.6666667,[single]0.8333333)
            $deadlines=@($events | ForEach-Object {[int][Math]::Ceiling([double]$_/[double]$speed*100)})
            Assert-Glove ([int]$trigger.Groups[3].Value -eq $deadlines[3]+1) 'Actual trigger initial counter differs from source four-event clip.'
            for ($e=1;$e -le 4;++$e) {
                Assert-Glove ([int]$trigger.Groups[$e+3].Value -eq $deadlines[$e-1]) 'Logged quad deadline differs from independent source float32 event.'
                $shot=@($shots | Where-Object {[int]$_.Groups[2].Value -eq $v -and [int]$_.Groups[3].Value -eq $e})
                Assert-Glove ($shot.Count -eq 1 -and [int]$shot[0].Groups[5].Value -eq [int]$trigger.Groups[2].Value -and
                    [int]$shot[0].Groups[6].Value -eq $deadlines[$e-1] -and [int]$shot[0].Groups[4].Value -eq [int]$trigger.Groups[2].Value+$deadlines[$e-1]) 'Actual projectile event is early/late/duplicated/canceled.'
            }
        }
        Assert-Glove ($hostText -match ('GLOVE_TARGET_GONE kind='+$kind+' afterTrigger=1 positionOnly=1 remainingEventsMustFire=4') -and
            $hostText -match ('GLOVE_QUAD_UNCANCELED kind='+$kind+' events=4 retiredNaturally=1 hits=0 forcedClock=0') -and
            $hostText -match ('GLOVE_QUAD_DAMAGE kind='+$kind+' hits=8 damage='+$damage+' front='+$front+' rear=5000 adjacent=5000 noSun=1 noAoE=1 noPierce=1 healthWrites=0')) 'No-early-cancel/natural retirement/eight-hit isolation proof missing.'
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $gloveCommon=@($ap | Where-Object {$tx -contains $_} | Select-Object -Unique)
    $hostPost=$hostText.IndexOf('SMOKE_FUSION_GLOVE_COMBAT role=host kind=1017');$clientPost=$clientText.IndexOf('SMOKE_FUSION_GLOVE_COMBAT role=client kind=1017')
    Assert-Glove ($hostPost -ge 0 -and $clientPost -ge 0) 'Missing actual final combat barrier.'
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hostPost) -Marker SMOKE_SNAPSHOT_TX);$apPost=@(Get-SnapshotHashes -Text $clientText.Substring($clientPost) -Marker SMOKE_SNAPSHOT_APPLIED)
    $glovePost=@($apPost | Where-Object {$txPost -contains $_} | Select-Object -Unique)
    Assert-Glove ($gloveCommon.Count -ge 8 -and $glovePost.Count -ge 2) 'Insufficient actual common/post-combat snapshot hashes.'
    $gloveFrames=@((Assert-GloveFrames $gloveHost host $gloveHostWidth),(Assert-GloveFrames $gloveClient client $gloveClientWidth))
    Assert-Glove ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $gloveExeSha) 'Executable changed during owned run.'
    $glovePassed=$true
} catch { $gloveError=$_.Exception.Message;if ($_.ScriptStackTrace) {$gloveError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL glove: '+$gloveError) }
finally {
    foreach ($name in $gloveEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$gloveEnvironment[$name],'Process') }
    $clientStopped=Stop-OwnedProcess -Process $gloveClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $gloveHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$glovePassed=$false;$gloveError+=' Owned cleanup failed.'}
    Ensure-Directory $gloveRoot
    $summary=[ordered]@{status=$(if ($glovePassed) {'pass'} else {'fail'});error=$gloveError;fixture='FusionNativeGloveQuad';source_kinds=@(1032,1017);
        run_id=$gloveRunId;started_utc=$gloveStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$gloveExeSha;
        common_snapshot_hashes=$gloveCommon;post_combat_common_hashes=$glovePost;frames=$gloveFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);
        fixture_initial_sun=1000;campaign_initial_sun=500;sun_writes_after_setup=0;native_board=$true;second_world=$false;artifacts_root=$gloveRoot;
        protocol=48;native_schema=13;native_layer_kinds=@(0,24,27);native_layer_moves=3;layer_paid_crafts=0;
        limits='Native normal-zombie controls; full source zombie AI/source animation parity not covered. Layer slice uses one controlled three-actor factory setup and actual SDL body/cover/under moves with natural cooldowns. Host proves actual native counter preservation; client proves opaque identity/source metadata, not unsent launch counters. Paid layer crafting/equipment/recipient abilities are not covered.';visual_review='Required by Root; not inferred from nonblank BMP'}
    Write-Utf8NoBom -Path (Join-Path $gloveRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($glovePassed) {exit 0} else {exit 1}
