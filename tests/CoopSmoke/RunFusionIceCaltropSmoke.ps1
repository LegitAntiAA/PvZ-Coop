#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=180,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242,
    [switch]$PreflightOnly,
    [string]$ValidateExisting=''
)
$ErrorActionPreference='Stop'
$iceHostWidth=$HostWindowWidth;$iceClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $iceHostWidth -ClientWindowWidth $iceClientWidth
if($HostWindowWidth -ne $iceHostWidth -or $ClientWindowWidth -ne $iceClientWidth){throw 'Shared helper changed requested widths.'}
$iceRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $iceRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$iceGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $iceRepo 'work\fusion-icecaltrop'}
$script:iceChecks=0
function Assert-Ice([bool]$Condition,[string]$Message){++$script:iceChecks;if(-not $Condition){throw $Message}}
function Assert-IceInputs {
    foreach($path in @($GameExe,(Join-Path $iceGame 'main.pak'),(Join-Path $iceGame 'SDL3.dll'),(Join-Path $iceGame 'fusion\pilot\icecaltrop\animation.json'),(Join-Path $iceGame 'fusion\pilot\portrait-icecaltrop\animation.json'))){
        Assert-Ice (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $model=Get-Content -LiteralPath (Join-Path $iceGame 'fusion\pilot\icecaltrop\animation.json') -Raw -Encoding UTF8|ConvertFrom-Json
    Assert-Ice (@($model.ignoredBindings).Count -eq 0 -and @($model.omittedInactiveNodes).Count -eq 0) 'Strict source model contains omissions.'
}
Assert-IceInputs
if($PreflightOnly){Write-Output 'IceCaltrop runtime input preflight Passed; no processes started.';exit 0}
$iceRunId=[Guid]::NewGuid().ToString('N');$iceRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$iceRunId)
$iceHost=Join-Path $iceRoot 'host';$iceClient=Join-Path $iceRoot 'client'
$iceHostLog=Join-Path $iceHost 'logs';$iceClientLog=Join-Path $iceClient 'logs'
$iceHostProcess=$null;$iceClientProcess=$null;$iceHostStopped=$false;$iceClientStopped=$false
$icePassed=$false;$iceLifecycle=$false;$iceFullTail=$false;$iceError='';$iceExeSha='';$iceCommon=@();$icePost=@();$iceFrames=@();$iceStarted=[DateTime]::UtcNow;$iceEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$iceEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_ICE_CALTROP','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $iceEnvironment.ContainsKey($name)){$iceEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}}
function Assert-IceFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    foreach($name in @('chooser','playing','idle','attack','paused','complete')){
        $path=Join-Path $Directory ('fusion_icecaltrop_'+$name+'.bmp');Assert-Ice (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try{
            Assert-Ice ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual native viewport dimensions differ.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for($x=16;$x -lt $bitmap.Width;$x+=37){for($y=16;$y -lt $bitmap.Height;$y+=31){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
            Assert-Ice ($colors.Count -ge 8) ('Blank actual native frame: '+$path)
        }finally{$bitmap.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
if([string]::IsNullOrWhiteSpace($ValidateExisting)){
try{
    $iceExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($path in @($iceHostLog,$iceClientLog,(Join-Path $iceHost 'appdata'),(Join-Path $iceClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort}
    foreach($name in $iceEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_ICE_CALTROP='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$iceHost}else{$iceClient};$width=if($role -eq 'host'){$iceHostWidth}else{$iceClientWidth};$nickname=if($role -eq 'host'){'IceHost'}else{'IceClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native paid17+10 IceCaltrop1180; '+$iceRoot)
    $iceHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $iceGame -ManifestPath (Join-Path $iceHost 'smoke.ini') -Role host -RunId $iceRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $iceRoot 'pids.json') -Executable $GameExe -HostProcess $iceHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $iceHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $iceHostProcess -Role host -TimeoutSeconds 120
    $iceClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $iceGame -ManifestPath (Join-Path $iceClient 'smoke.ini') -Role client -RunId $iceRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $iceRoot 'pids.json') -Executable $GameExe -HostProcess $iceHostProcess -ClientProcess $iceClientProcess
    $iceResults=@(Wait-ForResults -HostResultPath (Join-Path $iceHost 'result.json') -ClientResultPath (Join-Path $iceClient 'result.json') -HostProcess $iceHostProcess -ClientProcess $iceClientProcess -HostLogDirectory $iceHostLog -ClientLogDirectory $iceClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Ice ($iceResults.Count -eq 2 -and $iceResults[0].role -eq 'host' -and $iceResults[1].role -eq 'client') 'Two actual native result identities missing.'
    foreach($result in $iceResults){$samples=if($result.role -eq 'host'){[int]$result.snapshotsSent}else{[int]$result.snapshotsApplied};Assert-Ice ($result.status -eq 'pass' -and [int]$result.protocol -eq 48 -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Strict protocol48/full native intro result failed.'}
    $iceLifecycle=$true
}catch{$iceError=$_.Exception.Message;if($_.ScriptStackTrace){$iceError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL IceCaltrop lifecycle: '+$iceError)}
finally{
    foreach($name in $iceEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$iceEnvironment[$name],'Process')}
    $iceClientStopped=Stop-OwnedProcess -Process $iceClientProcess -Role client;$iceHostStopped=Stop-OwnedProcess -Process $iceHostProcess -Role host
    if(-not $iceClientStopped -or -not $iceHostStopped){$iceLifecycle=$false;$iceError+=' Owned cleanup failed.'}
}
}
# Acceptance reads the full final files only after both owned processes stopped.
function Assert-IceOutcome {
        $hostText=Get-CombinedLogText $iceHostLog;$clientText=Get-CombinedLogText $iceClientLog
        Assert-Ice ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Real Fusion/protocol48 handshake missing.'
        Assert-Ice (@([regex]::Matches($hostText,'FIC251_AMBIENT initialOnly=1 initialSun=500 totalCost=175 finalExpected=325 sunWrites=0')).Count -eq 1) 'Initial ambient-only setup/budget proof differs.'
        $setups=@([regex]::Matches($hostText,'FIC251_SETUP index=(\d+) id=(\d+) generation=(\d+) row=(\d+) body=(\d+) helm=(\d+) shield=(\d+) level=(\d+) threshold=100 slow=(\d+) freeze=(\d+) contact=([01]) nativeNORMAL=1 fixtureInitialOnly=1'))
        Assert-Ice ($setups.Count -eq 5 -and @($setups|ForEach-Object {$_.Groups[2].Value}|Select-Object -Unique).Count -eq 5) 'Five distinct one-time native controls missing.'
        for($i=0;$i -lt 5;++$i){$q=$setups[$i];Assert-Ice ([int]$q.Groups[1].Value -eq $i -and [uint32]$q.Groups[3].Value -gt 0 -and [int]$q.Groups[4].Value -eq $(if($i -eq 3){3}else{2}) -and [int]$q.Groups[5].Value -eq $(if($i -eq 2){20}else{500}) -and [int]$q.Groups[6].Value -eq $(if($i -eq 0){100}else{0}) -and [int]$q.Groups[7].Value -eq $(if($i -eq 0){200}else{0}) -and [int]$q.Groups[8].Value -eq $(if($i -eq 2){90}else{0}) -and [int]$q.Groups[9].Value -eq $(if($i -lt 2){1400}else{0}) -and [int]$q.Groups[10].Value -eq $(if($i -eq 1){400}else{0})) 'Initial source cold/HP/armor vector differs.';if($i -lt 3){Assert-Ice ($q.Groups[11].Value -eq '1') 'Real native rectangle did not contact the source area.'};if($i -eq 4){Assert-Ice ($q.Groups[11].Value -eq '0') 'Outside control starts inside attack area.'}}
        $paidPeers=@();$outcomePeers=@()
        foreach($pair in @(@('host',$hostText),@('client',$clientText))){
            $role=[string]$pair[0];$text=[string]$pair[1]
            Assert-Ice ($text -notmatch 'Access Violation|\bSMOKE_FAIL\b|\bSMOKE_STOPPED\b|seq-gap|invalid snapshot|host state rejected|encode rejected|preflight (?:failed|rejected)|secondWorld=1|fatal:|protocol violation|queue overflow|assertion failed|unhandled exception') ('Full native lifecycle failure after cleanup: '+$role)
            Assert-Ice ($text -match ('FIC251_POOL role='+$role+' eligible=\d+ target=10 quotas=5,5 ownFirst=1 source17=1 source10=1') -and $text -match ('FIC251_READY role='+$role+' nativeSDL=1 hoverObserved=1')) 'Real native chooser/order/shared Ready missing.'
            $paid=@([regex]::Matches($text,('FIC251_PURCHASE role='+$role+' step=(\d+) incoming=(\d+) kind=(\d+) id=(\d+) token=(\d+) parentId=(\d+) parentToken=(\d+) owner=(\d+) packet=(\d+) cost=(\d+) sun=(\d+) HP=300 cooldown=(\d+) nativeSDL=1')))
            Assert-Ice ($paid.Count -eq 2) 'Two real canonical purchases missing.'
            for($i=0;$i -lt 2;++$i){$buy=$paid[$i];Assert-Ice ([int]$buy.Groups[1].Value -eq $i -and [int]$buy.Groups[2].Value -eq $(if($i){10}else{17}) -and [int]$buy.Groups[3].Value -eq $(if($i){1180}else{17}) -and [uint32]$buy.Groups[4].Value -gt 0 -and [uint32]$buy.Groups[5].Value -gt 0 -and [int]$buy.Groups[8].Value -eq $i -and [uint32]$buy.Groups[9].Value -gt 0 -and [int]$buy.Groups[10].Value -eq $(if($i){75}else{100}) -and [int]$buy.Groups[11].Value -eq $(if($i){325}else{400}) -and [int]$buy.Groups[12].Value -eq $(if($i){5000}else{750})) 'Actual source/incoming identity/owner/cost/cooldown differs.'}
            Assert-Ice ($paid[0].Groups[6].Value -eq '0' -and $paid[0].Groups[7].Value -eq '0' -and $paid[1].Groups[6].Value -eq $paid[0].Groups[4].Value -and $paid[1].Groups[7].Value -eq $paid[0].Groups[5].Value -and $paid[1].Groups[4].Value -ne $paid[0].Groups[4].Value -and $paid[1].Groups[5].Value -ne $paid[0].Groups[5].Value) '1180 did not replace the real17 actor with fresh identity.'
            $baseline=[regex]::Match($text,('FIC251_BASELINE role='+$role+' ids=(\d+),(\d+),(\d+),(\d+),(\d+) paused=1 visibleFullSnapshot=1 sun=325'));Assert-Ice ($baseline.Success) 'Initial visible full-snapshot barrier missing.'
            for($i=0;$i -lt 5;++$i){Assert-Ice ($baseline.Groups[$i+1].Value -eq $setups[$i].Groups[2].Value) 'Peer baseline actor IDs differ.'}
            $pause=[regex]::Match($text,('FIC251_PAUSE role='+$role+' slow=(\d+) freeze=(\d+) checks=(\d+) snapshots=(\d+) elapsedMs=(\d+) exactOwnCounters=1'))
            Assert-Ice ($pause.Success -and [int]$pause.Groups[2].Value -gt 0 -and [int]$pause.Groups[3].Value -ge 8 -and [int]$pause.Groups[4].Value -ge 2 -and [int]$pause.Groups[5].Value -ge 600) 'Frozen pause did not preserve actual counters through fresh network updates.'
            Assert-Ice ($text -match ('FIC251_RESUMED role='+$role+' observedUnpaused=1 beforeFinalRepause=1') -and $text -match ('FIC251_RETIRED role='+$role+' id='+$setups[2].Groups[2].Value+' firstTick=\d+ retiredTick=\d+ progress=[01] hostBounded=[01] canonicalOmission=1')) 'Actual unpaused/retirement observer missing.'
            $outcomes=@([regex]::Matches($text,('FIC251_OUTCOME role='+$role+' index=(\d+) id=(\d+) alive=([01]) body=(\d+) helm=(\d+) shield=(\d+) level=(\d+) threshold=(\d+) slow=(\d+) freeze=(\d+)')))
            Assert-Ice ($outcomes.Count -eq 5) 'Five stable final native outcomes missing.'
            for($i=0;$i -lt 5;++$i){$q=$outcomes[$i];Assert-Ice ([int]$q.Groups[1].Value -eq $i -and $q.Groups[2].Value -eq $setups[$i].Groups[2].Value -and [int]$q.Groups[3].Value -eq $(if($i -eq 2){0}else{1})) 'Final actor identity/liveness differs.';if($i -ge 3){Assert-Ice ([int]$q.Groups[4].Value -eq 500 -and [int]$q.Groups[5].Value -eq 0 -and [int]$q.Groups[6].Value -eq 0 -and [int]$q.Groups[7].Value -eq 0 -and [int]$q.Groups[9].Value -eq 0 -and [int]$q.Groups[10].Value -eq 0) 'Adjacent/outside native control changed.'}}
            Assert-Ice ($text -match ('FIC251_FINAL role='+$role+' kind=1180 id='+$paid[1].Groups[4].Value+' token='+$paid[1].Groups[5].Value+' sun=325 price17=100 price10=75 CD17=750 CD10=5000 postSetupHPWrites=0 postSetupStatusWrites=0 plantTimerWrites=0 nativeBoard=1 secondWorld=0') -and $text -match ('FIC251_COMPLETE role='+$role+' source1180=1 realPurchases=2 initialSun=500 totalCost=175 finalSun=325 damage20NoFrozenMultiplier=1 shieldBypass=1 postDamageCold10=1 lethalRetirement=1 pause=1 resumedACK=1 sourceZombieAIUnported=1')) 'Strict bounded source1180 completion missing.'
            foreach($m in @([regex]::Matches($text,'FIC251_RENDER role=\w+ frame=\w+ freshDraws=(\d+)'))){Assert-Ice ([int]$m.Groups[1].Value -ge 2) 'Renderer capture did not wait two real draws.'}
            $paidPeers+=,@($paid);$outcomePeers+=,@($outcomes)
        }
        for($i=0;$i -lt 2;++$i){foreach($g in 1..12){Assert-Ice ($paidPeers[0][$i].Groups[$g].Value -eq $paidPeers[1][$i].Groups[$g].Value) 'Peer paid provenance/card identity differs.'}}
        for($i=0;$i -lt 5;++$i){foreach($g in 1..10){Assert-Ice ($outcomePeers[0][$i].Groups[$g].Value -eq $outcomePeers[1][$i].Groups[$g].Value) 'Paused peer final canonical HP/status differs.'}}
        $armor=[regex]::Match($hostText,('FIC251_FIRST role=host index=0 id='+$setups[0].Groups[2].Value+' body=500 helm=80 shield=200 level=10 threshold=100 slow=(\d+) freeze=0 rate=0 hostExact=1'))
        $frozen=[regex]::Match($hostText,('FIC251_FIRST role=host index=1 id='+$setups[1].Groups[2].Value+' body=480 helm=0 shield=0 level=10 threshold=100 slow=(\d+) freeze=(\d+) rate=0 hostExact=1'))
        $lethal=[regex]::Match($hostText,('FIC251_FIRST role=host index=2 id='+$setups[2].Groups[2].Value+' body=0 helm=0 shield=0 level=0 threshold=200 slow=(\d+) freeze=(\d+) rate=([0-9.eE+\-]+) hostExact=1'))
        Assert-Ice ($armor.Success -and $frozen.Success -and $lethal.Success -and [int]$armor.Groups[1].Value -ge 997 -and [int]$armor.Groups[1].Value -le 1000 -and [int]$frozen.Groups[1].Value -ge 997 -and [int]$frozen.Groups[1].Value -le 1000 -and [int]$frozen.Groups[2].Value -gt 0 -and [int]$lethal.Groups[1].Value -ge 1397 -and [int]$lethal.Groups[2].Value -ge 397) 'Host first actual20/cold-order/frozen/no-multiplier vectors differ.'
        Assert-Ice ([single]::Parse($lethal.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture) -gt 0) 'Admitted lethal cold callback froze native death animation.'
        $retired=[regex]::Match($hostText,('FIC251_RETIRED role=host id='+$setups[2].Groups[2].Value+' firstTick=(\d+) retiredTick=(\d+) progress=1 hostBounded=1 canonicalOmission=1'))
        Assert-Ice ($retired.Success -and [int]$retired.Groups[2].Value-[int]$retired.Groups[1].Value -gt 0 -and [int]$retired.Groups[2].Value-[int]$retired.Groups[1].Value -lt 400) 'Lethal native dying actor did not progress and retire within4active seconds.'
        Assert-Ice ($hostText -match ('Fusion native cold add target='+$setups[2].Groups[2].Value+' amount=10 level=0 threshold=200 slow=1400 freeze=400 tick=\d+ retiring=1 hp=0') -and $hostText -match 'FIC251_FINAL_PAUSE bothResumedACK=1 bothRetiredACK=1 ownedSDL=1') 'Actual post-lethal continuation/final repause acknowledgment proof missing.'
        $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$iceCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
        $hi=$hostText.IndexOf('SMOKE_FIC251_RETIRED role=host');$ci=$clientText.IndexOf('SMOKE_FIC251_RETIRED role=client')
        $hr=$hostText.IndexOf('SMOKE_FIC251_RESUMED role=host');$cr=$clientText.IndexOf('SMOKE_FIC251_RESUMED role=client')
        Assert-Ice ($hr -ge 0 -and $cr -ge 0 -and $hi -gt $hr -and $ci -gt $cr) 'Natural retirement must follow each actual resume observer.'
        foreach($index in 0..2){Assert-Ice ($hostText.IndexOf('SMOKE_FIC251_FIRST role=host index='+$index) -ge 0 -and $hostText.IndexOf('SMOKE_FIC251_FIRST role=host index='+$index) -lt $hi -and $clientText.IndexOf('SMOKE_FIC251_FIRST role=client index='+$index) -ge 0 -and $clientText.IndexOf('SMOKE_FIC251_FIRST role=client index='+$index) -lt $ci) 'Postgameplay endpoint must follow all three actual strike observers.'}
        # Retirement is the last gameplay action. FINAL is intentionally later,
        # after the final pause has made every transmitted payload identical.
        # Keep two genuinely different applied payloads, never count repeats.
        $ht=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$ca=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED);$icePost=@($ca|Where-Object {$ht -contains $_}|Select-Object -Unique)
        Assert-Ice ($iceCommon.Count -ge 8 -and $icePost.Count -ge 2) 'Insufficient shared full/postgameplay distinct native snapshots.'
        $iceFrames=@((Assert-IceFrames $iceHost host $iceHostWidth),(Assert-IceFrames $iceClient client $iceClientWidth))
        Assert-Ice ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $iceExeSha) 'Executable changed before full-tail acceptance.'
        return [ordered]@{common=$iceCommon;post=$icePost;frames=$iceFrames}
}

if(-not [string]::IsNullOrWhiteSpace($ValidateExisting)){
    $iceRoot=Get-NormalizedPath $ValidateExisting;$originalPath=Join-Path $iceRoot 'summary.json'
    $originalSha=(Get-FileHash -LiteralPath $originalPath -Algorithm SHA256).Hash
    $original=Get-Content -LiteralPath $originalPath -Raw -Encoding UTF8|ConvertFrom-Json
    Assert-Ice ($original.status -eq 'fail' -and $original.error.StartsWith('Insufficient shared full/postaction native snapshots.') -and [bool]$original.owned_cleanup) 'Saved run is not the known completed harness-window failure.'
    Assert-Ice ($original.fixture -eq 'FusionNativePaidIceCaltrop' -and [int]$original.protocol -eq 48 -and [int]$original.native_schema -eq 13) 'Saved fixture/protocol identity differs.'
    Assert-Ice ((Get-NormalizedPath $original.artifacts_root) -eq $iceRoot -and (Get-NormalizedPath $original.executable) -eq $GameExe) 'Saved artifact/executable identity differs.'
    $iceRunId=[string]$original.run_id;$iceExeSha=[string]$original.executable_sha256
    Assert-Ice ((Split-Path -Leaf $iceRoot) -eq ('run_'+$iceRunId) -and (Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $iceExeSha) 'Saved executable/run-ID proof changed.'
    $iceHostWidth=[int]$original.requested_host_width;$iceClientWidth=[int]$original.requested_client_width
    $iceHost=Join-Path $iceRoot 'host';$iceClient=Join-Path $iceRoot 'client';$iceHostLog=Join-Path $iceHost 'logs';$iceClientLog=Join-Path $iceClient 'logs'
    $iceResults=@((Get-Content -LiteralPath (Join-Path $iceHost 'result.json') -Raw -Encoding UTF8|ConvertFrom-Json),(Get-Content -LiteralPath (Join-Path $iceClient 'result.json') -Raw -Encoding UTF8|ConvertFrom-Json))
    $Seed=[uint32]$iceResults[0].seed
    $decodedLedger=Get-Content -LiteralPath (Join-Path $iceRoot 'pids.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $ledger=@($decodedLedger)
    Assert-Ice ($ledger.Count -eq 2 -and @($ledger|ForEach-Object {$_.role}|Select-Object -Unique).Count -eq 2 -and @($ledger|ForEach-Object {$_.pid}|Select-Object -Unique).Count -eq 2) 'Saved two-peer ownership ledger invalid.'
    for($i=0;$i -lt 2;++$i){
        $role=if($i -eq 0){'host'}else{'client'};$result=$iceResults[$i];$own=@($ledger|Where-Object {$_.role -eq $role});$dir=if($i -eq 0){$iceHostLog}else{$iceClientLog}
        Assert-Ice ($own.Count -eq 1 -and [int]$own[0].pid -gt 0 -and (Get-NormalizedPath $own[0].executable) -eq $GameExe -and @((Get-ChildItem -LiteralPath $dir -Filter ('*_'+$role+'_'+$own[0].pid+'.log') -File)).Count -eq 1) 'Saved ledger PID/path/log identity differs.'
        [void][DateTime]::Parse($own[0].started_utc,[Globalization.CultureInfo]::InvariantCulture)
        $samples=if($i -eq 0){[int]$result.snapshotsSent}else{[int]$result.snapshotsApplied}
        Assert-Ice ($result.status -eq 'pass' -and $result.role -eq $role -and [int]$result.protocol -eq 48 -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Saved strict native result failed.'
    }
    $postPass=$false;$postError='';$evidence=$null
    try{$evidence=Assert-IceOutcome;Assert-Ice ((Get-FileHash -LiteralPath $originalPath -Algorithm SHA256).Hash -eq $originalSha) 'Original failed summary was modified.';$postPass=$true}
    catch{$postError=$_.Exception.Message;if($_.ScriptStackTrace){$postError+=' | '+$_.ScriptStackTrace}}
    $post=[ordered]@{status=$(if($postPass){'pass'}else{'fail'});error=$postError;mode='read_only_existing_run_validation';original_harness_status='fail';original_harness_error=[string]$original.error;original_summary_sha256_before=$originalSha;original_summary_sha256_after=(Get-FileHash -LiteralPath $originalPath -Algorithm SHA256).Hash;run_id=$iceRunId;executable=$GameExe;executable_sha256=$iceExeSha;protocol=48;native_schema=13;checks=$script:iceChecks;owned_cleanup=[bool]$original.owned_cleanup;ledger=$ledger;evidence=$evidence;
        scope='Same complete result/log/provenance/HP/cold/ACK/frames/full-tail checks. Postgameplay endpoint is actual natural animated lethal UID retirement after observed resume and all three strikes. Minimum8 common and2 genuinely distinct postgameplay payloads retained; repeated paused payloads are not counted as distinct. No processes started/stopped, no resources changed, original failed summary retained.'}
    Write-Utf8NoBom -Path (Join-Path $iceRoot 'postvalidation.json') -Text (($post|ConvertTo-Json -Depth 12)+[Environment]::NewLine)
    Write-Output ('IceCaltrop saved actual validation: '+$post.status+' checks='+$script:iceChecks)
    if($postPass){exit 0}else{Write-Output $postError;exit 1}
}
if($iceLifecycle){
    try{$evidence=Assert-IceOutcome;$iceCommon=$evidence.common;$icePost=$evidence.post;$iceFrames=$evidence.frames;$iceFullTail=$true;$icePassed=$true}
    catch{$iceError=$_.Exception.Message;if($_.ScriptStackTrace){$iceError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL full-tail IceCaltrop validation: '+$iceError)}
}

Ensure-Directory $iceRoot
$summary=[ordered]@{status=$(if($icePassed){'pass'}else{'fail'});error=$iceError;fixture='FusionNativePaidIceCaltrop';source_kind=1180;run_id=$iceRunId;started_utc=$iceStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$iceExeSha;protocol=48;native_schema=13;requested_host_width=$iceHostWidth;requested_client_width=$iceClientWidth;common_snapshot_hashes=$iceCommon;postaction_hashes=$icePost;postgameplay_endpoint='natural_animated_lethal_uid_retirement';validation_checks=$script:iceChecks;frames=$iceFrames;owned_cleanup=[bool]($iceClientStopped -and $iceHostStopped);full_tail_validated_after_cleanup=$iceFullTail;artifacts_root=$iceRoot;paid_transactions=2;initial_sun=500;total_cost=175;final_sun=325;sun_writes=0;postsetup_HP_writes=0;postsetup_status_writes=0;plant_timer_writes=0;
    limits='Controlled one-time native NORMAL HP/armor/cold cohort, frozen healthy ground-track pose only. Same native Board/Plant/Zombie, natural Caltrop counters/events, exact host first strike and lethal continuation; client full-snapshot provenance/outcomes, own paused counters, animated retirement and source frames. No source zombie AI, other NPC families/vehicles, 1060/1074/1075 obtaining/crash paths, full campaign, or NAT acceptance. Root must inspect saved actual frames.'}
Write-Utf8NoBom -Path (Join-Path $iceRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 9)+[Environment]::NewLine)
if($icePassed){exit 0}else{exit 1}
