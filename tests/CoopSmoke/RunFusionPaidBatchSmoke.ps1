#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(120,600)][int]$TimeoutSeconds=180,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242,
    [string]$ValidateExisting='',
    [switch]$AllowSupersededExecutable
)
$ErrorActionPreference='Stop'
$paidHostWidth=$HostWindowWidth;$paidClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $paidHostWidth -ClientWindowWidth $paidClientWidth
if($HostWindowWidth -ne $paidHostWidth -or $ClientWindowWidth -ne $paidClientWidth){throw 'Shared helper changed requested widths.'}
$paidRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $paidRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$paidGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $paidRepo 'work\fusion\paid233'}
$paidRun=[Guid]::NewGuid().ToString('N');$paidRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$paidRun)
$paidHost=Join-Path $paidRoot 'host';$paidClient=Join-Path $paidRoot 'client';$paidHostLog=Join-Path $paidHost 'logs';$paidClientLog=Join-Path $paidClient 'logs'
$paidHostProcess=$null;$paidClientProcess=$null;$paidPassed=$false;$paidResultsReady=$false;$paidError='';$paidExeSha='';$paidSourceSha='';$paidFrames=@();$paidCommon=@();$paidEpochs=@();$paidEvidence=@()
$paidStarted=[DateTime]::UtcNow;$paidEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$paidEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_PAID_BATCH','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $paidEnvironment.ContainsKey($name)){$paidEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}}
function Assert-Paid([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Get-PaidFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    for($g=0;$g -lt 2;++$g){$suffixes=if($g -eq 0){@('chooser','paid_11','paid_16','complete')}else{@('chooser','parent','fused','complete')}
        foreach($suffix in $suffixes){$path=Join-Path $Directory ('fusion_paid_'+$g+'_'+$suffix+'.bmp')
            Assert-Paid (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual native paid frame: '+$path);$bmp=[Drawing.Bitmap]::new($path)
            try{Assert-Paid ($bmp.Width -eq $Width -and $bmp.Height -eq 600) 'Paid viewport dimensions differ.'
                $colors=New-Object 'System.Collections.Generic.HashSet[int]';for($x=16;$x -lt $bmp.Width;$x+=37){for($y=16;$y -lt $bmp.Height;$y+=31){[void]$colors.Add($bmp.GetPixel($x,$y).ToArgb())}}
                Assert-Paid ($colors.Count -ge 8) 'Blank actual native paid frame.'
            }finally{$bmp.Dispose()}
            [ordered]@{role=$Role;group=$g;frame=$suffix;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
        }
    }
}
function Assert-PaidOutcome([string]$RunRoot,[string]$Executable,[string]$SourcePath,[string]$ExeSha,[string]$SourceSha,[uint32]$ExpectedSeed,[int]$HostWidth,[int]$ClientWidth,[bool]$AllowSupersededExecutable=$false){
    $paidHost=Join-Path $RunRoot 'host';$paidClient=Join-Path $RunRoot 'client';$paidHostLog=Join-Path $paidHost 'logs';$paidClientLog=Join-Path $paidClient 'logs'
    $GameExe=$Executable;$sourcePath=$SourcePath;$paidExeSha=$ExeSha;$paidSourceSha=$SourceSha;$Seed=$ExpectedSeed;$paidHostWidth=$HostWidth;$paidClientWidth=$ClientWidth
    $paidEvidence=@();$paidEpochs=@();$paidFrames=@();$paidCommon=@()
    if(-not $AllowSupersededExecutable){Assert-Paid ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $paidExeSha) 'Saved/runtime executable identity differs.'}
    Assert-Paid ((Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash -eq $paidSourceSha) 'Saved/runtime catalog identity differs.'
    $results=@((Read-JsonIfComplete (Join-Path $paidHost 'result.json')),(Read-JsonIfComplete (Join-Path $paidClient 'result.json')))
    $definitions=@{};foreach($p in (Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8|ConvertFrom-Json)){$definitions[[int]$p.id]=$p.stats}
    $baseKinds=@(8,9,11,13,14,16,17,24,27,31);$total=0
    foreach($kind in @($baseKinds+@(21,1088))){Assert-Paid ($definitions.ContainsKey($kind) -and [int]$definitions[$kind].maxHealth -gt 0) 'Typed source definition missing.'}
    foreach($kind in $baseKinds){$total += [int]$definitions[$kind].sunCost};Assert-Paid ($total -eq 950) 'Inspected ten-base source cost total changed.'
    Assert-Paid ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Paired actual paid results missing.'
    foreach($r in $results){$samples=if($r.role -eq 'host'){[int]$r.snapshotsSent}else{[int]$r.snapshotsApplied};Assert-Paid ($r.status -eq 'pass' -and [int]$r.protocol -eq 48 -and $r.mode -eq 'fusion_validation' -and $r.scenario -eq 'fusion_validation' -and [int]$r.level -eq 1 -and [uint32]$r.seed -eq $Seed -and -not [bool]$r.skipIntro -and $samples -ge 8) 'Strict protocol48/full intro paid result failed.'}
    $hostText=Get-CombinedLogText $paidHostLog;$clientText=Get-CombinedLogText $paidClientLog
    Assert-Paid ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual Fusion protocol48 handshake missing.'
    $peers=@();$peerEpochs=@()
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){$role=[string]$pair[0];$text=[string]$pair[1]
        Assert-Paid ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|host state rejected|encode rejected|preflight (?:failed|rejected)') ('Native paid failure: '+$role)
        $epochs=@([regex]::Matches($text,('SMOKE_PAID233_EPOCH role='+$role+' group=(\d+) epoch=(\d+) previous=(\d+)')))
        $rows=@([regex]::Matches($text,('SMOKE_PAID233_PURCHASE role='+$role+' group=(\d+) epoch=(\d+) index=(\d+) incoming=(\d+) kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) parents=(\d+),(\d+) HP=(\d+) cost=(\d+) sun=(\d+) cooldown=(\d+) column=(\d+) row=(\d+) layer=(\d+) nativeSDL=1')))
        Assert-Paid ($epochs.Count -eq 2 -and $rows.Count -eq 12) 'Two activities and twelve actual paid actors required.'
        $groups=@()
        for($g=0;$g -lt 2;++$g){$epoch=$epochs[$g].Groups[2].Value;$count=if($g -eq 0){10}else{2};$sun=if($g -eq 0){2500}else{500};$ids=@{};$tokens=@{};$groupRows=@($rows|Where-Object {[int]$_.Groups[1].Value -eq $g})
            Assert-Paid ([int]$epochs[$g].Groups[1].Value -eq $g -and [uint32]$epoch -gt [uint32]$epochs[$g].Groups[3].Value -and ($g -eq 0 -or $epochs[$g].Groups[3].Value -eq $epochs[0].Groups[2].Value) -and $groupRows.Count -eq $count) 'Native paid epoch/order/count differs.'
            Assert-Paid ($text -match ('SMOKE_PAID233_ENTRY role='+$role+' group='+$g+' epoch='+$epoch+' initialSun='+$sun+' initialSunOverride='+$(if($g -eq 0){1}else{0})+' elapsedFrames=[1-9]\d* naturalIntroCleanup=1 otherWorldWrites=0')) 'Natural intro delete wait/disclosed initial balance missing.'
            Assert-Paid ($text -match ('SMOKE_PAID233_BUDGET role='+$role+' group='+$g+' epoch='+$epoch+' sun='+$sun+' money=3000')) 'Both peers must observe the actual initial wallet.'
            $acks=@([regex]::Matches($text,('SMOKE_PAID233_CHOICE_ACK role='+$role+' group='+$g+' own=(\d+) dense=(\d+) scroll=([-+\deE.]+)')))
            Assert-Paid ($acks.Count -eq 5 -and [int]$acks[4].Groups[1].Value -eq 5 -and $text -match ('SMOKE_PAID233_POOL role='+$role+' group='+$g+' epoch='+$epoch+' distinct=10 normal=10 target=10 own=5')) 'Actual normal native chooser/full quota missing.'
            for($i=0;$i -lt $count;++$i){$m=$groupRows[$i];$incoming=if($g -eq 0){$baseKinds[$i]}elseif($i -eq 0){24}else{21};$kind=if($g -eq 1 -and $i -eq 1){1088}else{$incoming};$owner=if($g -eq 0){[int][Math]::Floor($i/5)}else{$i}
                $definition=$definitions[$incoming];$price=[int]$definition.sunCost;$sun-=$price;$cd=[int][Math]::Round([double]$definition.rechargeSeconds*100);$layer=if($kind -eq 27){1}elseif($kind -in @(24,1088)){2}else{0};$col=if($g -eq 0){2*($i%5)}else{5};$row=if($g -eq 0){if($i -lt 5){0}else{4}}else{2}
                Assert-Paid ($m.Groups[2].Value -eq $epoch -and [int]$m.Groups[3].Value -eq $i -and [int]$m.Groups[4].Value -eq $incoming -and [int]$m.Groups[5].Value -eq $kind -and [int]$m.Groups[9].Value -eq $owner -and [int]$m.Groups[12].Value -eq [int]$definitions[$kind].maxHealth -and [int]$m.Groups[13].Value -eq $price -and [int]$m.Groups[14].Value -eq $sun -and [int]$m.Groups[15].Value -eq $cd -and [int]$m.Groups[16].Value -eq $col -and [int]$m.Groups[17].Value -eq $row -and [int]$m.Groups[18].Value -eq $layer) 'Paid actor source stats/cost/CD/layer/owner/placement differs.'
                Assert-Paid ([int]$m.Groups[10].Value -eq $(if($g -eq 1 -and $i -eq 1){24}else{65535}) -and [int]$m.Groups[11].Value -eq $(if($g -eq 1 -and $i -eq 1){21}else{65535})) 'Actual ordered recipe ancestry differs.'
                Assert-Paid ([uint32]$m.Groups[6].Value -gt 0 -and [uint32]$m.Groups[7].Value -gt 0 -and [uint32]$m.Groups[8].Value -gt 0 -and -not $ids.ContainsKey($m.Groups[6].Value) -and -not $tokens.ContainsKey($m.Groups[7].Value)) 'Actual paid native generation/packet missing or reused.'
                $ids[$m.Groups[6].Value]=$true;$tokens[$m.Groups[7].Value]=$true
                $paidEvidence+=,[ordered]@{role=$role;group=$g;epoch=$epoch;index=$i;incoming=$incoming;kind=$kind;id=$m.Groups[6].Value;token=$m.Groups[7].Value;packet=$m.Groups[8].Value;owner=$owner;cost=$price;sun=$sun;cooldown=$cd;layer=$layer}
            }
            Assert-Paid ($sun -eq $(if($g -eq 0){1550}else{250}) -and $text -match ('SMOKE_PAID233_CASE role='+$role+' group='+$g+' epoch='+$epoch+' purchases='+$count+' sun='+$sun+' money=3000 naturalRetirements='+$(if($g -eq 0){2}else{0})+' childHP='+$(if($g -eq 0){0}else{3996})+' postSetupActorWrites=0')) 'Scoped paid activity incomplete.'
            if($g -eq 0){foreach($index in @(2,5)){$m=$groupRows[$index];Assert-Paid ($text -match ('SMOKE_PAID233_RETIRED role='+$role+' group=0 epoch='+$epoch+' kind='+$baseKinds[$index]+' id='+$m.Groups[6].Value+' token='+$m.Groups[7].Value+' localGeneration=[1-9]\d* elapsed=-?\d+ natural=1')) 'Observed instant actor did not naturally retire.'}}
            else{$child=$groupRows[1];$before=[regex]::Match($text,('SMOKE_PAID233_BEFORE role='+$role+' group=1 epoch='+$epoch+' child='+$child.Groups[6].Value+' token='+$child.Groups[7].Value+' target=(\d+) body=3 max=3 childHP=4000 freshSnapshots=2'))
                Assert-Paid ($before.Success -and $text -match ('SMOKE_PAID233_REFLECT role='+$role+' group=1 epoch='+$epoch+' child='+$child.Groups[6].Value+' token='+$child.Groups[7].Value+' target='+$before.Groups[1].Value+' reflected=4 initialBody=3 dead=1 childHP=3996 originalBiteContinued=1 nativeCombat=1')) 'Legitimately crafted child must naturally reflect the real bite.'
                $resumed=[regex]::Match($text,('SMOKE_PAID233_RESUMED role='+$role+' group=1 epoch='+$epoch+' paused=0 samples=\d+'));$ack=[regex]::Match($text,('SMOKE_PAID233_RESUME_ACK role='+$role+' group=1 epoch='+$epoch+' peers=2'))
                Assert-Paid ($resumed.Success -and $ack.Success -and $before.Index -lt $resumed.Index -and $resumed.Index -lt $ack.Index -and $ack.Index -lt $text.IndexOf('SMOKE_PAID233_REFLECT role='+$role+' group=1 epoch='+$epoch+' ')) 'Actual unpaused-phase observation/peer acknowledgement before outcome is missing.'
            }
            $groups+=,@($groupRows)
        }
        Assert-Paid ($text -match ('SMOKE_PAID233_COMPLETE role='+$role+' activities=2 baseKinds=10 legitimate1088=1 actualPurchases=12 nativeSDL=1 naturalInstants=2 fullReflection=4 childHP=3996 finalSun=250 postSetupActorWrites=0 sourceNPC=NotExecuted gold=NotExecuted equipment=NotExecuted')) 'Paid batch final result missing.'
        $peers+=,@($groups);$peerEpochs+=,@($epochs)
    }
    for($g=0;$g -lt 2;++$g){$epoch=$peerEpochs[0][$g].Groups[2].Value;Assert-Paid ($epoch -eq $peerEpochs[1][$g].Groups[2].Value) 'Peer paid epochs differ.'
        for($i=0;$i -lt $peers[0][$g].Count;++$i){foreach($j in 1..18){Assert-Paid ($peers[0][$g][$i].Groups[$j].Value -eq $peers[1][$g][$i].Groups[$j].Value) 'Host/client paid IDs/tokens/source data differ.'}
            $m=$peers[0][$g][$i];$ownerRole=if([int]$m.Groups[9].Value -eq 0){'host'}else{'client'};$text=if($ownerRole -eq 'host'){$hostText}else{$clientText}
            Assert-Paid ($text -match ('SMOKE_PAID233_HELD role='+$ownerRole+' group='+$g+' index='+$i+' incoming='+$m.Groups[4].Value+' packet='+$m.Groups[8].Value+' heldEcho=1 sun=')) 'Real owning peer held-server echo/input proof missing.'
        }
        $hs=$hostText.IndexOf('SMOKE_PAID233_PURCHASE role=host group='+$g+' ');$he=$hostText.IndexOf('SMOKE_PAID233_CASE role=host group='+$g+' ');$cs=$clientText.IndexOf('SMOKE_PAID233_PURCHASE role=client group='+$g+' ');$ce=$clientText.IndexOf('SMOKE_PAID233_CASE role=client group='+$g+' ')
        Assert-Paid ($hs -ge 0 -and $he -gt $hs -and $cs -ge 0 -and $ce -gt $cs) 'Actual paid activity evidence interval absent.'
        $tx=@(Get-SnapshotHashes -Text $hostText.Substring($hs,$he-$hs) -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText.Substring($cs,$ce-$cs) -Marker SMOKE_SNAPSHOT_APPLIED);$common=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
        Assert-Paid ($common.Count -ge 2) 'Each native paid activity needs common applied snapshots.';$paidEpochs+=,[ordered]@{group=$g;epoch=$epoch;common_hashes=$common}
    }
    $g0epoch=$peerEpochs[0][0].Groups[2].Value
    $g0start=$hostText.IndexOf('SMOKE_PAID233_ENTRY role=host group=0 epoch='+$g0epoch+' ');$g0end=$hostText.IndexOf('SMOKE_PAID233_CASE role=host group=0 epoch='+$g0epoch+' ')
    Assert-Paid ($g0start -ge 0 -and $g0end -gt $g0start) 'Actual activity0 initializer interval missing.'
    $g0text=$hostText.Substring($g0start,$g0end-$g0start);$instantProof=@()
    foreach($index in @(2,5)){$m=$peers[0][0][$index];$kind=$baseKinds[$index]
        $initializers=@([regex]::Matches($g0text,('Fusion native Batch init id=(\d+) kind='+$kind+' speed=([-+\deE.]+) launch=-?\d+ special=(\d+)')))
        $instants=@([regex]::Matches($g0text,('SMOKE_PAID233_INSTANT kind='+$kind+' id='+$m.Groups[6].Value+' token='+$m.Groups[7].Value+' remaining=(\d+) sourceTicks=(\d+) speed=([-+\deE.]+) natural=1')))
        Assert-Paid ($initializers.Count -eq 1 -and $instants.Count -eq 1) 'Paid instant needs exactly one initializer and one actual-ID finite-counter observation in activity0.'
        $init=$initializers[0];$instant=$instants[0]
        # Source initialization precedes Board network-ID assignment. Its id0
        # is correlated by kind, identical float output and full initial ticks;
        # the later observation and purchase supply the nonzero ID/full token.
        Assert-Paid (($init.Groups[1].Value -eq '0' -or $init.Groups[1].Value -eq $m.Groups[6].Value) -and $init.Index -lt $instant.Index -and
            $init.Groups[2].Value -eq $instant.Groups[3].Value -and [int]$init.Groups[3].Value -eq [int]$instant.Groups[2].Value -and
            [int]$instant.Groups[1].Value -gt 0 -and [int]$instant.Groups[1].Value -le [int]$instant.Groups[2].Value) 'Paid instant initializer speed/counter/identity correlation differs.'
        $instantProof+=,[ordered]@{kind=$kind;id=[int]$m.Groups[6].Value;token=[uint32]$m.Groups[7].Value;initializer_id=[int]$init.Groups[1].Value;initial_ticks=[int]$init.Groups[3].Value;observed_remaining=[int]$instant.Groups[1].Value;source_ticks=[int]$instant.Groups[2].Value;speed=$instant.Groups[3].Value;initializer_count=1;observation_count=1;activity=0}
    }
    $child=$peers[0][1][1];Assert-Paid ([regex]::Matches($hostText,('Fusion native Defender bite plant='+$child.Groups[6].Value+' kind=1088 target=\d+ bite=4 reflect=4 slow=0 levels=0 warm=0 tick=\d+')).Count -eq 1) 'Real full-reflection callback must occur exactly once for the paid child.'
    Assert-Paid ([regex]::Matches($hostText,'SMOKE_PAID233_NPC_SETUP id=\d+ generation=\d+ row=2 body=3 max=3 x=[-+\deE.]+ y=[-+\deE.]+ vx=[-+\deE.]+ initialOnly=1').Count -eq 1) 'Exactly one disclosed initial native biter setup required.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$paidCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique);Assert-Paid ($paidCommon.Count -ge 8) 'Insufficient actual common native snapshots.'
    $paidFrames=@(Get-PaidFrames $paidHost host $paidHostWidth; Get-PaidFrames $paidClient client $paidClientWidth);Assert-Paid ($paidFrames.Count -eq 16) 'Sixteen actual paid renderer frames required.'
    if(-not $AllowSupersededExecutable){Assert-Paid ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $paidExeSha) 'Executable changed during owned run.'}
    Assert-Paid ((Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash -eq $paidSourceSha) 'Source catalog changed during owned run.'
    return [pscustomobject]@{frames=@($paidFrames);common_hashes=@($paidCommon);per_activity=@($paidEpochs);purchases=@($paidEvidence);instant_proof=@($instantProof);host_snapshots=[int]$results[0].snapshotsSent;client_snapshots=[int]$results[1].snapshotsApplied;frame_count=$paidFrames.Count;
        executable_identity=[ordered]@{status=$(if($AllowSupersededExecutable){'Skipped'}else{'Passed'});recorded_sha256=$paidExeSha;reason=$(if($AllowSupersededExecutable){'Current executable superseded by an authorized post-run build; no current binary rehash is presented as the old run identity.'}else{'Actual executable SHA matched before and after validation.'})};catalog_identity='Passed'}
}

Assert-Paid (-not $AllowSupersededExecutable -or $ValidateExisting) 'Superseded executable mode is only allowed for explicit saved-artifact validation.'
if($ValidateExisting){
    $existingRoot=Get-NormalizedPath $ValidateExisting;$summaryPath=Join-Path $existingRoot 'summary.json'
    $oldSummaryHash=(Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash;$old=Read-JsonIfComplete $summaryPath
    Assert-Paid ($null -ne $old) 'Saved paid summary missing/incomplete.'
    Assert-Paid (Test-SafeRunRoot -RunRoot $existingRoot -ArtifactsRoot (Split-Path -Parent $existingRoot) -RunId ([string]$old.run_id)) 'Unsafe existing paid run root.'
    $postPath=Join-Path $existingRoot 'postvalidation.json';Assert-Paid (-not (Test-Path -LiteralPath $postPath)) 'Refusing to overwrite saved postvalidation evidence.'
    $validated=$false;$validationError='';$identity=@();$outcome=$null
    try{
        Assert-Paid ($old.fixture -eq 'FusionNativePaidBatch' -and [int]$old.protocol -eq 48 -and [int]$old.native_schema -eq 13 -and $old.owned_cleanup -and
            ($old.status -eq 'pass' -or ($old.status -eq 'fail' -and $old.error -like 'Paid instant must initialize exactly once with a finite real callback counter.*'))) 'Saved run is not a completed, cleaned paid fixture with the known initializer-log mismatch.'
        Assert-Paid ((Get-NormalizedPath $old.executable) -eq $GameExe -and [int]$old.requested_host_width -in @(800,1066) -and [int]$old.requested_client_width -in @(800,1066)) 'Saved executable/viewport identity differs.'
        $ledger=@(Read-JsonIfComplete (Join-Path $existingRoot 'pids.json'));Assert-Paid ($ledger.Count -eq 2) 'Exactly two original owned process records required.'
        foreach($role in @('host','client')){$entry=@($ledger|Where-Object {$_.role -eq $role});Assert-Paid ($entry.Count -eq 1 -and [int]$entry[0].pid -gt 0 -and (Get-NormalizedPath $entry[0].executable) -eq $GameExe -and
                [DateTime]::Parse($entry[0].started_utc).ToUniversalTime() -ge [DateTime]::Parse($old.started_utc).ToUniversalTime().AddSeconds(-2) -and [DateTime]::Parse($entry[0].started_utc).ToUniversalTime() -le [DateTime]::Parse($old.finished_utc).ToUniversalTime()) 'Original owned PID/path/start identity differs.'
            $identity+=,[ordered]@{role=$role;pid=[int]$entry[0].pid;started_utc=$entry[0].started_utc;executable=$entry[0].executable}
        }
        $outcome=Assert-PaidOutcome -RunRoot $existingRoot -Executable $GameExe -SourcePath (Join-Path $paidRepo 'assets\fusion\catalog\plants.json') -ExeSha $old.executable_sha256 -SourceSha $old.source_catalog_sha256 -ExpectedSeed $Seed -HostWidth ([int]$old.requested_host_width) -ClientWidth ([int]$old.requested_client_width) -AllowSupersededExecutable ([bool]$AllowSupersededExecutable)
        $validated=$true
    }catch{$validationError=$_.Exception.Message;if($_.ScriptStackTrace){$validationError+=' | '+$_.ScriptStackTrace}}
    $summaryUnchanged=(Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash -eq $oldSummaryHash
    if(-not $summaryUnchanged){$validated=$false;$validationError+=' Original summary hash changed.'}
    $post=[ordered]@{status=$(if($validated){'pass'}else{'fail'});validation='existing-artifacts-only';error=$validationError;original_harness_status=$old.status;original_harness_error=$old.error;
        original_summary_sha256=$oldSummaryHash;original_summary_unchanged=$summaryUnchanged;run_id=$old.run_id;executable=$GameExe;executable_sha256=$old.executable_sha256;source_catalog_sha256=$old.source_catalog_sha256;
        owned_process_identity=$identity;owned_cleanup=$old.owned_cleanup;outcome=$outcome;spawned=0;staged=0;renamed=0;visual_review='Root pending; no new rendering';scope='Same final result/log/hash/frame/identity assertions for the completed saved paired run; no new game execution.'}
    Write-Utf8NoBom -Path $postPath -Text (($post|ConvertTo-Json -Depth 12)+[Environment]::NewLine)
    Assert-Paid $validated ('Saved paid validation failed: '+$validationError)
    Write-Output ('Fusion paid saved-artifact validation Passed; host='+$outcome.host_snapshots+' client='+$outcome.client_snapshots+' common='+$outcome.common_hashes.Count+' frames='+$outcome.frame_count+'; '+$postPath)
    return
}

try{
    $sourcePath=Join-Path $paidRepo 'assets\fusion\catalog\plants.json'
    foreach($path in @($GameExe,(Join-Path $paidGame 'main.pak'),(Join-Path $paidGame 'SDL3.dll'),$sourcePath)){Assert-Paid (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime/source input missing: '+$path)}
    $paidExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash;$paidSourceSha=(Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    $definitions=@{};foreach($p in (Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8|ConvertFrom-Json)){$definitions[[int]$p.id]=$p.stats}
    $baseKinds=@(8,9,11,13,14,16,17,24,27,31);$total=0
    foreach($kind in @($baseKinds+@(21,1088))){Assert-Paid ($definitions.ContainsKey($kind) -and [int]$definitions[$kind].maxHealth -gt 0) 'Typed source definition missing.'}
    foreach($kind in $baseKinds){$total += [int]$definitions[$kind].sunCost};Assert-Paid ($total -eq 950) 'Inspected ten-base source cost total changed.'
    foreach($path in @($paidHostLog,$paidClientLog,(Join-Path $paidHost 'appdata'),(Join-Path $paidClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort}
    foreach($name in $paidEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_PAID_BATCH='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$paidHost}else{$paidClient};$width=if($role -eq 'host'){$paidHostWidth}else{$paidClientWidth};$nick=if($role -eq 'host'){'PaidBatchHost'}else{'PaidBatchClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nick -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nick -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native paid10 bases + actual24+21 craft, two fresh activities; '+$paidRoot)
    $paidHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $paidGame -ManifestPath (Join-Path $paidHost 'smoke.ini') -Role host -RunId $paidRun -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $paidRoot 'pids.json') -Executable $GameExe -HostProcess $paidHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $paidHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $paidHostProcess -Role host -TimeoutSeconds 120
    $paidClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $paidGame -ManifestPath (Join-Path $paidClient 'smoke.ini') -Role client -RunId $paidRun -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $paidRoot 'pids.json') -Executable $GameExe -HostProcess $paidHostProcess -ClientProcess $paidClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $paidHost 'result.json') -ClientResultPath (Join-Path $paidClient 'result.json') -HostProcess $paidHostProcess -ClientProcess $paidClientProcess -HostLogDirectory $paidHostLog -ClientLogDirectory $paidClientLog -TimeoutSeconds $TimeoutSeconds)
    $paidResultsReady=$true
}catch{$paidError=$_.Exception.Message;if($_.ScriptStackTrace){$paidError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL PaidBatch: '+$paidError)}
finally{
    foreach($name in $paidEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$paidEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $paidClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $paidHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$paidPassed=$false;$paidError+=' Owned cleanup failed.'};Ensure-Directory $paidRoot
}
# Read the complete tails only after both owned processes have stopped. A
# successful result cannot hide an encoder/preflight failure during departure.
if($paidResultsReady -and $clientStopped -and $hostStopped){
    try{
        $outcome=Assert-PaidOutcome -RunRoot $paidRoot -Executable $GameExe -SourcePath $sourcePath -ExeSha $paidExeSha -SourceSha $paidSourceSha -ExpectedSeed $Seed -HostWidth $paidHostWidth -ClientWidth $paidClientWidth
        $paidEvidence=@($outcome.purchases);$paidEpochs=@($outcome.per_activity);$paidFrames=@($outcome.frames);$paidCommon=@($outcome.common_hashes);$paidPassed=$true
    }catch{$paidError=$_.Exception.Message;if($_.ScriptStackTrace){$paidError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL full-tail PaidBatch validation: '+$paidError)}
}
$summary=[ordered]@{status=$(if($paidPassed){'pass'}else{'fail'});error=$paidError;fixture='FusionNativePaidBatch';run_id=$paidRun;started_utc=$paidStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
        executable=$GameExe;executable_sha256=$paidExeSha;source_catalog_sha256=$paidSourceSha;protocol=48;native_schema=13;requested_host_width=$paidHostWidth;requested_client_width=$paidClientWidth;
        base_kinds=@(8,9,11,13,14,16,17,24,27,31);legitimate_recipe=@(24,21,1088);native_activities=2;paid_purchases=$paidEvidence;common_snapshot_hashes=$paidCommon;per_activity=$paidEpochs;frames=$paidFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);full_tail_validated_after_cleanup=$paidPassed;artifacts_root=$paidRoot;
        limits='Paid access/native identities/stat/cost/CD/layer checks plus natural Doom/Jalapeno retirement and legitimate CactusPumpkin full bite reflection. One disclosed initial SUN2500 in activity0; one initial NORMAL biter position/HP3 setup in activity1. No later actor/HP/timer/RNG/velocity writes. Core combat matrices remain separate evidence. SourceNPC94, Gold/upgrade workflow, equipment and other recipient abilities are Not executed.';
        visual_review='Root must view actual chooser/instant/parent/fused/completion frames; nonblank BMP is not visual acceptance.'}
Write-Utf8NoBom -Path (Join-Path $paidRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 10)+[Environment]::NewLine)
if($paidPassed){exit 0}else{exit 1}
