#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=180,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$recipientHostWidth=$HostWindowWidth;$recipientClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $recipientHostWidth -ClientWindowWidth $recipientClientWidth
if($HostWindowWidth -ne $recipientHostWidth -or $ClientWindowWidth -ne $recipientClientWidth){throw 'Shared helper changed requested widths.'}
$recipientRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $recipientRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$recipientGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $recipientRepo 'work\fusion-recipient'}
$recipientRunId=[Guid]::NewGuid().ToString('N');$recipientRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$recipientRunId)
$recipientHost=Join-Path $recipientRoot 'host';$recipientClient=Join-Path $recipientRoot 'client'
$recipientHostLog=Join-Path $recipientHost 'logs';$recipientClientLog=Join-Path $recipientClient 'logs'
$recipientHostProcess=$null;$recipientClientProcess=$null;$recipientPassed=$false;$recipientError='';$recipientExeSha='';$recipientFrames=@();$recipientCommon=@();$recipientByEpoch=@()
$recipientStarted=[DateTime]::UtcNow;$recipientEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$recipientEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_RECIPIENT','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $recipientEnvironment.ContainsKey($name)){$recipientEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}}
function Assert-Recipient([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Assert-RecipientFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    for($g=0;$g -lt 2;++$g){foreach($suffix in @('chooser','before','flight','complete')){
        $name=$g.ToString()+'_'+$suffix;$path=Join-Path $Directory ('fusion_recipient_'+$name+'.bmp')
        Assert-Recipient (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual native source frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try{
            Assert-Recipient ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual native viewport dimensions differ.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for($x=16;$x -lt $bitmap.Width;$x+=37){for($y=16;$y -lt $bitmap.Height;$y+=31){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
            Assert-Recipient ($colors.Count -ge 8) 'Blank actual native renderer frame.'
        }finally{$bitmap.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }}
}
try{
    foreach($path in @($GameExe,(Join-Path $recipientGame 'main.pak'),(Join-Path $recipientGame 'SDL3.dll'))){Assert-Recipient (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)}
    $recipientExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($path in @($recipientHostLog,$recipientClientLog,(Join-Path $recipientHost 'appdata'),(Join-Path $recipientClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort}
    foreach($name in $recipientEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_RECIPIENT='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$recipientHost}else{$recipientClient};$width=if($role -eq 'host'){$recipientHostWidth}else{$recipientClientWidth};$nickname=if($role -eq 'host'){'RecipientHost'}else{'RecipientClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native paid Cactus/StarFruit:2 owned activities; '+$recipientRoot)
    $recipientHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $recipientGame -ManifestPath (Join-Path $recipientHost 'smoke.ini') -Role host -RunId $recipientRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $recipientRoot 'pids.json') -Executable $GameExe -HostProcess $recipientHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $recipientHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $recipientHostProcess -Role host -TimeoutSeconds 120
    $recipientClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $recipientGame -ManifestPath (Join-Path $recipientClient 'smoke.ini') -Role client -RunId $recipientRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $recipientRoot 'pids.json') -Executable $GameExe -HostProcess $recipientHostProcess -ClientProcess $recipientClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $recipientHost 'result.json') -ClientResultPath (Join-Path $recipientClient 'result.json') -HostProcess $recipientHostProcess -ClientProcess $recipientClientProcess -HostLogDirectory $recipientHostLog -ClientLogDirectory $recipientClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Recipient ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Paired actual native results missing.'
    foreach($result in $results){$samples=if($result.role -eq 'host'){[int]$result.snapshotsSent}else{[int]$result.snapshotsApplied};Assert-Recipient ($result.status -eq 'pass' -and [int]$result.protocol -eq 48 -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Strict protocol48/full intro native result failed.'}
    $hostText=Get-CombinedLogText $recipientHostLog;$clientText=Get-CombinedLogText $recipientClientLog
    Assert-Recipient ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Real Fusion/protocol48 handshake missing.'
    Assert-Recipient ([regex]::Matches($hostText,'RECIPIENT_SETUP group=[01] index=\d+ target=\d+ row=\d+ body=5000 shield=\d+ center=[0-9.eE+\-]+,[0-9.eE+\-]+ oneTimeInitialValues=1 sourceZombieAIUnported=1 canonicalLaneY=1 contactTick=[0-9.]+').Count -eq 7) 'One-time canonical lane/ray setup evidence missing.'
    $peerEpochs=@();$peerPaid=@();$peerBefore=@();$peerAfter=@();$peerShots=@();$peerCurves=@()
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){
        $role=[string]$pair[0];$text=[string]$pair[1]
        Assert-Recipient ($text -notmatch 'SMOKE_FAIL|seq-gap|invalid snapshot|secondWorld=1|host state rejected') ('Native runtime error in '+$role)
        $epochs=@([regex]::Matches($text,('RECIPIENT_EPOCH role='+$role+' group=(\d+) epoch=(\d+) previous=(\d+)')))
        $paid=@([regex]::Matches($text,('RECIPIENT_PURCHASE role='+$role+' group=(\d+) epoch=(\d+) kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) alias=(\d+) HP=300 cost=125 sun=375 cooldown=750 nativeSDL=1')))
        Assert-Recipient ($epochs.Count -eq 2 -and $paid.Count -eq 2) 'Two fresh activities and paid actual native actors required.'
        $beforeByGroup=@();$afterByGroup=@();$shotsByGroup=@();$curvesByGroup=@()
        for($g=0;$g -lt 2;++$g){
            $epoch=$epochs[$g].Groups[2].Value;$kind=if($g -eq 0){21}else{23};$count=if($g -eq 0){2}else{5}
            Assert-Recipient ([int]$epochs[$g].Groups[1].Value -eq $g -and [uint32]$epoch -gt [uint32]$epochs[$g].Groups[3].Value -and ($g -eq 0 -or $epochs[$g].Groups[3].Value -eq $epochs[$g-1].Groups[2].Value)) 'Activity epoch/ordering differs.'
            Assert-Recipient ([int]$paid[$g].Groups[1].Value -eq $g -and $paid[$g].Groups[2].Value -eq $epoch -and [int]$paid[$g].Groups[3].Value -eq $kind -and [uint32]$paid[$g].Groups[4].Value -gt 0 -and [uint32]$paid[$g].Groups[5].Value -gt 0 -and [uint32]$paid[$g].Groups[6].Value -gt 0 -and [int]$paid[$g].Groups[7].Value -eq $g) 'Paid source identity/owner differs.'
            Assert-Recipient ($text -match ('RECIPIENT_POOL role='+$role+' group='+$g+' eligible=\d+ target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1')) 'Actual shared native chooser/order missing.'
            $pause=[regex]::Match($text,('RECIPIENT_PAUSE role='+$role+' group='+$g+' epoch='+$epoch+' stableMs=(\d+) snapshots=(\d+) launch=\d+ phase=-?[0-9.]+'))
            Assert-Recipient ($pause.Success -and [int]$pause.Groups[1].Value -ge 400 -and [int]$pause.Groups[2].Value -ge 2) 'Actual source clock/HP pause proof missing.'
            $before=@([regex]::Matches($text,('RECIPIENT_BEFORE role='+$role+' group='+$g+' epoch='+$epoch+' index=(\d+) target=(\d+) row=(\d+) body=(\d+) max=(\d+) shield=(\d+) shieldMax=(\d+)')))
            $after=@([regex]::Matches($text,('RECIPIENT_OUTCOME role='+$role+' group='+$g+' epoch='+$epoch+' index=(\d+) target=(\d+) row=(\d+) body=(\d+) max=(\d+) shield=(\d+) shieldMax=(\d+) expectedDamage=(\d+)')))
            Assert-Recipient ($before.Count -eq $count -and $after.Count -eq $count) 'Actual paused before/after native target records missing.'
            for($i=0;$i -lt $count;++$i){
                $shield=if($g -eq 1 -and $i -eq 2){3000}else{0};$damage=if($g -eq 1){20}elseif($i -eq 0){40}else{0}
                Assert-Recipient ([int]$before[$i].Groups[1].Value -eq $i -and [int]$before[$i].Groups[4].Value -eq 5000 -and [int]$before[$i].Groups[5].Value -eq 5000 -and [int]$before[$i].Groups[6].Value -eq $shield -and [int]$before[$i].Groups[7].Value -eq $shield -and [int]$after[$i].Groups[1].Value -eq $i -and $after[$i].Groups[2].Value -eq $before[$i].Groups[2].Value -and $after[$i].Groups[3].Value -eq $before[$i].Groups[3].Value -and [int]$after[$i].Groups[4].Value -eq (5000-$damage) -and [int]$after[$i].Groups[5].Value -eq 5000 -and [int]$after[$i].Groups[6].Value -eq $shield -and [int]$after[$i].Groups[7].Value -eq $shield -and [int]$after[$i].Groups[8].Value -eq $damage) 'Exact20/direct-vs-behind/shield-bypass native outcome differs.'
                if($g -eq 1 -and $i -ne 2){Assert-Recipient ([int]$after[$i].Groups[3].Value -ne 2) 'Crossrow Star hit was accidentally same row.'}
            }
            $payload=if($g -eq 0){37}else{39}
            $shots=@([regex]::Matches($text,('RECIPIENT_PROJECTILE role='+$role+' group='+$g+' epoch='+$epoch+' id=(\d+) payload='+$payload+' way=(\d+) type=(\d+) motion=(\d+) damage=20 sun=0 x=[0-9.eE+\-]+ y=[0-9.eE+\-]+')))
            $curves=@([regex]::Matches($text,('RECIPIENT_CURVE role='+$role+' group='+$g+' epoch='+$epoch+' id=(\d+) index=(\d+) way=(\d+) dx=([0-9.eE+\-]+) dy=([0-9.eE+\-]+) vx=([0-9.eE+\-]+) vy=([0-9.eE+\-]+) hostPhysics=([01])')))
            Assert-Recipient ($shots.Count -ge $(if($g -eq 0){2}else{5})) 'Actual native source projectile metadata missing.'
            foreach($index in $(if($g -eq 0){@(0)}else{@(0,1,2,3,4)})){
                $lane=@($curves|Where-Object {[int]$_.Groups[2].Value -eq $index});Assert-Recipient ($lane.Count -ge 1) 'Actual physical source vector not observed.'
                foreach($curve in $lane){$way=if($g -eq 1){if($index -eq 2){9}else{2}}else{0};Assert-Recipient ([int]$curve.Groups[3].Value -eq $way -and [int]$curve.Groups[8].Value -eq $(if($role -eq 'host'){1}else{0})) 'Source way/physics ownership differs.'}
            }
            Assert-Recipient ($text -match ('RECIPIENT_CASE role='+$role+' group='+$g+' epoch='+$epoch+' kind='+$kind+' paidNative=1 naturalHits=1 pause=1')) 'Natural paid family activity incomplete.'
            $beforeByGroup+=,@($before);$afterByGroup+=,@($after);$shotsByGroup+=,@($shots);$curvesByGroup+=,@($curves)
        }
        Assert-Recipient ($text -match ('RECIPIENT_COMPLETE role='+$role+' kinds=21,23 activities=2 purchases=2 cactusRepeated20=1 cactusOneContact=1 starPaths=5 crossrow=1 leftShieldless=1 sourceZombieAIUnported=1 postSetupActorWrites=0 air10=NotExecuted starStorage=NotExecuted nativeBoard=1 secondWorld=0')) 'Strict bounded recipient completion missing.'
        $peerEpochs+=,@($epochs);$peerPaid+=,@($paid);$peerBefore+=,@($beforeByGroup);$peerAfter+=,@($afterByGroup);$peerShots+=,@($shotsByGroup);$peerCurves+=,@($curvesByGroup)
    }
    for($g=0;$g -lt 2;++$g){
        foreach($j in 1..8){Assert-Recipient ($peerPaid[0][$g].Groups[$j].Value -eq $peerPaid[1][$g].Groups[$j].Value) 'Peer paid source actor/alias/owner differs.'}
        for($i=0;$i -lt $peerBefore[0][$g].Count;++$i){foreach($j in 1..7){Assert-Recipient ($peerBefore[0][$g][$i].Groups[$j].Value -eq $peerBefore[1][$g][$i].Groups[$j].Value -and $peerAfter[0][$g][$i].Groups[$j].Value -eq $peerAfter[1][$g][$i].Groups[$j].Value) 'Peer exact native before/after target differs.'}}
        $kind=if($g -eq 0){21}else{23};$payload=if($g -eq 0){37}else{39};$actor=$peerPaid[0][$g].Groups[4].Value
        $emitted=@([regex]::Matches($hostText,('Fusion native Recipient projectile plant='+$actor+' kind='+$kind+' id=(\d+) payload='+$payload+' way=(\d+) shot=(\d+) vx=[0-9.eE+\-]+ vy=[0-9.eE+\-]+ damage=20')))
        $impacts=@([regex]::Matches($hostText,('Fusion native Recipient impact projectile=(\d+) payload='+$payload+' way=(\d+) target=(\d+) damage=20 flags=(\d+) shield=(\d+) helm=0 body=(\d+)')))
        Assert-Recipient ($impacts.Count -eq $(if($g -eq 0){2}else{5}) -and @($impacts|ForEach-Object {$_.Groups[1].Value}|Select-Object -Unique).Count -eq $impacts.Count) 'Projectile must impact once per actual distinct native actor.'
        foreach($hit in $impacts){
            $id=$hit.Groups[1].Value;Assert-Recipient (@($emitted|Where-Object {$_.Groups[1].Value -eq $id}).Count -eq 1) 'Actual impact lacks source producer identity.'
            foreach($peer in 0..1){
                Assert-Recipient (@($peerShots[$peer][$g]|Where-Object {$_.Groups[1].Value -eq $id}).Count -eq 1) 'Impacted source projectile ID was not replicated to both peers.'
                $role=if($peer -eq 0){'host'}else{'client'};$text=if($peer -eq 0){$hostText}else{$clientText};$epoch=$peerEpochs[$peer][$g].Groups[2].Value
                Assert-Recipient ($text -match ('RECIPIENT_RETIRED role='+$role+' group='+$g+' epoch='+$epoch+' id='+$id+' localGeneration=[1-9]\d* nextFrame=1 deadOrFreed=1')) 'Impacted full-generation native projectile did not naturally retire.'
            }
            $left=$g -eq 1 -and [int]$hit.Groups[2].Value -eq 9
            Assert-Recipient ([int]$hit.Groups[4].Value -eq $(if($left){1}else{0}) -and [int]$hit.Groups[5].Value -eq $(if($left){3000}else{0})) 'Source normal-vs-Shieldless impact flags differ.'
        }
        foreach($index in $(if($g -eq 0){@(0)}else{@(0,1,2,3,4)})){
            $hostIds=@($peerCurves[0][$g]|Where-Object {[int]$_.Groups[2].Value -eq $index}|ForEach-Object {$_.Groups[1].Value})
            Assert-Recipient (@($peerCurves[1][$g]|Where-Object {[int]$_.Groups[2].Value -eq $index -and $hostIds -contains $_.Groups[1].Value}).Count -ge 1) 'Actual physical lane lacks a shared host/client projectile ID.'
        }
        $epoch=$peerEpochs[0][$g].Groups[2].Value;Assert-Recipient ($epoch -eq $peerEpochs[1][$g].Groups[2].Value) 'Peer fresh activity epochs differ.'
        $entry=[regex]::Match($hostText,('RECIPIENT_ENTRY_EMPTY role=host group='+$g+' epoch='+$epoch+' firstAppFrame=(\d+) observedAppFrame=(\d+) plantAllocated=0 zombieAllocated=0 naturalCleanup=1'))
        Assert-Recipient ($entry.Success -and [int]$entry.Groups[2].Value -gt [int]$entry.Groups[1].Value) 'Strict empty native host world after natural intro retirement missing.'
        $hs=$hostText.IndexOf('SMOKE_FUSION_RECIPIENT_PURCHASE role=host group='+$g+' ');$he=$hostText.IndexOf('SMOKE_FUSION_RECIPIENT_CASE role=host group='+$g+' ')
        $cs=$clientText.IndexOf('SMOKE_FUSION_RECIPIENT_PURCHASE role=client group='+$g+' ');$ce=$clientText.IndexOf('SMOKE_FUSION_RECIPIENT_CASE role=client group='+$g+' ')
        Assert-Recipient ($hs -ge 0 -and $he -gt $hs -and $cs -ge 0 -and $ce -gt $cs) 'Native activity snapshot evidence interval missing.'
        $tx=@(Get-SnapshotHashes -Text $hostText.Substring($hs,$he-$hs) -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText.Substring($cs,$ce-$cs) -Marker SMOKE_SNAPSHOT_APPLIED);$common=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
        Assert-Recipient ($common.Count -ge 2) 'Each real native activity needs common applied snapshots.';$recipientByEpoch+=,[ordered]@{group=$g;epoch=$epoch;common_hashes=$common}
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$recipientCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    Assert-Recipient ($recipientCommon.Count -ge 8) 'Insufficient common real native full snapshots.'
    $recipientFrames=@((Assert-RecipientFrames $recipientHost host $recipientHostWidth),(Assert-RecipientFrames $recipientClient client $recipientClientWidth))
    Assert-Recipient ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $recipientExeSha) 'Executable changed during owned run.';$recipientPassed=$true
}catch{$recipientError=$_.Exception.Message;if($_.ScriptStackTrace){$recipientError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Recipient: '+$recipientError)}
finally{
    foreach($name in $recipientEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$recipientEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $recipientClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $recipientHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$recipientPassed=$false;$recipientError+=' Owned cleanup failed.'};Ensure-Directory $recipientRoot
    $summary=[ordered]@{status=$(if($recipientPassed){'pass'}else{'fail'});error=$recipientError;fixture='FusionNativeRecipientShooters';source_kinds=@(21,23);run_id=$recipientRunId;started_utc=$recipientStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
        executable=$GameExe;executable_sha256=$recipientExeSha;protocol=48;native_schema=13;requested_host_width=$recipientHostWidth;requested_client_width=$recipientClientWidth;common_snapshot_hashes=$recipientCommon;per_activity=$recipientByEpoch;frames=$recipientFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$recipientRoot;
        native_activities=2;source_zombie_AI_accepted=0;postsetup_direct_actor_writes=0;paid_source_kinds=@(21,23);
        limits='Ground Cactus21 and Star23 paid/native/source identity, natural physical projectiles, native stationary NORMAL targets, source row policy and left shield bypass only. Source airborne NPC10/upper Cactus callback and Star storage recipients are Not executed. Unit timing endpoints are not reclassified as native runtime coverage. Root must inspect actual source art frames.'}
    Write-Utf8NoBom -Path (Join-Path $recipientRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 9)+[Environment]::NewLine)
}
if($recipientPassed){exit 0}else{exit 1}
