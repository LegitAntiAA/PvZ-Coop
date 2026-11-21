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
$ErrorActionPreference='Stop';$silverHostWidth=$HostWindowWidth;$silverClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $silverHostWidth -ClientWindowWidth $silverClientWidth
if($HostWindowWidth -ne $silverHostWidth -or $ClientWindowWidth -ne $silverClientWidth){throw 'Shared helper changed requested widths.'}
$silverRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $silverRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$silverGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $silverRepo 'work\fusion-silver-sunflower'}
$silverRunId=[Guid]::NewGuid().ToString('N');$silverRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$silverRunId)
$silverHost=Join-Path $silverRoot 'host';$silverClient=Join-Path $silverRoot 'client';$silverHostLog=Join-Path $silverHost 'logs';$silverClientLog=Join-Path $silverClient 'logs'
$silverHostProcess=$null;$silverClientProcess=$null;$silverHostStopped=$false;$silverClientStopped=$false;$silverPassed=$false;$silverFullTail=$false;$silverError='';$silverExeSha='';$silverFrames=@();$silverCommon=@();$silverPost=@();$silverStarted=[DateTime]::UtcNow;$silverEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$silverEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_SILVER_SUNFLOWER','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $silverEnvironment.ContainsKey($name)){$silverEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}}
function Assert-Silver([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Assert-SilverFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    foreach($name in @('chooser','fused','paused0','produced','paused1','complete')){
        $path=Join-Path $Directory ('fusion_silversunflower_'+$name+'.bmp');Assert-Silver (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try{Assert-Silver ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport mismatch.';$colors=New-Object 'System.Collections.Generic.HashSet[int]';for($x=16;$x -lt $bitmap.Width;$x+=37){for($y=16;$y -lt $bitmap.Height;$y+=31){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}};Assert-Silver ($colors.Count -ge 8) 'Blank actual renderer frame.'}finally{$bitmap.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try{
    foreach($path in @($GameExe,(Join-Path $silverGame 'main.pak'),(Join-Path $silverGame 'SDL3.dll'))){Assert-Silver (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime missing: '+$path)}
    $silverExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($path in @($silverHostLog,$silverClientLog,(Join-Path $silverHost 'appdata'),(Join-Path $silverClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort};foreach($name in $silverEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_SILVER_SUNFLOWER='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$silverHost}else{$silverClient};$width=if($role -eq 'host'){$silverHostWidth}else{$silverClientWidth};$nickname=if($role -eq 'host'){'SilverHost'}else{'SilverClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native paid1+31 SilverSunflower1187; '+$silverRoot)
    $silverHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $silverGame -ManifestPath (Join-Path $silverHost 'smoke.ini') -Role host -RunId $silverRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $silverRoot 'pids.json') -Executable $GameExe -HostProcess $silverHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $silverHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $silverHostProcess -Role host -TimeoutSeconds 120
    $silverClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $silverGame -ManifestPath (Join-Path $silverClient 'smoke.ini') -Role client -RunId $silverRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $silverRoot 'pids.json') -Executable $GameExe -HostProcess $silverHostProcess -ClientProcess $silverClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $silverHost 'result.json') -ClientResultPath (Join-Path $silverClient 'result.json') -HostProcess $silverHostProcess -ClientProcess $silverClientProcess -HostLogDirectory $silverHostLog -ClientLogDirectory $silverClientLog -TimeoutSeconds $TimeoutSeconds)
    # Result files are not full-lifecycle acceptance. Close both owned peers
    # before reading/validating the complete trace, including roster teardown.
    $silverClientStopped=Stop-OwnedProcess -Process $silverClientProcess -Role client;$silverHostStopped=Stop-OwnedProcess -Process $silverHostProcess -Role host
    Assert-Silver ($silverClientStopped -and $silverHostStopped) 'Owned process cleanup failed.'
    Assert-Silver ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Paired native results missing.'
    foreach($result in $results){$samples=if($result.role -eq 'host'){[int]$result.snapshotsSent}else{[int]$result.snapshotsApplied};Assert-Silver ($result.status -eq 'pass' -and [int]$result.protocol -eq 48 -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Strict protocol48/full intro result failed.'}
    $hostText=Get-CombinedLogText $silverHostLog;$clientText=Get-CombinedLogText $silverClientLog
    Assert-Silver ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Real protocol48/Fusion handshake missing.'
    Assert-Silver ([regex]::Matches($hostText,'SUN248_SETUP sun=500 money=3000 ambientOnly=1 sunWrites=0 actorWrites=0').Count -eq 1) 'One untouched initial ledger setup missing.'
    Assert-Silver ($hostText -notmatch 'Fusion native Producer emit [^\r\n]*kind=1\b|Fusion native Money (?:emit|credit)') 'A different producer/money source contaminated the sun case.'
    $paidByPeer=@();$pauseByPeer=@();$creditByPeer=@();$eligible=@()
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){
        $role=[string]$pair[0];$text=[string]$pair[1]
        Assert-Silver ($text -notmatch 'Access Violation|\bSMOKE_FAIL\b|\bSMOKE_STOPPED\b|seq-gap|invalid snapshot|host state rejected|encode rejected|preflight (?:failed|rejected)|secondWorld=1|fatal:|protocol violation|queue overflow|assertion failed|unhandled exception') ('Full native lifecycle failure after cleanup: '+$role)
        $pool=[regex]::Match($text,('SUN248_POOL role='+$role+' eligible=(\d+) target=10 quotas=5,5 ownFirst=1'));Assert-Silver ($pool.Success -and [int]$pool.Groups[1].Value -ge 10) 'Actual source eligibility/own-first chooser missing.';$eligible+=,[int]$pool.Groups[1].Value
        $paid=@([regex]::Matches($text,('SUN248_PURCHASE role='+$role+' epoch=(\d+) step=(\d+) kind=(\d+) incoming=(\d+) id=(\d+) token=(\d+) parentId=(\d+) parentToken=(\d+) owner=(\d+) packet=(\d+) cost=(\d+) sun=(\d+) HP=300 cooldown=750 alias=(\d+) nativeSDL=1')))
        Assert-Silver ($paid.Count -eq 2) 'Both actual paid purchases missing.'
        for($i=0;$i -lt 2;++$i){$buy=$paid[$i];Assert-Silver ([int]$buy.Groups[2].Value -eq $i -and [int]$buy.Groups[3].Value -eq $(if($i -eq 0){1}else{1187}) -and [int]$buy.Groups[4].Value -eq $(if($i -eq 0){1}else{31}) -and [uint32]$buy.Groups[5].Value -gt 0 -and [uint32]$buy.Groups[6].Value -gt 0 -and [int]$buy.Groups[9].Value -eq $i -and [uint32]$buy.Groups[10].Value -gt 0 -and [int]$buy.Groups[11].Value -eq $(if($i -eq 0){50}else{25}) -and [int]$buy.Groups[12].Value -eq $(if($i -eq 0){450}else{425})) 'Paid source/incoming owner/cost/SUN identity differs.'}
        Assert-Silver ([int]$paid[0].Groups[7].Value -eq 0 -and [int]$paid[0].Groups[8].Value -eq 0 -and $paid[1].Groups[7].Value -eq $paid[0].Groups[5].Value -and $paid[1].Groups[8].Value -eq $paid[0].Groups[6].Value -and $paid[1].Groups[5].Value -ne $paid[0].Groups[5].Value -and $paid[1].Groups[6].Value -ne $paid[0].Groups[6].Value -and $paid[0].Groups[1].Value -eq $paid[1].Groups[1].Value) 'Child did not replace its actual1 parent with a fresh generation.'
        $actor=$paid[1].Groups[5].Value;$epoch=$paid[1].Groups[1].Value
        $pauses=@([regex]::Matches($text,('SUN248_PAUSE role='+$role+' round=([01]) epoch='+$epoch+' launch=(\d+) pending=(\d+) card1=(\d+) time1=750 card31=(\d+) time31=750 stableMs=(\d+) snapshots=(\d+) sun=(\d+) money=3000')))
        Assert-Silver ($pauses.Count -eq 2) 'Both actual pause proofs missing.'
        for($i=0;$i -lt 2;++$i){Assert-Silver ([int]$pauses[$i].Groups[1].Value -eq $i -and [int]$pauses[$i].Groups[6].Value -ge 400 -and [int]$pauses[$i].Groups[7].Value -ge 2 -and [int]$pauses[$i].Groups[8].Value -eq $(if($i -eq 0){425}else{450})) 'Actual frozen source/card clocks or ledgers differ.'}
        Assert-Silver ([int]$pauses[0].Groups[3].Value -ge 1 -and [int]$pauses[0].Groups[3].Value -le 50) 'First actual pause must retain a pending callback.'
        Assert-Silver ($text -match ('SUN248_CALLBACK role='+$role+' actor='+$actor+' count=1 callbacks=2') -and [regex]::Matches($text,('SUN248_CYCLE role='+$role+' actor='+$actor+' cycle=\d+ cycles=\d+')).Count -ge 2) 'Two natural producer callbacks/cycles missing.'
        $resumed=$text.IndexOf('SMOKE_SUN248_RESUMED role='+$role+' round=0 epoch='+$epoch+' ');$acks=$text.IndexOf('SMOKE_SUN248_RESUME_ACKS role='+$role+' epoch='+$epoch+' host=1 client=1 beforeRepause=1');$repause=$text.IndexOf('SMOKE_SUN248_PAUSE role='+$role+' round=1 ')
        Assert-Silver ($resumed -ge 0 -and $acks -gt $resumed -and $repause -gt $acks) 'Both-peer observed RESUMED barrier must precede repause.'
        $credit=[regex]::Match($text,('SUN248_CREDIT role='+$role+' id=(\d+) before=425 after=450 value=25 money=3000 profileUnchanged=1'));Assert-Silver ($credit.Success -and [uint32]$credit.Groups[1].Value -gt 0) 'One actual source Sun25 credit missing.'
        Assert-Silver ($text -match ('SUN248_COIN role='+$role+' id='+$credit.Groups[1].Value+' value=25 child='+$actor+' token='+$paid[1].Groups[6].Value)) 'Credited native sun lacks child activity identity.'
        Assert-Silver ($text -match ('SUN248_COMPLETE role='+$role+' epoch='+$epoch+' id='+$actor+' token='+$paid[1].Groups[6].Value+' parents=1,31 owner=1 purchases=2 cost=75 sun=450 collected=25 money=3000 profileUnchanged=1 interval=15 count=1 callbacks=\d+ cycles=\d+ pauses=2 resumedBoth=1 sunWrites=0 actorWrites=0 nativeBoard=1 secondWorld=0')) 'Strict paid1187 completion missing.'
        $paidByPeer+=,@($paid);$pauseByPeer+=,@($pauses);$creditByPeer+=,$credit
    }
    Assert-Silver ($eligible[0] -eq $eligible[1]) 'Peer actual eligible pools differ.'
    for($i=0;$i -lt 2;++$i){foreach($j in 1..13){Assert-Silver ($paidByPeer[0][$i].Groups[$j].Value -eq $paidByPeer[1][$i].Groups[$j].Value) 'Peer canonical paid provenance differs.'}}
    for($round=0;$round -lt 2;++$round){
        # Pure producer clocks are exact FNS fields; seed cooldown counters use
        # the existing native U8 ratio projection, never equal peer tick counts.
        foreach($field in @(2,3,8)){Assert-Silver ($pauseByPeer[0][$round].Groups[$field].Value -eq $pauseByPeer[1][$round].Groups[$field].Value) 'Exact paused FNS producer/ledger state differs.'}
        foreach($field in @(4,5)){$raw=[int]$pauseByPeer[0][$round].Groups[$field].Value;$projected=[int][Math]::Floor([Math]::Floor([double]$raw*255.0/750.0)*750.0/255.0);Assert-Silver ([int]$pauseByPeer[1][$round].Groups[$field].Value -eq $projected) 'Paused native cooldown codec projection differs.'}
    }
    $child=$paidByPeer[0][1].Groups[5].Value;$token=$paidByPeer[0][1].Groups[6].Value
    $init=@([regex]::Matches($hostText,('Fusion native Producer init id=\d+ token='+$token+' kind=1187 hp=300 initialSeconds=([0-9.eE+\-]+) counter=(\d+) interval=15\.0 count=1 tick=\d+')));Assert-Silver ($init.Count -eq 1) 'Fresh1187 source initialization must occur once.'
    $initial=[single]::Parse($init[0].Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture);$initialTicks=[int][Math]::Floor([double]$initial*100.0)+1
    Assert-Silver ($initial -ge 4 -and $initial -le 7 -and [int]$init[0].Groups[2].Value -eq $initialTicks) 'Source4..7 initial counter formula differs.'
    $triggers=@([regex]::Matches($hostText,('Fusion native Producer trigger id='+$child+' token='+$token+' kind=1187 count=1 cycle=(\d+) pending=50 tick=(\d+) nextDeadline=(\d+)')))
    $emits=@([regex]::Matches($hostText,('Fusion native Producer emit id='+$child+' token='+$token+' kind=1187 count=1 value=25 sound=1 tick=(\d+) nextDeadline=\d+')))
    Assert-Silver ($triggers.Count -ge 2 -and $emits.Count -ge 2) 'Natural pending50/oneSun25 callbacks missing.'
    foreach($trigger in $triggers){Assert-Silver (@(1301,1401,1501,1601,1701) -contains [int]$trigger.Groups[1].Value) 'Source integer15+-2 cycle differs.';Assert-Silver ($trigger.Groups[1].Value -eq $trigger.Groups[3].Value) 'Authoritative trigger deadline differs.'}
    # Pause extends wall time, so callback proof uses pending50 and the actual
    # source emit after it, not a renderer tick or host/client index equality.
    for($i=0;$i -lt 2;++$i){Assert-Silver ([int]$emits[$i].Groups[1].Value -ge [int]$triggers[$i].Groups[2].Value) 'Source callback precedes its actual pending trigger.'}
    $coin=$creditByPeer[0].Groups[1].Value;Assert-Silver ($coin -eq $creditByPeer[1].Groups[1].Value) 'Peer collected sun IDs differ.'
    Assert-Silver ([regex]::Matches($clientText,('SUN248_CLICK role=client id='+$coin+' value=25 point=\d+,\d+ singleSDL=1 settledSnapshots=\d+ margin=8')).Count -eq 1) 'Exactly one actual settled client sun click required.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$silverCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique);Assert-Silver ($silverCommon.Count -ge 8) 'Insufficient common actual native snapshots.'
    $hs=$hostText.IndexOf('SMOKE_SUN248_PURCHASE role=host epoch='+$paidByPeer[0][1].Groups[1].Value+' step=1 ');$cs=$clientText.IndexOf('SMOKE_SUN248_PURCHASE role=client epoch='+$paidByPeer[1][1].Groups[1].Value+' step=1 ')
    Assert-Silver ($hs -ge 0 -and $cs -ge 0) 'Paid native child boundary missing.';$tx=@(Get-SnapshotHashes -Text $hostText.Substring($hs) -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText.Substring($cs) -Marker SMOKE_SNAPSHOT_APPLIED);$silverPost=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique);Assert-Silver ($silverPost.Count -ge 8) 'Paid1187 world lacks common applied snapshots.'
    $silverFrames=@((Assert-SilverFrames $silverHost host $silverHostWidth),(Assert-SilverFrames $silverClient client $silverClientWidth));Assert-Silver ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $silverExeSha) 'Executable changed during owned run.';$silverFullTail=$true;$silverPassed=$true
}catch{$silverError=$_.Exception.Message;if($_.ScriptStackTrace){$silverError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL SilverSunflower: '+$silverError)}
finally{
    foreach($name in $silverEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$silverEnvironment[$name],'Process')}
    if(-not $silverClientStopped){$silverClientStopped=Stop-OwnedProcess -Process $silverClientProcess -Role client};if(-not $silverHostStopped){$silverHostStopped=Stop-OwnedProcess -Process $silverHostProcess -Role host}
    if(-not $silverClientStopped -or -not $silverHostStopped){$silverPassed=$false;$silverError+=' Owned cleanup failed.'};Ensure-Directory $silverRoot
    $summary=[ordered]@{status=$(if($silverPassed){'pass'}else{'fail'});error=$silverError;fixture='FusionNativePaidSilverSunflower';source_kind=1187;run_id=$silverRunId;started_utc=$silverStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$silverExeSha;protocol=48;native_schema=13;requested_host_width=$silverHostWidth;requested_client_width=$silverClientWidth;common_snapshot_hashes=$silverCommon;postcraft_hashes=$silverPost;frames=$silverFrames;owned_cleanup=[bool]($silverClientStopped -and $silverHostStopped);full_tail_validated_after_cleanup=$silverFullTail;artifacts_root=$silverRoot;paid_transactions=2;initial_sun=500;paid_sun=425;final_sun=450;source_money=3000;sun_writes=0;postsetup_actor_writes=0;
        limits='Actual1+31 paid1187, incoming owner/parent retirement, source initial/cycle/pending and Sun25 callbacks, two native pauses with explicit both-peer resumed acknowledgement before repause, one real client sun click and exact existing seed cooldown codec projection. No source zombieAI, all equipment/aura/difficulty/probability coverage claim; Root must inspect source renderer frames. Complete native lifecycle trace is validated only after both owned processes exit.'}
    Write-Utf8NoBom -Path (Join-Path $silverRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 9)+[Environment]::NewLine)
}
if($silverPassed){exit 0}else{exit 1}
