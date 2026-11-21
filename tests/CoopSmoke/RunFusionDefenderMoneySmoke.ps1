#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(120,600)][int]$TimeoutSeconds=240,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$dmHostWidth=$HostWindowWidth;$dmClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $dmHostWidth -ClientWindowWidth $dmClientWidth
if($HostWindowWidth -ne $dmHostWidth -or $ClientWindowWidth -ne $dmClientWidth){throw 'Shared helper changed requested widths.'}
$dmRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $dmRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$dmGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $dmRepo 'work\fusion\defendermoney196'}
$dmRun=[Guid]::NewGuid().ToString('N');$dmRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$dmRun)
$dmHost=Join-Path $dmRoot 'host';$dmClient=Join-Path $dmRoot 'client'
$dmHostLog=Join-Path $dmHost 'logs';$dmClientLog=Join-Path $dmClient 'logs'
$dmHostProcess=$null;$dmClientProcess=$null;$dmPassed=$false;$dmError='';$dmFrames=@();$dmCommon=@();$dmPost=@();$dmSha=''
$dmStarted=[DateTime]::UtcNow;$dmEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*' -or $_.Name -in @('PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')}){$dmEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_DEFENDER_MONEY','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $dmEnvironment.ContainsKey($name)){$dmEnvironment[$name]=$null}}
function Assert-DM([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Get-DMActors([string]$Text,[string]$Role){
    $rows=@([regex]::Matches($Text,('SMOKE_DM196_ACTOR role='+$Role+' kind=(\d+) id=(\d+) token=(\d+) HP=(\d+) layer=(\d+) column=(\d+) row=(\d+) parents=(\d+),(\d+) pending=(\d+)')))
    Assert-DM ($rows.Count -eq 22) ($Role+' requires22 actual actor records.')
    $expected=@(1029,1048,1189,1027,1028,1039,1073,27,1112,1114,1125,1130,1133,24,1088,1089,1164,31,1140,1134,1138,1141)
    $ids=@{};$tokens=@{}
    for($i=0;$i -lt $rows.Count;++$i){$m=$rows[$i];$id=$m.Groups[2].Value;$token=$m.Groups[3].Value
        Assert-DM ([int]$m.Groups[1].Value -eq $expected[$i] -and [uint32]$id -gt 0 -and [uint32]$token -gt 0 -and -not $ids.ContainsKey($id) -and -not $tokens.ContainsKey($token)) 'Canonical scope or generation differs.'
        Assert-DM ([int]$m.Groups[4].Value -gt 0) 'Canonical maximum health missing.';$ids[$id]=$true;$tokens[$token]=$true
    };return $rows
}
function Assert-DMFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    foreach($name in @('matrix','effects','silver','money','body_crack','tall_crack','cover_crack','complete')){
        $path=Join-Path $Directory ('fusion_defendermoney_'+$name+'.bmp');Assert-DM (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bmp=[Drawing.Bitmap]::new($path)
        try{Assert-DM ($bmp.Width -eq $Width -and $bmp.Height -eq 600) 'Actual native viewport differs.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]';for($x=16;$x -lt $bmp.Width;$x+=37){for($y=16;$y -lt $bmp.Height;$y+=31){[void]$colors.Add($bmp.GetPixel($x,$y).ToArgb())}}
            Assert-DM ($colors.Count -ge 8) 'Actual renderer frame is blank.'
        }finally{$bmp.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try{
    foreach($path in @($GameExe,(Join-Path $dmGame 'main.pak'),(Join-Path $dmGame 'SDL3.dll'))){Assert-DM (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)}
    $dmSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($path in @($dmHostLog,$dmClientLog,(Join-Path $dmHost 'appdata'),(Join-Path $dmClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort}
    foreach($name in $dmEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_DEFENDER_MONEY='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$dmHost}else{$dmClient};$width=if($role -eq 'host'){$dmHostWidth}else{$dmClientWidth};$nickname=if($role -eq 'host'){'DefMoneyHost'}else{'DefMoneyClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Defender17/Money5 grouped native mechanics; two hidden owned windows; '+$dmRoot)
    $dmHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $dmGame -ManifestPath (Join-Path $dmHost 'smoke.ini') -Role host -RunId $dmRun -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $dmRoot 'pids.json') -Executable $GameExe -HostProcess $dmHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $dmHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $dmHostProcess -Role host -TimeoutSeconds 120
    $dmClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $dmGame -ManifestPath (Join-Path $dmClient 'smoke.ini') -Role client -RunId $dmRun -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $dmRoot 'pids.json') -Executable $GameExe -HostProcess $dmHostProcess -ClientProcess $dmClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $dmHost 'result.json') -ClientResultPath (Join-Path $dmClient 'result.json') -HostProcess $dmHostProcess -ClientProcess $dmClientProcess -HostLogDirectory $dmHostLog -ClientLogDirectory $dmClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-DM ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Paired native results missing.'
    foreach($r in $results){$samples=if($r.role -eq 'host'){[int]$r.snapshotsSent}else{[int]$r.snapshotsApplied}
        Assert-DM ($r.status -eq 'pass' -and [int]$r.protocol -eq 48 -and $r.mode -eq 'fusion_validation' -and $r.scenario -eq 'fusion_validation' -and [int]$r.level -eq 1 -and [uint32]$r.seed -eq $Seed -and -not [bool]$r.skipIntro -and $samples -ge 8) 'Strict protocol48/full-intro native fixture failed.'
    }
    $hostText=Get-CombinedLogText $dmHostLog;$clientText=Get-CombinedLogText $dmClientLog
    Assert-DM ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual Fusion handshake missing.'
    $ha=@(Get-DMActors $hostText host);$ca=@(Get-DMActors $clientText client)
    for($i=0;$i -lt 22;++$i){foreach($g in 1..10){Assert-DM ($ha[$i].Groups[$g].Value -eq $ca[$i].Groups[$g].Value) 'Cross-peer canonical actor fields differ.'}}
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){$role=[string]$pair[0];$text=[string]$pair[1]
        Assert-DM ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|secondWorld=1|credit rejected|credit omitted') ('Native fixture failure: '+$role)
        foreach($kind in @(1048,1088,1039,1189,1073)){Assert-DM ($text -match ('SMOKE_DM196_BITE role='+$role+' kind='+$kind+'\b')) 'Natural native bite effect missing.'}
        foreach($reflection in @(@(1048,2),@(1088,4))){Assert-DM ($text -match ('SMOKE_DM196_BITE role='+$role+' kind='+$reflection[0]+' plant=\d+ target=\d+ lethalReflect='+$reflection[1]+' victimDamage=4 originalBiteContinued=1')) 'Exact lethal reflection must preserve the original native bite.'}
        foreach($kind in @(1029,1073,24)){foreach($stage in @(1,2)){Assert-DM ($text -match ('SMOKE_DM196_CRACK role='+$role+' kind='+$kind+' id=\d+ stage='+$stage+'\b')) 'Natural representative damage stage missing.'}}
        Assert-DM ($text -match ('SMOKE_DM196_CRACK role='+$role+' kind=1039 id=\d+ stage=1\b')) 'Natural cold TallNut damage stage missing.'
        Assert-DM ($text -match ('SMOKE_DM196_POT role='+$role+' under=\d+ body=\d+ underHP=300 bodyHP=\d+ biteOrderProtected=1 nonbiteImmunityRuntime=0')) 'Actual native layered bite order missing.'
        foreach($payload in @(74,76,78,81)){Assert-DM ($text -match ('SMOKE_DM196_PROJECTILE role='+$role+' id=\d+ kind='+$payload+'\b')) 'Actual Silver payload missing.'}
        Assert-DM ($text -match ('SMOKE_DM196_CREDIT role='+$role+' id=\d+ delta=25 initial=3000 money=3025 sun=500 profileUnchanged=1')) 'Actual separate single money credit missing.'
        Assert-DM ($text -match ('SMOKE_DM196_COMPLETE role='+$role+' defenders=17 moneyKinds=5 canonicalActors=22 naturalFamilies=3 coldBiters=2 warm=1 reflect=2 potBiteProtection=1 credit25=1 money=3025 sun=500 profileUnchanged=1 controlledFactory=1 paidCrafts=0 postSetupHealthTimerWrites=0 equipmentAndRecipientExtrasDeferred=1 sourceZombieAIUnported=1')) 'Grouped native completion proof missing.'
    }
    Assert-DM ($hostText -match 'SMOKE_DM196_SETUP actors=22 sourceDefenders=17 moneyKinds=5 initialFactoryOnly=1 paidCrafts=0 actorHealthTimerWrites=0 RNGReseeds=0') 'Controlled initial setup was not disclosed.'
    Assert-DM ($hostText -match 'SMOKE_DM196_INITIAL_METADATA actors=22 packets=10 productionEncoder=1 metadataOnly=1 nativeBodyPreflight=0' -and $hostText -notmatch 'SMOKE_DM196_INVALID_FIXTURE|native Fusion snapshot encode rejected') 'Initial fixture metadata must pass the real production codec.'
    Assert-DM ($hostText -match 'SMOKE_DM196_COMBAT_SETUP targets=4 biters=7 initialPositionHealthColdAndRateOnly=1 healthWritesAfterSetup=0 timerWritesAfterSetup=0') 'Controlled NPC prerequisites were not disclosed.'
    foreach($binding in @(@(74,60),@(76,40),@(78,60),@(81,100))){Assert-DM ($hostText -match ('Fusion native Lobber primary projectile=\d+ payload='+$binding[0]+' target=\d+ damage='+$binding[1]+' flags=1 coldAfter=0')) 'Actual single-hit Silver damage missing.'}
    Assert-DM ($hostText -match 'Fusion native Lobber secondary projectile=\d+ payload=81 target=\d+ count=\d+ base=66 damage=66 flags=2 coldBefore=0') 'Actual enhanced Melon66 splash missing.'
    Assert-DM ($hostText -match 'SMOKE_DM196_POT_EFFECT kind=1134 under=1112 naturalCycle=\d+ interval=2') 'Actual CabbagePot natural interval missing.'
    foreach($binding in @(@(1039,0,1000),@(1073,1,3000))){
        $m=[regex]::Match($hostText,('Fusion native Defender pulse plant=\d+ kind='+$binding[0]+' element='+$binding[1]+' before=(\d+) after=(\d+) amount='+$binding[2]+' row=\d+ tick=\d+'))
        Assert-DM ($m.Success -and [int]$m.Groups[1].Value -gt 0 -and [int]$m.Groups[1].Value -lt 8000 -and [int]$m.Groups[1].Value+[int]$binding[2] -ge 8000 -and [int]$m.Groups[2].Value -eq 8000) 'Actual natural elemental recovery clamp missing.'
    }
    foreach($kind in @(31,1140)){
        $events=@([regex]::Matches($hostText,('Fusion native Money producer plant=\d+ kind='+$kind+' index=(\d+) count=(\d+) draw=(\d+) item=(\d+) value=(\d+) emitted=1 tick=(\d+)')))
        Assert-DM ($events.Count -ge $(if($kind -eq 31){2}else{4})) 'Two natural producer callbacks missing.'
        foreach($e in $events){$draw=[int]$e.Groups[3].Value;$item=if($draw -lt 20){35}elseif($draw -gt 94){36}else{34};$value=if($item -eq 35){300}elseif($item -eq 36){1000}else{100}
            Assert-DM ($draw -ge 0 -and $draw -lt 100 -and [int]$e.Groups[4].Value -eq $item -and [int]$e.Groups[5].Value -eq $value -and [int]$e.Groups[2].Value -eq $(if($kind -eq 31){1}else{2})) 'Actual producer draw/type/value mapping differs.'
        }
        $init=[regex]::Match($hostText,('Fusion native Producer init id=\d+ token=\d+ kind='+$kind+' hp=300 initialSeconds=([0-9.eE+-]+) counter=(\d+) interval=[0-9.]+ count=\d+ tick=(\d+)'))
        Assert-DM $init.Success 'Actual source producer initial deadline missing.'
        $seconds=[single]::Parse($init.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture)
        Assert-DM ([Math]::Ceiling([double]$seconds*100) -eq [int]$init.Groups[2].Value) 'Marigold inclusive float deadline differs.'
        $triggers=@([regex]::Matches($hostText,('Fusion native Producer trigger id=\d+ token=\d+ kind='+$kind+' count=\d+ cycle=(\d+) pending=50 tick=(\d+) nextDeadline=\d+')))
        Assert-DM ($triggers.Count -ge 2) 'Two natural producer trigger deadlines missing.'
        $previousTick=[int]$init.Groups[3].Value;$deadline=[int]$init.Groups[2].Value
        foreach($trigger in $triggers){$tick=[int]$trigger.Groups[2].Value;$cycle=[int]$trigger.Groups[1].Value
            Assert-DM ([Math]::Abs(($tick-$previousTick)-$deadline) -le 1) 'Producer natural counter advanced at the wrong cadence.'
            Assert-DM ($cycle -ge $(if($kind -eq 31){2300}else{1800}) -and $cycle -le $(if($kind -eq 31){2700}else{2200})) 'Marigold float-jitter cycle outside source bounds.'
            $previousTick=$tick;$deadline=$cycle
        }
        $callbacks=@($events|Group-Object {$_.Groups[6].Value})
        Assert-DM ($callbacks.Count -ge 2) 'Two distinct natural money emission ticks missing.'
        foreach($callback in $callbacks){$tick=[int]$callback.Name;$near=@($triggers|Where-Object {[Math]::Abs(($tick-[int]$_.Groups[2].Value)-50) -le 1})
            Assert-DM ($near.Count -eq 1 -and $callback.Count -eq $(if($kind -eq 31){1}else{2})) 'Natural pending50 callback count/timing differs.'
        }
    }
    $clicks=@([regex]::Matches($clientText,'SMOKE_DM196_COIN_CLICK role=client candidate=\d+ value=25 point=-?\d+,-?\d+ singleSDL=1 settledSnapshots=(\d+) margin=8'))
    Assert-DM ($clicks.Count -eq 1 -and [int]$clicks[0].Groups[1].Value -ge 2) 'Exactly one shared settled native SDL cash click required.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $dmCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_DM196_COMBAT_SETUP');$ci=$clientText.IndexOf('SMOKE_DM196_BITE role=client')
    Assert-DM ($hi -ge 0 -and $ci -ge 0) 'Postcombat hash boundary missing.'
    $hp=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$cp=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED)
    $dmPost=@($cp|Where-Object {$hp -contains $_}|Select-Object -Unique)
    Assert-DM ($dmCommon.Count -ge 8 -and $dmPost.Count -ge 2) 'Insufficient actual common/postcombat snapshots.'
    $dmFrames=@((Assert-DMFrames $dmHost host $dmHostWidth),(Assert-DMFrames $dmClient client $dmClientWidth))
    Assert-DM ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $dmSha) 'Owned executable changed during run.'
    $dmPassed=$true
}catch{$dmError=$_.Exception.Message;if($_.ScriptStackTrace){$dmError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL DefenderMoney: '+$dmError)}
finally{
    foreach($name in $dmEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$dmEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $dmClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $dmHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$dmPassed=$false;$dmError+=' Owned cleanup failed.'}
    Ensure-Directory $dmRoot
    $summary=[ordered]@{status=$(if($dmPassed){'pass'}else{'fail'});error=$dmError;fixture='FusionNativeDefenderMoney';run_id=$dmRun;started_utc=$dmStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
        executable=$GameExe;executable_sha256=$dmSha;protocol=48;native_schema=13;common_snapshot_hashes=$dmCommon;postcombat_common_hashes=$dmPost;frames=$dmFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$dmRoot;
        defender_profiles_initialized=17;money_kinds_initialized=5;initial_factory_actors=22;paid_crafts=0;all17_abilities_proven=$false;all40_gameplay_proven=$false;
        limits='Controlled initial actor/NPC setup and two later natural countdown actors. No postsetup HP/timer/RNG writes. Representative Body/Tall/Cover natural cracks and bite effects, actual natural fire/freeze recovery, Silver3 projectiles and shared pot effects, natural Money31/1140 callbacks, one settled client Money25 credit. Nonbite pot immunity, gear1027/1028 workflow, recipient extras, all random reward outcomes, all17 separate crack chains and source zombieAI remain unproven.';
        visual_review='Root must inspect both peers actual renderer frames; nonblank BMP is not visual correctness.'}
    Write-Utf8NoBom -Path (Join-Path $dmRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if($dmPassed){exit 0}else{exit 1}
