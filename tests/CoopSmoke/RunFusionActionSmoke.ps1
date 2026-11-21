#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(180,3600)][int]$TimeoutSeconds=360,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$actionHostWidth=$HostWindowWidth;$actionClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $actionHostWidth -ClientWindowWidth $actionClientWidth
if($HostWindowWidth -ne $actionHostWidth -or $ClientWindowWidth -ne $actionClientWidth){throw 'Shared helper changed requested widths.'}
$actionRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $actionRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$actionGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $actionRepo 'work\fusion-action'}
$actionRunId=[Guid]::NewGuid().ToString('N');$actionRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$actionRunId)
$actionHost=Join-Path $actionRoot 'host';$actionClient=Join-Path $actionRoot 'client'
$actionHostLog=Join-Path $actionHost 'logs';$actionClientLog=Join-Path $actionClient 'logs'
$actionHostProcess=$null;$actionClientProcess=$null;$actionPassed=$false;$actionError='';$actionExeSha='';$actionFrames=@();$actionCommon=@();$actionByEpoch=@()
$actionStarted=[DateTime]::UtcNow;$actionEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$actionEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_ACTION','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $actionEnvironment.ContainsKey($name)){$actionEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}}
$actionKinds=@(8,1022,11,16,13,1057,17,1155,14,1058,9,1024,1025)
$actionGroups=@(@(8,1022,1024),@(13,1057,17,1155),@(11,16),@(9,1024,1025,17,17),@(14,18,18,18))
$actionHp=@(@(800,800,800),@(5000,5000,1000,1000),@(3000,3000,3000,3000),@(60,3000,60,3000,20,3000),@(5000,5000,5000))
function Assert-Action([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Assert-ActionFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    $frames=@();for($g=0;$g -lt 5;++$g){foreach($suffix in @('before','battle','after')){$frames+=($g.ToString()+'_'+$suffix)}}
    $frames+=@('0_hypno','1_stomp13','1_stomp1057','1_melee','2_instant','3_fear9','3_fear1024','3_close1025','3_ranged','3_projectiles','4_three14','4_entry1058','4_three1058','4_projectiles')
    foreach($name in $frames){
        $path=Join-Path $Directory ('fusion_action_'+$name+'.bmp');Assert-Action (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual source family frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try{
            Assert-Action ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual native viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for($x=16;$x -lt $bitmap.Width;$x+=37){for($y=16;$y -lt $bitmap.Height;$y+=31){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
            Assert-Action ($colors.Count -ge 8) 'Blank native renderer frame.'
        }finally{$bitmap.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
function Get-ActionRows([string]$Text,[string]$Role,[int]$Group,[string]$When){
    $pattern=if($When -eq 'before'){
        'ACTION_NPC role='+$Role+' group='+$Group+' epoch=(\d+) when=before index=(\d+) id=(\d+) body=(\d+) max=(\d+) helm=(\d+) helmMax=(\d+) shield=(\d+) shieldMax=(\d+) scale=([0-9.eE+\-]+) controlled=(\d+)'
    }else{
        'ACTION_NPC role='+$Role+' group='+$Group+' epoch=(\d+) when=after index=(\d+) id=(\d+) alive=(\d+) body=(-?\d+) max=(\d+) helm=(-?\d+) helmMax=(\d+) shield=(-?\d+) shieldMax=(\d+) scale=([0-9.eE+\-]+) controlled=(\d+) mask=(\d+)'
    }
    return @([regex]::Matches($Text,$pattern))
}
try{
    foreach($path in @($GameExe,(Join-Path $actionGame 'main.pak'),(Join-Path $actionGame 'SDL3.dll'))){Assert-Action (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)}
    $actionExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($path in @($actionHostLog,$actionClientLog,(Join-Path $actionHost 'appdata'),(Join-Path $actionClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort}
    foreach($name in $actionEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_ACTION='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$actionHost}else{$actionClient};$width=if($role -eq 'host'){$actionHostWidth}else{$actionClientWidth};$nick=if($role -eq 'host'){'ActionHost'}else{'ActionClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nick -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nick -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Grouped native Actions:5 isolated native activities,13 source kinds; '+$actionRoot)
    $actionHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $actionGame -ManifestPath (Join-Path $actionHost 'smoke.ini') -Role host -RunId $actionRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $actionRoot 'pids.json') -Executable $GameExe -HostProcess $actionHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $actionHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $actionHostProcess -Role host -TimeoutSeconds 120
    $actionClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $actionGame -ManifestPath (Join-Path $actionClient 'smoke.ini') -Role client -RunId $actionRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $actionRoot 'pids.json') -Executable $GameExe -HostProcess $actionHostProcess -ClientProcess $actionClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $actionHost 'result.json') -ClientResultPath (Join-Path $actionClient 'result.json') -HostProcess $actionHostProcess -ClientProcess $actionClientProcess -HostLogDirectory $actionHostLog -ClientLogDirectory $actionClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Action ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Paired native results missing.'
    foreach($result in $results){$samples=if($result.role -eq 'host'){[int]$result.snapshotsSent}else{[int]$result.snapshotsApplied};Assert-Action ($result.status -eq 'pass' -and [int]$result.protocol -eq 48 -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Strict native protocol48/full intro manifest failed.'}
    $hostText=Get-CombinedLogText $actionHostLog;$clientText=Get-CombinedLogText $actionClientLog
    Assert-Action ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Real Fusion/protocol48 handshake missing.'
    $epochRows=@();$plantRows=@();$beforeRows=@();$afterRows=@();$paidRows=@();$childShots=@()
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){
        $role=[string]$pair[0];$text=[string]$pair[1]
        Assert-Action ($text -notmatch 'SMOKE_FAIL|seq-gap|invalid snapshot|secondWorld=1') ('Actual runtime error in '+$role)
        $epochs=@([regex]::Matches($text,('ACTION_EPOCH role='+$role+' group=(\d+) epoch=(\d+) previous=(\d+)')));Assert-Action ($epochs.Count -eq 5) 'Exactly5 new epoch observations required.'
        $rolePlants=@();$roleBefore=@();$roleAfter=@()
        for($g=0;$g -lt 5;++$g){
            Assert-Action ([int]$epochs[$g].Groups[1].Value -eq $g -and [uint32]$epochs[$g].Groups[2].Value -gt [uint32]$epochs[$g].Groups[3].Value -and ($g -eq 0 -or $epochs[$g].Groups[3].Value -eq $epochs[$g-1].Groups[2].Value)) 'Old or out-of-order native restart epoch.'
            $epoch=$epochs[$g].Groups[2].Value
            Assert-Action ($text -match ('ACTION_CASE role='+$role+' group='+$g+' epoch='+$epoch+' nativeBattle=1 initialSetupOnly=1')) 'Family battle did not complete on each peer.'
            $plants=@([regex]::Matches($text,('ACTION_PLANT role='+$role+' group='+$g+' epoch='+$epoch+' index=(\d+) kind=(\d+) id=(\d+) token=(\d+) alias=(\d+) HP=(\d+) owner=255 packet=0 parents=(\d+),(\d+)')))
            Assert-Action ($plants.Count -eq $actionGroups[$g].Count) 'Actual per-kind source/native metadata missing.'
            for($i=0;$i -lt $plants.Count;++$i){Assert-Action ([int]$plants[$i].Groups[1].Value -eq $i -and [int]$plants[$i].Groups[2].Value -eq $actionGroups[$g][$i] -and [uint32]$plants[$i].Groups[3].Value -gt 0 -and [uint32]$plants[$i].Groups[4].Value -gt 0 -and [int]$plants[$i].Groups[6].Value -eq 300) 'Source plant kind/token/HP differs.'}
            $before=@(Get-ActionRows $text $role $g before);$after=@(Get-ActionRows $text $role $g after)
            Assert-Action ($before.Count -eq $actionHp[$g].Count -and $after.Count -eq $before.Count) 'Before/after actual NPC records missing or duplicated.'
            for($i=0;$i -lt $before.Count;++$i){
                $b=$before[$i];$z=$after[$i];$helm=if($g -eq 0 -and $i -eq 1){400}else{0};$shield=if($g -eq 0 -and $i -eq 1){600}else{0}
                Assert-Action ([int]$b.Groups[2].Value -eq $i -and [int]$b.Groups[4].Value -eq $actionHp[$g][$i] -and [int]$b.Groups[5].Value -eq $actionHp[$g][$i] -and [int]$b.Groups[6].Value -eq $helm -and [int]$b.Groups[7].Value -eq $helm -and [int]$b.Groups[8].Value -eq $shield -and [int]$b.Groups[9].Value -eq $shield -and [double]$b.Groups[10].Value -eq 1 -and [int]$b.Groups[11].Value -eq 0 -and $z.Groups[3].Value -eq $b.Groups[3].Value) 'Actual initial normal target body/max/armour/scale or identity differs.'
            }
            $rolePlants+=,@($plants);$roleBefore+=,@($before);$roleAfter+=,@($after)
        }
        $epochRows+=,@($epochs);$plantRows+=,@($rolePlants);$beforeRows+=,@($roleBefore);$afterRows+=,@($roleAfter)
        foreach($z in $roleAfter[0]){Assert-Action ([int]$z.Groups[4].Value -eq 1 -and [int]$z.Groups[12].Value -eq 1) 'Actual bite hypnosis missing.'}
        $shrink=$roleAfter[0][1];Assert-Action ([int]$shrink.Groups[5].Value -eq 400 -and [int]$shrink.Groups[6].Value -eq 400 -and [int]$shrink.Groups[7].Value -eq 200 -and [int]$shrink.Groups[8].Value -eq 200 -and [int]$shrink.Groups[9].Value -eq 300 -and [int]$shrink.Groups[10].Value -eq 300 -and [double]$shrink.Groups[11].Value -eq .5) 'HypnoPuff exact after body/max/armour/scale not restored.'
        for($i=0;$i -lt 4;++$i){$hp=[int]$roleAfter[1][$i].Groups[5].Value;Assert-Action ([int]$roleAfter[1][$i].Groups[4].Value -eq 1 -and $(if($i -lt 2){$hp -eq 3200}else{$hp -gt 0 -and $hp -le 960 -and (1000-$hp)%20 -eq 0})) 'Native Squash1800/repeatedSpike20 outcome differs.'}
        for($i=0;$i -lt 4;++$i){Assert-Action ([int]$roleAfter[2][$i].Groups[5].Value -eq $(if($i -eq 3){3000}else{1200})) 'Actual Doom/Jalapeno strong/outside target HP differs.'}
        $ranged=[regex]::Match($text,('ACTION_RANGED role='+$role+' fear9=1 recovered9=1 fear1024=1 recovered1024=1 colorMask=(\d+) fullSeven=[01] close1025=1 closeDead=1 farDoubleShots=(\d+) rareSevenRequired=0'))
        Assert-Action ($ranged.Success -and [int]$ranged.Groups[1].Value -ge 1 -and [int]$ranged.Groups[1].Value -le 127 -and [int]$ranged.Groups[2].Value -ge 2) 'Natural fear/recovery/color mask/close fume/far double missing.'
        $paid=[regex]::Match($text,('ACTION_PAID_ENTRY role='+$role+' epoch=(\d+) kind=1058 id=(\d+) token=(\d+) parents=14,16 owner=0 packet=(\d+) cost=125 sun=375 sourceCooldown=5000 nativeSDL=1 hostDamageProof=([01])'))
        Assert-Action ($paid.Success -and [uint32]$paid.Groups[2].Value -gt 0 -and [uint32]$paid.Groups[3].Value -gt 0 -and [int]$paid.Groups[5].Value -eq $(if($role -eq 'host'){1}else{0})) 'Actual paid14+16 entry/UID/price/cooldown missing.';$paidRows+=,$paid
        $shots=@([regex]::Matches($text,('ACTION_CHILD_SHOT role='+$role+' epoch='+$paid.Groups[1].Value+' child='+$paid.Groups[2].Value+' projectile=(\d+) phase=(0|3|4) hot=1 firstObservedAfterEntry=1')));$childShots+=,@($shots)
        foreach($phase in @(0,3,4)){Assert-Action (@($shots|Where-Object {[int]$_.Groups[2].Value -eq $phase}).Count -ge 1) 'Fresh1058 must actually fire each heated Three lane.'}
        Assert-Action ($text -match ('ACTION_COMPLETE role='+$role+' groups=5 sourceKinds=8,1022,11,16,13,1057,17,1155,14,1058,9,1024,1025 paidEntry1058=1 rareSevenColorControl=NotRequired sourceZombieAIUnported=1 postSetupActorWrites=0 nativeBoard=1 secondWorld=0')) 'Strict grouped action completion missing.'
    }
    for($g=0;$g -lt 5;++$g){
        Assert-Action ($epochRows[0][$g].Groups[2].Value -eq $epochRows[1][$g].Groups[2].Value) 'Peer activity epochs differ.'
        for($i=0;$i -lt $plantRows[0][$g].Count;++$i){foreach($j in 1..8){Assert-Action ($plantRows[0][$g][$i].Groups[$j].Value -eq $plantRows[1][$g][$i].Groups[$j].Value) 'Peer initial source plant identity/metadata differs.'}}
        for($i=0;$i -lt $beforeRows[0][$g].Count;++$i){foreach($j in 1..11){Assert-Action ($beforeRows[0][$g][$i].Groups[$j].Value -eq $beforeRows[1][$g][$i].Groups[$j].Value) 'Peer before-state native NPC differs.'};foreach($j in 1..13){Assert-Action ($afterRows[0][$g][$i].Groups[$j].Value -eq $afterRows[1][$g][$i].Groups[$j].Value) 'Peer paused after-state native NPC differs.'}}
    }
    foreach($j in 1..4){Assert-Action ($paidRows[0].Groups[$j].Value -eq $paidRows[1].Groups[$j].Value) 'Paid1058 identity/provenance differs across peers.'}
    Assert-Action (@([regex]::Matches($hostText,'ACTION_ENTRY_DAMAGE target=\d+ row=[123] before=(\d+) after=(\d+) delta=1800')).Count -eq 3) 'Exactly three actual host entry damage outcomes required.'
    Assert-Action ($hostText -match 'ACTION_ENTRY_DRAIN epoch=\d+ liveProjectiles=0 naturalShootingCounter=\d+ oldShooter=\d+') 'Natural pre-entry projectile drain missing.'
    $child=$paidRows[0].Groups[2].Value
    Assert-Action ([regex]::Matches($hostText,('Fusion native Batch entry id='+$child+' kind=1058 rows=3 moving=0')).Count -eq 1) 'Paid1058 fresh callback must execute once.'
    $original=$plantRows[0][4][0].Groups[3].Value
    foreach($phase in @(0,3,4)){
        $originalSpawns=@([regex]::Matches($hostText,('Fusion native Batch projectile plant='+$original+' kind=14 id=(\d+) payload=0 row=[123] phase='+$phase+' variant=0 damage=20'))|ForEach-Object {$_.Groups[1].Value})
        $ordinary=@([regex]::Matches($hostText,('ACTION_PROJECTILE role=host group=4 epoch=\d+ id=(\d+) payload=0 row=[123] phase='+$phase+' variant=0 damage=20 type=\d+ motion=\d+ hostPhysics=1'))|ForEach-Object {$_.Groups[1].Value})
        $converted=@([regex]::Matches($hostText,('ACTION_HOT role=host epoch=\d+ id=(\d+) phase='+$phase+' damage=40 motion=\d+ torch=\d+ token=\d+ sameNativeActor=1'))|Where-Object {$originalSpawns -contains $_.Groups[1].Value -and $ordinary -contains $_.Groups[1].Value}|ForEach-Object {$_.Groups[1].Value})
        $peerHot=@([regex]::Matches($clientText,('ACTION_HOT role=client epoch=\d+ id=(\d+) phase='+$phase+' damage=40 motion=\d+ torch=\d+ token=\d+ sameNativeActor=0'))|Where-Object {$converted -contains $_.Groups[1].Value})
        Assert-Action ($peerHot.Count -gt 0) 'Original14 must heat the same actual source projectile in each lane and replicate its ID.'
        $spawn=@([regex]::Matches($hostText,('Fusion native Batch projectile plant='+$child+' kind=1058 id=(\d+) payload=0 row=[123] phase='+$phase+' variant=0 damage=20'))|ForEach-Object {$_.Groups[1].Value})
        $hostIds=@($childShots[0]|Where-Object {[int]$_.Groups[2].Value -eq $phase}|ForEach-Object {$_.Groups[1].Value})
        $matched=@($childShots[1]|Where-Object {[int]$_.Groups[2].Value -eq $phase -and $spawn -contains $_.Groups[1].Value -and $hostIds -contains $_.Groups[1].Value})
        Assert-Action ($matched.Count -gt 0) '1058 hot phase must be backed by actual child producer and the same projectile ID on both peers.'
    }
    foreach($kind in @(13,1057,17,1155)){
        $hits=@([regex]::Matches($hostText,('Fusion native Batch area plant=\d+ kind='+$kind+' target=\d+ damage='+$(if($kind -in @(13,1057)){1800}else{20})+' shield=0 helm=0 body=\d+')))
        Assert-Action ($hits.Count -ge $(if($kind -in @(13,1057)){1}else{2})) 'Actual native melee callbacks missing.'
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$actionCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    Assert-Action ($actionCommon.Count -ge 8) 'Insufficient common actual native snapshots.'
    for($g=0;$g -lt 5;++$g){
        $hs=$hostText.IndexOf('SMOKE_FUSION_ACTION_SETUP group='+$g+' ');$he=$hostText.IndexOf('SMOKE_FUSION_ACTION_CASE role=host group='+$g+' ')
        $cs=$clientText.IndexOf('SMOKE_FUSION_ACTION_PLANT role=client group='+$g+' ');$ce=$clientText.IndexOf('SMOKE_FUSION_ACTION_CASE role=client group='+$g+' ')
        Assert-Action ($hs -ge 0 -and $he -gt $hs -and $cs -ge 0 -and $ce -gt $cs) 'Epoch snapshot evidence interval missing.'
        $st=@(Get-SnapshotHashes -Text $hostText.Substring($hs,$he-$hs) -Marker SMOKE_SNAPSHOT_TX);$sa=@(Get-SnapshotHashes -Text $clientText.Substring($cs,$ce-$cs) -Marker SMOKE_SNAPSHOT_APPLIED);$common=@($sa|Where-Object {$st -contains $_}|Select-Object -Unique)
        Assert-Action ($common.Count -ge 2) 'An individual native activity lacks common applied world snapshots.';$actionByEpoch+=,[ordered]@{group=$g;epoch=$epochRows[0][$g].Groups[2].Value;common_hashes=$common}
    }
    $actionFrames=@((Assert-ActionFrames $actionHost host $actionHostWidth),(Assert-ActionFrames $actionClient client $actionClientWidth))
    Assert-Action ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $actionExeSha) 'Executable changed during owned run.'
    $actionPassed=$true
}catch{$actionError=$_.Exception.Message;if($_.ScriptStackTrace){$actionError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Action: '+$actionError)}
finally{
    foreach($name in $actionEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$actionEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $actionClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $actionHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$actionPassed=$false;$actionError+=' Owned cleanup failed.'};Ensure-Directory $actionRoot
    $summary=[ordered]@{status=$(if($actionPassed){'pass'}else{'fail'});error=$actionError;fixture='FusionNativeActionFamilies';source_kinds=$actionKinds;run_id=$actionRunId;started_utc=$actionStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
        executable=$GameExe;executable_sha256=$actionExeSha;protocol=48;native_schema=13;requested_host_width=$actionHostWidth;requested_client_width=$actionClientWidth;common_snapshot_hashes=$actionCommon;per_activity=$actionByEpoch;frames=$actionFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$actionRoot;
        native_activities=5;source_zombie_AI_accepted=0;postsetup_direct_actor_writes=0;paid_entry_source_kind=1058;full_seven_color_control='Not exercised unless explicitly observed; not required';
        limits='Initial factory/source ancestry and stationary/actual-biting native NORMAL controls, five genuine native epochs/full intros, natural family combat plus one real paid1058 callback. No source94-NPC AI parity, all difficulty/aura/gear variants, probability-distribution or guaranteed full seven-color control coverage. No rare outcome forced; compilation or renderer nonblankness alone is not gameplay acceptance.';visual_review='Root must inspect actual source family/phase/FX frames; paused matrices include the native Options overlay.'}
    Write-Utf8NoBom -Path (Join-Path $actionRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 9)+[Environment]::NewLine)
}
if($actionPassed){exit 0}else{exit 1}
