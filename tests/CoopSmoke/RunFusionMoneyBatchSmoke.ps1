#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(120,3600)][int]$TimeoutSeconds=240,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$moneyHostWidth=$HostWindowWidth;$moneyClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $moneyHostWidth -ClientWindowWidth $moneyClientWidth
if($HostWindowWidth -ne $moneyHostWidth -or $ClientWindowWidth -ne $moneyClientWidth){throw 'Shared helper changed requested widths.'}
$moneyRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $moneyRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$moneyGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $moneyRepo 'work\fusion-money-batch'}
$moneyRunId=[Guid]::NewGuid().ToString('N');$moneyRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$moneyRunId)
$moneyHost=Join-Path $moneyRoot 'host';$moneyClient=Join-Path $moneyRoot 'client';$moneyHostLog=Join-Path $moneyHost 'logs';$moneyClientLog=Join-Path $moneyClient 'logs'
$moneyHostProcess=$null;$moneyClientProcess=$null;$moneyPassed=$false;$moneyFullTail=$false;$moneyError='';$moneyExeSha='';$moneyFrames=@();$moneyCommon=@();$moneyPost=@();$moneyStarted=[DateTime]::UtcNow;$moneyEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$moneyEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_MONEY_BATCH','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $moneyEnvironment.ContainsKey($name)){$moneyEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}}
$moneyIncoming=@(31,31,31,31,26,31,28,31,32);$moneyResult=@(31,31,1140,31,1134,31,1138,31,1141)
$moneyExtra=@(0,1,0,1,0,0,0,1,0);$moneyOwners=@(0,1,0,1,1,0,1,1,1);$moneyCosts=@(25,50,25,50,100,25,100,50,300);$moneySun=@(2475,2425,2400,2350,2250,2225,2125,2075,1775)
function Assert-Money([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Assert-MoneyFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    foreach($name in @('chooser','paid','silver','producers','complete')){
        $path=Join-Path $Directory ('fusion_moneybatch_'+$name+'.bmp');Assert-Money (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual native Money frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try{
            Assert-Money ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for($x=16;$x -lt $bitmap.Width;$x+=37){for($y=16;$y -lt $bitmap.Height;$y+=31){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
            Assert-Money ($colors.Count -ge 8) 'Blank native renderer frame.'
        }finally{$bitmap.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try{
    foreach($path in @($GameExe,(Join-Path $moneyGame 'main.pak'),(Join-Path $moneyGame 'SDL3.dll'))){Assert-Money (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)}
    $moneyExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($path in @($moneyHostLog,$moneyClientLog,(Join-Path $moneyHost 'appdata'),(Join-Path $moneyClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort};foreach($name in $moneyEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_MONEY_BATCH='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$moneyHost}else{$moneyClient};$width=if($role -eq 'host'){$moneyHostWidth}else{$moneyClientWidth};$nickname=if($role -eq 'host'){'MoneyHost'}else{'MoneyClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native Money5:9 real purchases/fresh hybrids; '+$moneyRoot)
    $moneyHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $moneyGame -ManifestPath (Join-Path $moneyHost 'smoke.ini') -Role host -RunId $moneyRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $moneyRoot 'pids.json') -Executable $GameExe -HostProcess $moneyHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $moneyHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $moneyHostProcess -Role host -TimeoutSeconds 120
    $moneyClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $moneyGame -ManifestPath (Join-Path $moneyClient 'smoke.ini') -Role client -RunId $moneyRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $moneyRoot 'pids.json') -Executable $GameExe -HostProcess $moneyHostProcess -ClientProcess $moneyClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $moneyHost 'result.json') -ClientResultPath (Join-Path $moneyClient 'result.json') -HostProcess $moneyHostProcess -ClientProcess $moneyClientProcess -HostLogDirectory $moneyHostLog -ClientLogDirectory $moneyClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Money ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Paired native result identity missing.'
    foreach($result in $results){$samples=if($result.role -eq 'host'){[int]$result.snapshotsSent}else{[int]$result.snapshotsApplied};Assert-Money ($result.status -eq 'pass' -and [int]$result.protocol -eq 48 -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Strict protocol48/full intro result failed.'}
    $hostText=Get-CombinedLogText $moneyHostLog;$clientText=Get-CombinedLogText $moneyClientLog
    Assert-Money ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Real protocol48/Fusion handshake missing.'
    Assert-Money ([regex]::Matches($hostText,'MONEY234_INITIAL_CONTROL role=host sun=2500 once=1 money=3000 productionClocksUntouched=1').Count -eq 1) 'Exactly one declared initial SUN2500 setup required.'
    $paidByPeer=@();$beforeByPeer=@();$afterByPeer=@();$outcomeByPeer=@();$creditByPeer=@();$shotsByPeer=@()
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){
        $role=[string]$pair[0];$text=[string]$pair[1];Assert-Money ($text -notmatch 'SMOKE_FAIL|seq-gap|invalid snapshot|host state rejected|secondWorld=1') ('Runtime failure in '+$role)
        Assert-Money ($text -match ('MONEY234_POOL role='+$role+' eligible=\d+ target=10 quotas=5,5 ownFirst=1')) 'Actual personal native chooser/owner order missing.'
        $paid=@([regex]::Matches($text,('MONEY234_PURCHASE role='+$role+' epoch=(\d+) step=(\d+) incoming=(\d+) extra=([01]) result=(\d+) id=(\d+) token=(\d+) parentId=(\d+) parentToken=(\d+) parents=(\d+),(\d+) owner=(\d+) packet=(\d+) cost=(\d+) sun=(\d+) HP=300 cooldown=750 nativeSDL=1')))
        Assert-Money ($paid.Count -eq 9) 'Nine actual paid purchases required.'
        for($i=0;$i -lt 9;++$i){
            $buy=$paid[$i];$hybrid=$moneyResult[$i] -ne 31
            Assert-Money ([int]$buy.Groups[2].Value -eq $i -and [int]$buy.Groups[3].Value -eq $moneyIncoming[$i] -and [int]$buy.Groups[4].Value -eq $moneyExtra[$i] -and [int]$buy.Groups[5].Value -eq $moneyResult[$i] -and [uint32]$buy.Groups[6].Value -gt 0 -and [uint32]$buy.Groups[7].Value -gt 0 -and [int]$buy.Groups[10].Value -eq $(if($hybrid){31}else{65535}) -and [int]$buy.Groups[11].Value -eq $(if($hybrid){$moneyIncoming[$i]}else{65535}) -and [int]$buy.Groups[12].Value -eq $moneyOwners[$i] -and [uint32]$buy.Groups[13].Value -gt 0 -and [int]$buy.Groups[14].Value -eq $moneyCosts[$i] -and [int]$buy.Groups[15].Value -eq $moneySun[$i]) 'Source recipe/incoming variant/owner/price/cooldown/SUN differs.'
            if($hybrid){Assert-Money ($buy.Groups[8].Value -eq $paid[$i-1].Groups[6].Value -and $buy.Groups[9].Value -eq $paid[$i-1].Groups[7].Value -and $buy.Groups[6].Value -ne $buy.Groups[8].Value -and $buy.Groups[7].Value -ne $buy.Groups[9].Value) 'Paid hybrid did not replace its actual fresh31 parent.'}else{Assert-Money ([int]$buy.Groups[8].Value -eq 0 -and [int]$buy.Groups[9].Value -eq 0) 'Base31 fabricated parent identity.'}
            Assert-Money ($buy.Groups[1].Value -eq $paid[0].Groups[1].Value) 'Activity changed during paid chain.'
        }
        $before=@([regex]::Matches($text,('MONEY234_ACTOR role='+$role+' when=before kind=(\d+) id=(\d+) token=(\d+) owner=(\d+) packet=(\d+) parents=(\d+),(\d+) HP=300 alias=(\d+)')))
        $after=@([regex]::Matches($text,('MONEY234_ACTOR role='+$role+' when=after kind=(\d+) id=(\d+) token=(\d+) owner=(\d+) packet=(\d+) parents=(\d+),(\d+) HP=300 alias=(\d+)')))
        Assert-Money ($before.Count -eq 5 -and $after.Count -eq 5) 'Five actual paid native/source actor matrices missing.'
        $finalSteps=@(0,2,4,6,8);for($i=0;$i -lt 5;++$i){$buy=$paid[$finalSteps[$i]];Assert-Money ([int]$before[$i].Groups[1].Value -eq $moneyResult[$finalSteps[$i]] -and $before[$i].Groups[2].Value -eq $buy.Groups[6].Value -and $before[$i].Groups[3].Value -eq $buy.Groups[7].Value -and $before[$i].Groups[4].Value -eq $buy.Groups[12].Value -and $before[$i].Groups[5].Value -eq $buy.Groups[13].Value) 'Final paid actor lost provenance.';foreach($j in 1..8){Assert-Money ($before[$i].Groups[$j].Value -eq $after[$i].Groups[$j].Value) 'Paid actor changed identity/HP/ownership during natural gameplay.'}}
        foreach($kind in @(31,1140)){
            $index=if($kind -eq 31){0}else{1};$actor=$before[$index].Groups[2].Value;$count=if($kind -eq 31){1}else{2}
            Assert-Money ($text -match ('MONEY234_CALLBACK role='+$role+' kind='+$kind+' actor='+$actor+' callbacks=2 count='+$count)) 'Natural source producer did not complete two callbacks.'
            $cycles=@([regex]::Matches($text,('MONEY234_CYCLE role='+$role+' kind='+$kind+' actor='+$actor+' cycle=(\d+) count='+$count)))
            Assert-Money ($cycles.Count -ge 2) 'Natural producer source cycle resets missing.'
            if($role -eq 'host'){foreach($cycle in $cycles){Assert-Money ([int]$cycle.Groups[1].Value -ge $(if($kind -eq 31){2300}else{1800}) -and [int]$cycle.Groups[1].Value -le $(if($kind -eq 31){2700}else{2200})) 'Actual authoritative producer interval differs.'}}
        }
        $shots=@([regex]::Matches($text,('MONEY234_PROJECTILE role='+$role+' id=(\d+) payload=(74|76|78|81) damage=(\d+) type=(\d+) motion=(\d+) enhanced=0 sun=0')))
        foreach($payload in @(74,76,78,81)){$damage=if($payload -eq 76){40}elseif($payload -eq 81){100}else{60};Assert-Money (@($shots|Where-Object {[int]$_.Groups[2].Value -eq $payload -and [int]$_.Groups[3].Value -eq $damage}).Count -ge 1) 'Natural Silver payload/source damage missing.'}
        $outcomes=@([regex]::Matches($text,('MONEY234_OUTCOME role='+$role+' index=(\d+) id=(\d+) row=(\d+) body=(\d+) max=10000')))
        Assert-Money ($outcomes.Count -eq 3) 'Actual native natural NPC outcomes missing.'
        for($i=0;$i -lt 3;++$i){Assert-Money ([int]$outcomes[$i].Groups[1].Value -eq $i -and [int]$outcomes[$i].Groups[3].Value -eq ($i+2) -and [int]$outcomes[$i].Groups[4].Value -lt 10000 -and [int]$outcomes[$i].Groups[4].Value -gt 0) 'Natural controlled NPC outcome invalid.'}
        $credit=[regex]::Match($text,('MONEY234_CREDIT role='+$role+' id=(\d+) delta=25 money=3025 sun=1775 profileUnchanged=1'));Assert-Money $credit.Success 'Exactly separated Money25 credit missing.'
        Assert-Money ($text -match ('MONEY234_COMPLETE role='+$role+' epoch='+$paid[0].Groups[1].Value+' kinds=31,1140,1134,1138,1141 purchases=9 cost=725 sun=1775 money=3025 credit25=1 profileUnchanged=1 producersCount=1,2 payloads=74,76,78,81 postSetupWrites=0 nativeBoard=1 secondWorld=0 sourceZombieAIUnported=1')) 'Strict five-kind paid completion missing.'
        $paidByPeer+=,@($paid);$beforeByPeer+=,@($before);$afterByPeer+=,@($after);$outcomeByPeer+=,@($outcomes);$creditByPeer+=,$credit;$shotsByPeer+=,@($shots)
    }
    for($i=0;$i -lt 9;++$i){foreach($j in 1..15){Assert-Money ($paidByPeer[0][$i].Groups[$j].Value -eq $paidByPeer[1][$i].Groups[$j].Value) 'Peer canonical paid transaction differs.'}}
    for($i=0;$i -lt 5;++$i){foreach($j in 1..8){Assert-Money ($beforeByPeer[0][$i].Groups[$j].Value -eq $beforeByPeer[1][$i].Groups[$j].Value -and $afterByPeer[0][$i].Groups[$j].Value -eq $afterByPeer[1][$i].Groups[$j].Value) 'Peer paid source actor matrix differs.'}}
    for($i=0;$i -lt 3;++$i){foreach($j in 1..4){Assert-Money ($outcomeByPeer[0][$i].Groups[$j].Value -eq $outcomeByPeer[1][$i].Groups[$j].Value) 'Peer paused natural NPC result differs.'}}
    foreach($payload in @(74,76,78,81)){
        $kind=if($payload -eq 74){1134}elseif($payload -eq 81){1141}else{1138};$index=if($kind -eq 1134){2}elseif($kind -eq 1138){3}else{4};$actor=$beforeByPeer[0][$index].Groups[2].Value
        $fires=@([regex]::Matches($hostText,('Fusion native Lobber fire plant='+$actor+' kind='+$kind+' projectile=(\d+) payload='+$payload+' damage=(\d+)'))|ForEach-Object {$_.Groups[1].Value})
        $hostIds=@($shotsByPeer[0]|Where-Object {[int]$_.Groups[2].Value -eq $payload -and $fires -contains $_.Groups[1].Value}|ForEach-Object {$_.Groups[1].Value})
        Assert-Money (@($shotsByPeer[1]|Where-Object {[int]$_.Groups[2].Value -eq $payload -and $hostIds -contains $_.Groups[1].Value}).Count -ge 1) 'Paid source producer/projectile ID was not observed on both peers.'
        $damage=if($payload -eq 76){40}elseif($payload -eq 81){100}else{60}
        Assert-Money ([regex]::Matches($hostText,('Fusion native Lobber primary projectile=\d+ payload='+$payload+' target=\d+ damage='+$damage+' flags=1 coldAfter=0')).Count -ge 1) 'Natural Silver actual primary damage callback missing.'
    }
    foreach($kind in @(31,1140)){
        $index=if($kind -eq 31){0}else{1};$actor=$beforeByPeer[0][$index].Groups[2].Value;$count=if($kind -eq 31){1}else{2}
        $rewards=@([regex]::Matches($hostText,('Fusion native Money producer plant='+$actor+' kind='+$kind+' index=(\d+) count='+$count+' draw=(\d+) item=(\d+) value=(\d+) emitted=1 tick=(\d+)')))
        Assert-Money ($rewards.Count -ge (2*$count)) 'Two real producer batches were not emitted.'
        foreach($reward in $rewards){$draw=[int]$reward.Groups[2].Value;$item=if($draw -lt 20){35}elseif($draw -gt 94){36}else{34};$value=if($item -eq 35){300}elseif($item -eq 36){1000}else{100};Assert-Money ($draw -ge 0 -and $draw -lt 100 -and [int]$reward.Groups[3].Value -eq $item -and [int]$reward.Groups[4].Value -eq $value) 'Source random reward mapping differs.'}
        $batches=@($rewards|Group-Object {$_.Groups[5].Value});foreach($batch in $batches){$indices=@($batch.Group|ForEach-Object {[int]$_.Groups[1].Value}|Sort-Object -Unique);Assert-Money ($batch.Count -eq $count -and $indices.Count -eq $count -and $indices[0] -eq 0 -and $indices[-1] -eq ($count-1)) 'Producer callback did not emit its exact one/two items.'}
    }
    $coin=$creditByPeer[0].Groups[1].Value;Assert-Money ($coin -eq $creditByPeer[1].Groups[1].Value) 'Peer credited Money25 identity differs.'
    Assert-Money ([regex]::Matches($clientText,('MONEY234_CLICK role=client id='+$coin+' value=25 point=\d+,\d+ singleSDL=1 settledSnapshots=\d+ margin=8')).Count -eq 1) 'Exactly one real settled client coin click required.'
    Assert-Money ($hostText -match ('Fusion native Money emit item=38 coin='+$coin+' value=25 type=\d+[^\r\n]*typed=1')) 'Credited Money25 lacks actual SilverButter item38 provenance.'
    Assert-Money ([regex]::Matches($hostText,'Fusion native Money credit coin=\d+ value=\d+ before=\d+ after=\d+[^\r\n]*host=1 profile=0 sun=0').Count -eq 1 -and $hostText -match ('Fusion native Money credit coin='+$coin+' value=25 before=3000 after=3025')) 'Source money was not credited exactly once.'
    foreach($delta in @(40,60,100)){Assert-Money ($hostText -match ('MONEY234_HIT index=\d+ target=\d+ delta='+$delta+' body=\d+ mask=\d+')) 'Real native target HP delta missing.'}
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$moneyCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique);Assert-Money ($moneyCommon.Count -ge 8) 'Insufficient common actual native snapshots.'
    $hs=$hostText.IndexOf('SMOKE_MONEY234_PURCHASE role=host epoch='+$paidByPeer[0][8].Groups[1].Value+' step=8 ');$cs=$clientText.IndexOf('SMOKE_MONEY234_PURCHASE role=client epoch='+$paidByPeer[1][8].Groups[1].Value+' step=8 ')
    Assert-Money ($hs -ge 0 -and $cs -ge 0) 'Final actual crafting boundary missing.'
    $tx=@(Get-SnapshotHashes -Text $hostText.Substring($hs) -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText.Substring($cs) -Marker SMOKE_SNAPSHOT_APPLIED);$moneyPost=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique);Assert-Money ($moneyPost.Count -ge 8) 'Final paid world lacks common applied snapshots.'
    $moneyFrames=@((Assert-MoneyFrames $moneyHost host $moneyHostWidth),(Assert-MoneyFrames $moneyClient client $moneyClientWidth));Assert-Money ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $moneyExeSha) 'Executable changed during owned run.';$moneyPassed=$true
}catch{$moneyError=$_.Exception.Message;if($_.ScriptStackTrace){$moneyError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL MoneyBatch: '+$moneyError)}
finally{
    foreach($name in $moneyEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$moneyEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $moneyClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $moneyHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$moneyPassed=$false;$moneyError+=' Owned cleanup failed.'};Ensure-Directory $moneyRoot
}
# Result files precede roster teardown. Validate the complete trace only after
# both owned processes exit, including native snapshot admission/encode errors.
if($moneyPassed -and $clientStopped -and $hostStopped){
    try{
        foreach($pair in @(@('host',$moneyHostLog),@('client',$moneyClientLog))){$tail=Get-CombinedLogText ([string]$pair[1]);Assert-Money ($tail -notmatch 'Access Violation|\bSMOKE_FAIL\b|\bSMOKE_STOPPED\b|seq-gap|invalid snapshot|host state rejected|encode rejected|preflight (?:failed|rejected)|secondWorld=1|fatal:|protocol violation|queue overflow|assertion failed|unhandled exception') ('Full native lifecycle failure after cleanup: '+$pair[0])}
        Assert-Money ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $moneyExeSha) 'Executable changed before full-tail acceptance.';$moneyFullTail=$true
    }catch{$moneyPassed=$false;$moneyError=$_.Exception.Message;if($_.ScriptStackTrace){$moneyError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL full-tail MoneyBatch validation: '+$moneyError)}
}
$summary=[ordered]@{status=$(if($moneyPassed){'pass'}else{'fail'});error=$moneyError;fixture='FusionNativePaidMoneyBatch';source_kinds=@(31,1140,1134,1138,1141);run_id=$moneyRunId;started_utc=$moneyStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$moneyExeSha;protocol=48;native_schema=13;requested_host_width=$moneyHostWidth;requested_client_width=$moneyClientWidth;common_snapshot_hashes=$moneyCommon;postcraft_hashes=$moneyPost;frames=$moneyFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);full_tail_validated_after_cleanup=$moneyFullTail;artifacts_root=$moneyRoot;native_activities=1;paid_transactions=9;initial_fixture_sun=2500;final_sun=1775;source_money=3025;source_zombie_AI_accepted=0;
        limits='One declared initial SUN2500 control; all five source plants obtained by nine actual paid base/recipe placements, full native intro and personal card cooldowns. Three stationary native NORMAL NPCs initialized once at canonical lane Y; natural producer/corn randomness, projectiles, impacts and one real client Money25 click. No gold-upgrade/skill workflow, source94-zombie AI, full probability distribution, all equipment or aura variants claim. Root must inspect actual renderer frames.'}
Write-Utf8NoBom -Path (Join-Path $moneyRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 9)+[Environment]::NewLine)
if($moneyPassed){exit 0}else{exit 1}
