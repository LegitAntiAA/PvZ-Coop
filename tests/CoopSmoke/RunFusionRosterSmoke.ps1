#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(120,600)][int]$TimeoutSeconds=180,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242,
    [string]$ValidateExisting=''
)
$ErrorActionPreference='Stop'
$rosterHostWidth=$HostWindowWidth;$rosterClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $rosterHostWidth -ClientWindowWidth $rosterClientWidth
if($HostWindowWidth -ne $rosterHostWidth -or $ClientWindowWidth -ne $rosterClientWidth){throw 'Shared helper changed requested widths.'}
$rosterRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $rosterRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$rosterGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $rosterRepo 'work\fusion\roster240'}
$rosterRun=[Guid]::NewGuid().ToString('N');$rosterRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$rosterRun)
$rosterPeers=@('host','client1','client2');$rosterProcesses=[ordered]@{host=$null;client1=$null;client2=$null};$rosterLedger=@();$rosterSaved=@{}
$rosterStarted=[DateTime]::UtcNow;$rosterSha='';$rosterError='';$rosterPass=$false;$rosterLifecycle=$false;$rosterClean=$true;$rosterDrop=$null;$rosterEvidence=$null
foreach($e in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$rosterSaved[$e.Name]=$e.Value}
foreach($n in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_ROSTER','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $rosterSaved.ContainsKey($n)){$rosterSaved[$n]=[Environment]::GetEnvironmentVariable($n,'Process')}}
function Assert-Roster([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Roster-Path([string]$Peer,[string]$Leaf){return Join-Path (Join-Path $rosterRoot $Peer) $Leaf}
function Assert-RosterLogs {
    foreach($peer in $rosterPeers){$text=Get-CombinedLogText (Roster-Path $peer 'logs')
        Assert-Roster ($text -notmatch 'Access Violation|\bSMOKE_FAIL\b|\bSMOKE_STOPPED\b|seq-gap|invalid snapshot|host state rejected|encode rejected|preflight (?:failed|rejected)|fatal:|protocol violation|queue overflow|assertion failed|unhandled exception') ('Full native lifecycle failure in '+$peer)
    }
}
function Test-RosterResult($Result,[int]$Slot){
    if($null -eq $Result){return $false};$role=if($Slot -eq 0){'host'}else{'client'};$count=if($Slot -eq 0){[int]$Result.snapshotsSent}else{[int]$Result.snapshotsApplied}
    return ($Result.status -eq 'pass' -and $Result.role -eq $role -and [int]$Result.protocol -eq 48 -and $Result.scenario -eq 'fusion_validation' -and $Result.mode -eq 'fusion_validation' -and [int]$Result.level -eq 1 -and [uint32]$Result.seed -eq $Seed -and -not [bool]$Result.skipIntro -and $count -ge 8)
}
function Get-RosterFrames([string]$Peer,[int]$Slot){
    Add-Type -AssemblyName System.Drawing
    $width=if($Slot -eq 0){$rosterHostWidth}else{$rosterClientWidth};$names=if($Slot -eq 2){@('chooser','before')}else{@('chooser','before','after_drop','complete')}
    foreach($name in $names){$path=Roster-Path $Peer ('fusion_roster_'+$name+'.bmp');Assert-Roster (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual native roster frame: '+$path);$bmp=[Drawing.Bitmap]::new($path)
        try{Assert-Roster ($bmp.Width -eq $width -and $bmp.Height -eq 600) 'Actual roster viewport differs.';$colors=New-Object 'System.Collections.Generic.HashSet[int]';for($x=16;$x -lt $bmp.Width;$x+=37){for($y=16;$y -lt $bmp.Height;$y+=31){[void]$colors.Add($bmp.GetPixel($x,$y).ToArgb())}};Assert-Roster ($colors.Count -ge 8) 'Blank actual roster renderer frame.'}finally{$bmp.Dispose()}
        [ordered]@{peer=$Peer;slot=$Slot;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
function Get-RosterBank([string]$Text,[string]$Role,[int]$Slot,[string]$Phase){
    $rows=@([regex]::Matches($Text,('SMOKE_ROSTER240_BANK role='+$Role+' slot='+$Slot+' phase='+$Phase+' epoch=(\d+) packet=(\d+) selection=(\d+) kind=(\d+) extra=([01]) owner=(\d+) origin=(\d+) price=(\d+) recharge=([-+\deE.]+) type=(\d+) time=(\d+) counter=(\d+) refreshing=([01]) active=([01]) remaining=([-+\deE.]+) nativeOwner=(\d+)')))
    Assert-Roster ($rows.Count -eq 10) ('Exactly ten canonical packet records required: '+$Slot+'/'+$Phase);$map=@{};$counts=@(0,0,0,0)
    foreach($m in $rows){$id=$m.Groups[2].Value;$owner=[int]$m.Groups[6].Value;Assert-Roster ([int]$id -gt 0 -and -not $map.ContainsKey($id) -and $owner -ge 0 -and $owner -lt 4 -and $m.Groups[6].Value -eq $m.Groups[16].Value) 'Physical/metadata packet owner or unique ID differs.';$map[$id]=$m;++$counts[$owner]}
    if($Phase -eq 'before'){Assert-Roster (($counts -join ',') -eq '4,3,3,0') 'Initial owner quotas are not4/3/3.'}else{Assert-Roster (($counts -join ',') -eq '5,5,0,0') 'Reconciled actual owners are not5/5.'};return $map
}
function Project-RosterCounter([int]$Counter,[int]$Time){
    Assert-Roster ($Counter -ge 0 -and $Time -ge 0 -and ($Time -gt 0 -or $Counter -eq 0) -and ($Time -eq 0 -or $Counter -le $Time)) 'Invalid native cooldown projection input.'
    if($Time -eq 0){return 0}
    # Existing NetBoardSync U8(counter*255/time), then U8*time/255.
    # Both native divisions truncate integers; this is not a +/-1 tolerance.
    $wire=[int][Math]::Floor(([double]$Counter*255)/$Time)
    return [int][Math]::Floor(([double]$wire*$Time)/255)
}
function Assert-RosterCrossBank([hashtable]$HostBank,[hashtable]$PeerBank){
    foreach($id in $HostBank.Keys){Assert-Roster ($PeerBank.ContainsKey($id)) 'Shared packet ID missing between peers.'
        foreach($j in 1..16){$expected=if($j -eq 12){[string](Project-RosterCounter ([int]$HostBank[$id].Groups[12].Value) ([int]$HostBank[$id].Groups[11].Value))}else{$HostBank[$id].Groups[$j].Value}
            Assert-Roster ($PeerBank[$id].Groups[$j].Value -eq $expected) ('Shared paused packet data differs: packet='+$id+' field='+$j+' expected='+$expected+' actual='+$PeerBank[$id].Groups[$j].Value)
        }
    }
}
function Get-RosterSnapshotPairs([string]$HostText,[string]$ClientText,[uint32]$HostStart,[uint32]$ClientStart){
    $tx=@([regex]::Matches($HostText,'SMOKE_SNAPSHOT_TX index=(\d+) bytes=(\d+) hash=([0-9a-fA-F]{8})'));$ap=@([regex]::Matches($ClientText,'SMOKE_SNAPSHOT_APPLIED index=(\d+) bytes=(\d+) hash=([0-9a-fA-F]{8})'));$queues=@{}
    foreach($m in $tx){if([uint32]$m.Groups[1].Value -le $HostStart){continue};$key=$m.Groups[2].Value+':'+$m.Groups[3].Value.ToUpperInvariant();if(-not $queues.ContainsKey($key)){$queues[$key]=New-Object System.Collections.Queue};$queues[$key].Enqueue($m)}
    foreach($m in $ap){if([uint32]$m.Groups[1].Value -le $ClientStart){continue};$key=$m.Groups[2].Value+':'+$m.Groups[3].Value.ToUpperInvariant();if($queues.ContainsKey($key) -and $queues[$key].Count){$h=$queues[$key].Dequeue();[ordered]@{host_index=[uint32]$h.Groups[1].Value;client_index=[uint32]$m.Groups[1].Value;bytes=[int]$m.Groups[2].Value;hash=$m.Groups[3].Value.ToUpperInvariant()}}}
}
function Assert-RosterOutcome {
    Assert-RosterLogs;$texts=@();$before=@();$initial=@();$old=@();$epoch='';$frames=@();$after=@();$drops=@();$completed=@();$reused=@()
    for($slot=0;$slot -lt 3;++$slot){$peer=$rosterPeers[$slot];$role=if($slot -eq 0){'host'}else{'client'};$result=Read-JsonIfComplete (Roster-Path $peer 'result.json');Assert-Roster (Test-RosterResult $result $slot) ('Invalid actual result: '+$peer);$text=Get-CombinedLogText (Roster-Path $peer 'logs');$texts+=,$text
        Assert-Roster ($text -match ('SMOKE_ROSTER240_IDENTITY role='+$role+' slot='+$slot+'\b') -and $text -match ('SMOKE_ROSTER240_POOL role='+$role+' slot='+$slot+' mask=7 quotas=4,3,3,0 cards=10 actualPlayers=3')) 'Actual three-slot assignment/chooser proof missing.'
        if($slot -gt 0){Assert-Roster ($text -match ('WELCOME assignedSlot='+$slot+'[^\r\n]*contentMode=')) 'Saved client WELCOME assignment differs.'}
        $e=[regex]::Match($text,('SMOKE_ROSTER240_EPOCH role='+$role+' slot='+$slot+' epoch=(\d+)'));Assert-Roster $e.Success 'Native roster epoch missing.';if($slot -eq 0){$epoch=$e.Groups[1].Value}else{Assert-Roster ($epoch -eq $e.Groups[1].Value) 'Native peer epochs differ.'}
        $paid=[regex]::Match($text,('SMOKE_ROSTER240_PURCHASE role='+$role+' slot='+$slot+' phase=initial epoch='+$epoch+' id=(\d+) token=(\d+) packet=(\d+) owner=2 kind=27 HP=300 cost=25 sun=475 cooldown=750 nativeSDL=1'))
        Assert-Roster ($paid.Success -and [uint32]$paid.Groups[1].Value -gt 0 -and [uint32]$paid.Groups[2].Value -gt 0 -and [uint32]$paid.Groups[3].Value -gt 0) 'Actual slot2 paid-source birth identity missing.';$initial+=,$paid
        $before+=,(Get-RosterBank $text $role $slot 'before');$o=[regex]::Match($text,('SMOKE_ROSTER240_OLD role='+$role+' slot='+$slot+' phase=before epoch='+$epoch+' id=(\d+) token=(\d+) packet=(\d+) owner=2 kind=27 HP=300 column=2 row=1'));Assert-Roster $o.Success 'Original full-provenance actor baseline missing.';$old+=,$o
        foreach($j in 1..3){Assert-Roster ($paid.Groups[$j].Value -eq $o.Groups[$j].Value) 'Baseline actor differs from the paid birth.'}
        if($slot -eq 2){Assert-Roster ($text -match ('SMOKE_ROSTER240_DROP_SAFE role=client slot=2 epoch='+$epoch+' packet='+$paid.Groups[3].Value+' id='+$paid.Groups[1].Value+' token='+$paid.Groups[2].Value+' sun=475 mask=7 resultScope=departing-peer-only')) 'Slot2 did not announce the actual safe completed barrier.'}
        $frames+=@(Get-RosterFrames $peer $slot)
    }
    foreach($slot in @(1,2)){foreach($j in 1..3){Assert-Roster ($initial[$slot].Groups[$j].Value -eq $initial[0].Groups[$j].Value) 'Three peers disagree on initial paid actor identity.'};Assert-RosterCrossBank $before[0] $before[$slot]}
    $packet=$initial[0].Groups[3].Value;$used=$before[0][$packet];Assert-Roster ([int]$used.Groups[4].Value -eq 27 -and [int]$used.Groups[6].Value -eq 2 -and [int]$used.Groups[8].Value -eq 25 -and [int]$used.Groups[11].Value -eq 750 -and [int]$used.Groups[12].Value -gt 0 -and [int]$used.Groups[12].Value -lt 750 -and $used.Groups[13].Value -eq '1' -and $used.Groups[14].Value -eq '0') 'Baseline must contain a genuinely running750-tick source cooldown.'
    for($slot=0;$slot -lt 2;++$slot){$role=if($slot -eq 0){'host'}else{'client'};$text=$texts[$slot];$map=Get-RosterBank $text $role $slot 'after';$after+=,$map
        foreach($id in $before[$slot].Keys){Assert-Roster ($map.ContainsKey($id)) 'Roster reconciliation replaced a stable packet ID.';foreach($j in @(1,2,3,4,5,7,8,9,10,11,12,13,14,15)){Assert-Roster ($map[$id].Groups[$j].Value -eq $before[$slot][$id].Groups[$j].Value) 'Roster reconciliation changed identity, price, origin or paused live CD.'}}
        $drop=[regex]::Match($text,('SMOKE_ROSTER240_DROP role='+$role+' slot='+$slot+' epoch='+$epoch+' mask=3 quotas=5,5,0,0 packet='+$packet+' newOwner=([01]) sun=475 samples=(\d+) pausedCDUnchanged=1 birthOwner=2'))
        Assert-Roster ($drop.Success -and $map[$packet].Groups[6].Value -eq $drop.Groups[1].Value) 'Actual post-drop owner/mask reconciliation missing.';$drops+=,$drop
        foreach($phase in @('after','complete')){Assert-Roster ($text -match ('SMOKE_ROSTER240_OLD role='+$role+' slot='+$slot+' phase='+$phase+' epoch='+$epoch+' id='+$initial[0].Groups[1].Value+' token='+$initial[0].Groups[2].Value+' packet='+$packet+' owner=2 kind=27 HP=300 column=2 row=1')) 'Departed birth owner/token/HP was rewritten or lost.'}
        $fresh=[regex]::Match($text,('SMOKE_ROSTER240_POST_SNAPSHOTS role='+$role+' slot='+$slot+' epoch='+$epoch+' start=(\d+) now=(\d+) fresh=(\d+)'))
        Assert-Roster ($fresh.Success -and $fresh.Groups[1].Value -eq $drop.Groups[2].Value -and [uint32]$fresh.Groups[3].Value -ge 8 -and ([uint32]$fresh.Groups[2].Value-[uint32]$fresh.Groups[1].Value) -eq [uint32]$fresh.Groups[3].Value) 'Eight fresh post-drop native snapshots were not observed.'
        $new=[regex]::Match($text,('SMOKE_ROSTER240_PURCHASE role='+$role+' slot='+$slot+' phase=reused epoch='+$epoch+' id=(\d+) token=(\d+) packet='+$packet+' owner='+$drop.Groups[1].Value+' kind=27 HP=300 cost=25 sun=450 cooldown=750 nativeSDL=1'))
        Assert-Roster ($new.Success -and $new.Groups[1].Value -ne $initial[0].Groups[1].Value -and $new.Groups[2].Value -ne $initial[0].Groups[2].Value) 'Transferred packet did not create a fresh paid new-owner actor.';$reused+=,$new
        Assert-Roster ($text -match ('SMOKE_ROSTER240_COMPLETE role='+$role+' slot='+$slot+' epoch='+$epoch+' oldId='+$initial[0].Groups[1].Value+' oldToken='+$initial[0].Groups[2].Value+' oldOwner=2 packet='+$packet+' newOwner='+$drop.Groups[1].Value+' newId='+$new.Groups[1].Value+' newToken='+$new.Groups[2].Value+' mask=3 quotas=5,5,0,0 packets=10 sun=450 postDropSnapshots=8 naturalCD=1 pausedCDPreserved=1 paidCharges=2 birthPreserved=1 actorCounterRNGWrites=0 phase=Playing Choosing=NotExecuted Intro=NotExecuted')) 'Remaining peer succeeded before all post-drop gameplay assertions.'
    }
    Assert-Roster ($drops[0].Groups[1].Value -eq $drops[1].Groups[1].Value -and $reused[0].Groups[1].Value -eq $reused[1].Groups[1].Value -and $reused[0].Groups[2].Value -eq $reused[1].Groups[2].Value) 'Remaining peers disagree on transferred owner/new actor.'
    Assert-RosterCrossBank $after[0] $after[1]
    $owner=[int]$drops[0].Groups[1].Value;$ownerRole=if($owner -eq 0){'host'}else{'client'}
    Assert-Roster ($texts[2] -match ('SMOKE_ROSTER240_HELD role=client slot=2 phase=initial packet='+$packet+' owner=2 sun=500 heldEcho=1') -and $texts[$owner] -match ('SMOKE_ROSTER240_HELD role='+$ownerRole+' slot='+$owner+' phase=reused packet='+$packet+' owner='+$owner+' sun=475 heldEcho=1')) 'Both actual owning peers must use bank pickup plus held acknowledgement.'
    $ready=[regex]::Match($texts[0],('SMOKE_ROSTER240_CD_READY role=host slot=0 packet='+$packet+' owner='+$owner+' preservedRemaining=(\d+) hostElapsed=(\d+) natural=1'))
    Assert-Roster ($ready.Success -and [int]$ready.Groups[1].Value -eq ([int]$used.Groups[11].Value-[int]$used.Groups[12].Value) -and [int]$ready.Groups[2].Value -ge [int]$ready.Groups[1].Value) 'Acquired card bypassed its natural remaining native cooldown.'
    $pairs=@(Get-RosterSnapshotPairs $texts[0] $texts[1] ([uint32]$drops[0].Groups[2].Value) ([uint32]$drops[1].Groups[2].Value));Assert-Roster ($pairs.Count -ge 8) 'Eight fresh content-matching post-drop native snapshot messages required.'
    Assert-Roster ($frames.Count -eq 10) 'Ten actual native roster frames required.'
    return [pscustomobject]@{epoch=$epoch;packet=[int]$packet;old_id=[int]$initial[0].Groups[1].Value;old_token=[uint32]$initial[0].Groups[2].Value;old_owner=2;new_owner=$owner;new_id=[int]$reused[0].Groups[1].Value;new_token=[uint32]$reused[0].Groups[2].Value;postdrop_snapshot_pairs=$pairs;frames=$frames;
        snapshot_pair_policy='Each post-marker message used once, matched by bytes/hash; raw indices retained. Identical paused snapshots count as fresh messages, not new unique states.'}
}
if($ValidateExisting){
    $existingRoot=Get-NormalizedPath $ValidateExisting;$summaryPath=Join-Path $existingRoot 'summary.json';$oldHash=(Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash;$old=Read-JsonIfComplete $summaryPath
    Assert-Roster ($old -and $old.fixture -eq 'FusionNativeRosterPlaying' -and $old.owned_cleanup -and $old.graceful_lifecycle -and [int]$old.protocol -eq 48 -and [int]$old.native_schema -eq 13) 'Saved run is not a completed, gracefully cleaned three-peer fixture.'
    Assert-Roster (Test-SafeRunRoot -RunRoot $existingRoot -ArtifactsRoot (Split-Path -Parent $existingRoot) -RunId ([string]$old.run_id)) 'Unsafe existing roster root.'
    $postPath=Join-Path $existingRoot 'postvalidation.json';Assert-Roster (-not (Test-Path -LiteralPath $postPath)) 'Refusing to overwrite postvalidation evidence.'
    $valid=$false;$validationError='';$outcome=$null;$identity=@();$rosterRoot=$existingRoot
    try{
        Assert-Roster ((Get-NormalizedPath $old.executable) -eq $GameExe -and (Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $old.executable_sha256) 'Saved executable identity differs.'
        $ledger=@(Read-JsonIfComplete (Join-Path $existingRoot 'pids.json'));Assert-Roster ($ledger.Count -eq 3 -and @($old.owned_processes).Count -eq 3) 'Exactly three saved owned launches required.'
        for($slot=0;$slot -lt 3;++$slot){$peer=$rosterPeers[$slot];$entry=@($ledger|Where-Object {$_.peer -eq $peer});Assert-Roster ($entry.Count -eq 1 -and [int]$entry[0].expected_slot -eq $slot -and [int]$entry[0].pid -gt 0 -and (Get-NormalizedPath $entry[0].executable) -eq $GameExe -and [DateTime]::Parse($entry[0].started_utc).ToUniversalTime() -ge [DateTime]::Parse($old.started_utc).ToUniversalTime().AddSeconds(-2) -and [DateTime]::Parse($entry[0].started_utc).ToUniversalTime() -le [DateTime]::Parse($old.finished_utc).ToUniversalTime()) 'Saved PID/path/start identity differs.'
            $config=Read-SharedTextFile (Roster-Path $peer 'coop.ini');$width=[regex]::Match($config,'(?m)^window_width=(800|1066)\s*$');Assert-Roster $width.Success 'Saved native viewport config missing.'
            if($slot -eq 0){$rosterHostWidth=[int]$width.Groups[1].Value}elseif($slot -eq 1){$rosterClientWidth=[int]$width.Groups[1].Value}else{Assert-Roster ([int]$width.Groups[1].Value -eq $rosterClientWidth) 'Saved client viewport widths differ.'};$identity+=,$entry[0]
        }
        $drop=Read-JsonIfComplete (Join-Path $existingRoot 'drop.json');Assert-Roster ($drop -and [int]$drop.slot -eq 2 -and $drop.graceful -and $drop.result_before_close -and [int]$drop.exit_code -eq 0 -and [int]$drop.pid -eq [int]$ledger[2].pid) 'Actual slot2 safe graceful close evidence missing.'
        $outcome=Assert-RosterOutcome;Assert-Roster ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $old.executable_sha256) 'Executable changed during saved validation.';$valid=$true
    }catch{$validationError=$_.Exception.Message;if($_.ScriptStackTrace){$validationError+=' | '+$_.ScriptStackTrace}}
    $unchanged=(Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash -eq $oldHash;if(-not $unchanged){$valid=$false;$validationError+=' Original summary changed.'}
    $post=[ordered]@{status=$(if($valid){'pass'}else{'fail'});validation='existing-artifacts-only';error=$validationError;original_harness_status=$old.status;original_harness_error=$old.error;original_summary_sha256=$oldHash;original_summary_unchanged=$unchanged;executable_sha256=$old.executable_sha256;owned_identity=$identity;owned_cleanup=$old.owned_cleanup;graceful_lifecycle=$old.graceful_lifecycle;outcome=$outcome;spawned=0;staged=0;renamed=0;
        scope='Same full result/log/bank/UID/CD/frames/post-drop snapshot assertions after recorded graceful cleanup; cross-peer counter uses exact existing U8 encode/decode projection. Local before-to-after equality remains exact.';visual_review='Root reviews saved actual frames; no new rendering.'}
    Write-Utf8NoBom -Path $postPath -Text (($post|ConvertTo-Json -Depth 12)+[Environment]::NewLine);Assert-Roster $valid ('Saved roster validation failed: '+$validationError)
    Write-Output ('Roster saved-artifact validation Passed; postdropPairs='+$outcome.postdrop_snapshot_pairs.Count+' frames='+$outcome.frames.Count+'; '+$postPath);return
}
try{
    foreach($p in @($GameExe,(Join-Path $rosterGame 'main.pak'),(Join-Path $rosterGame 'SDL3.dll'))){Assert-Roster (Test-Path -LiteralPath $p -PathType Leaf) ('Runtime input missing: '+$p)}
    Assert-Roster (Test-SafeRunRoot -RunRoot $rosterRoot -ArtifactsRoot (Split-Path -Parent $rosterRoot) -RunId $rosterRun) 'Unsafe owned roster artifact root.';$rosterSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($n in $rosterSaved.Keys){[Environment]::SetEnvironmentVariable($n,$null,'Process')};$env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_ROSTER='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    if($Port -eq 0){$Port=Get-FreeLoopbackPort}
    foreach($peer in $rosterPeers){Ensure-Directory (Roster-Path $peer 'logs');Ensure-Directory (Roster-Path $peer 'appdata');$role=if($peer -eq 'host'){'host'}else{'client'};$nick=if($peer -eq 'host'){'RosterHost'}elseif($peer -eq 'client1'){'RosterKeep'}else{'RosterDrop'};$width=if($peer -eq 'host'){$rosterHostWidth}else{$rosterClientWidth}
        Write-CoopConfig -Path (Roster-Path $peer 'coop.ini') -Nickname $nick -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Roster-Path $peer 'smoke.ini') -Role $role -Nickname $nick -AppDataDirectory (Roster-Path $peer 'appdata') -ConfigPath (Roster-Path $peer 'coop.ini') -LogDirectory (Roster-Path $peer 'logs') -ResultPath (Roster-Path $peer 'result.json') -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native roster240: three owned peers, slot2 paid birth/drop then transferred-card reuse; '+$rosterRoot)
    for($slot=0;$slot -lt 3;++$slot){$peer=$rosterPeers[$slot];$role=if($slot -eq 0){'host'}else{'client'};$proc=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $rosterGame -ManifestPath (Roster-Path $peer 'smoke.ini') -Role $role -RunId $rosterRun -EnableUnicodeChat $false;$rosterProcesses[$peer]=$proc
        $rosterLedger+=,[ordered]@{peer=$peer;role=$role;expected_slot=$slot;pid=$proc.Id;executable=$proc.SmokeExecutable;started_utc=([DateTime]::new($proc.SmokeStartedUtcTicks,[DateTimeKind]::Utc)).ToString('o')};Write-Utf8NoBom -Path (Join-Path $rosterRoot 'pids.json') -Text (($rosterLedger|ConvertTo-Json -Depth 5)+[Environment]::NewLine)
        if($slot -eq 0){Wait-ForLogPattern -LogDirectory (Roster-Path $peer 'logs') -Pattern ('(?im)host listening port='+$Port+'\b') -Process $proc -Role host -TimeoutSeconds 120}else{Wait-ForLogPattern -LogDirectory (Roster-Path $peer 'logs') -Pattern ('WELCOME assignedSlot='+$slot+'\b') -Process $proc -Role $peer -TimeoutSeconds 120}
    }
    $until=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds);$departing=$null
    while([DateTime]::UtcNow -lt $until){Assert-RosterLogs;foreach($peer in $rosterPeers){Assert-Roster (-not $rosterProcesses[$peer].HasExited) ('Owned peer exited before safe departure: '+$peer)};$departing=Read-JsonIfComplete (Roster-Path client2 'result.json');if($departing){Assert-Roster (Test-RosterResult $departing 2) 'Departing peer did not complete its scoped phase.';$dropText=Get-CombinedLogText (Roster-Path client2 'logs');Assert-Roster ($dropText -match 'SMOKE_ROSTER240_DROP_SAFE role=client slot=2') 'Refusing early slot2 close without announced safe barrier.';break};Start-Sleep -Milliseconds 200}
    Assert-Roster ($null -ne $departing) 'Timed out before slot2 safe result.';$requested=[DateTime]::UtcNow
    Request-OwnedProcessClose -Process $rosterProcesses.client2 -Role 'client2-slot2';Wait-ForCleanProcessExit -Process $rosterProcesses.client2 -Role 'client2-slot2' -TimeoutSeconds 10
    $rosterDrop=[ordered]@{slot=2;pid=$rosterProcesses.client2.Id;requested_utc=$requested.ToString('o');exited_utc=[DateTime]::UtcNow.ToString('o');exit_code=$rosterProcesses.client2.ExitCode;graceful=$true;result_before_close=$true};Write-Utf8NoBom -Path (Join-Path $rosterRoot 'drop.json') -Text (($rosterDrop|ConvertTo-Json)+[Environment]::NewLine)
    [void](Wait-ForResults -HostResultPath (Roster-Path host 'result.json') -ClientResultPath (Roster-Path client1 'result.json') -HostProcess $rosterProcesses.host -ClientProcess $rosterProcesses.client1 -HostLogDirectory (Roster-Path host 'logs') -ClientLogDirectory (Roster-Path client1 'logs') -TimeoutSeconds $TimeoutSeconds)
    Request-OwnedProcessClose -Process $rosterProcesses.client1 -Role 'client1-slot1';Wait-ForCleanProcessExit -Process $rosterProcesses.client1 -Role 'client1-slot1' -TimeoutSeconds 10
    # Completed host's existing smoke lifecycle closes it only after the final
    # remaining client leaves. Wait for that real roster transition as well.
    Wait-ForCleanProcessExit -Process $rosterProcesses.host -Role host -TimeoutSeconds 10;$rosterLifecycle=$true
}catch{$rosterError=$_.Exception.Message;if($_.ScriptStackTrace){$rosterError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Roster: '+$rosterError)}
finally{
    foreach($n in $rosterSaved.Keys){[Environment]::SetEnvironmentVariable($n,$rosterSaved[$n],'Process')}
    foreach($peer in @('client2','client1','host')){if(-not (Stop-OwnedProcess -Process $rosterProcesses[$peer] -Role $peer)){$rosterClean=$false}}
}
if($rosterLifecycle -and $rosterClean){try{$rosterEvidence=Assert-RosterOutcome;Assert-Roster ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $rosterSha) 'Executable changed during owned run.';$rosterPass=$true}catch{$rosterError=$_.Exception.Message;if($_.ScriptStackTrace){$rosterError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL full-tail Roster validation: '+$rosterError)}}
if(-not $rosterClean){$rosterError+=' Owned cleanup failed.'};Ensure-Directory $rosterRoot
$summary=[ordered]@{status=$(if($rosterPass){'pass'}else{'fail'});error=$rosterError;fixture='FusionNativeRosterPlaying';run_id=$rosterRun;started_utc=$rosterStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');protocol=48;native_schema=13;
    executable=$GameExe;executable_sha256=$rosterSha;owned_processes=$rosterLedger;owned_cleanup=$rosterClean;graceful_lifecycle=$rosterLifecycle;departing_peer=$rosterDrop;outcome=$rosterEvidence;artifacts_root=$rosterRoot;maximum_games=3;
    limits='Playing drop only. Actual three slots join before selection, source27 paid birth from slot2, stable paused bank/cooldowns, inactive-owner birth provenance, eight fresh post-drop common messages, natural-CD transferred-card purchase and full tail after cleanup. Choosing/Intro drop, late observer/fourth peer, sourceNPC AI and NAT are Not executed.';
    visual_review='Root must inspect actual client bank/birth-owner/post-drop/reuse frames; nonblank pixels do not establish visual acceptance.'}
Write-Utf8NoBom -Path (Join-Path $rosterRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 12)+[Environment]::NewLine)
if($rosterPass){exit 0}else{exit 1}
