#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(120,600)][int]$TimeoutSeconds=180,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$garlicHostWidth=$HostWindowWidth;$garlicClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $garlicHostWidth -ClientWindowWidth $garlicClientWidth
if($HostWindowWidth -ne $garlicHostWidth -or $ClientWindowWidth -ne $garlicClientWidth){throw 'Shared helper changed requested widths.'}
$garlicRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $garlicRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$garlicGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $garlicRepo 'work\fusion\garlic225'}
$garlicRun=[Guid]::NewGuid().ToString('N');$garlicRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$garlicRun)
$garlicHost=Join-Path $garlicRoot 'host';$garlicClient=Join-Path $garlicRoot 'client'
$garlicHostLog=Join-Path $garlicHost 'logs';$garlicClientLog=Join-Path $garlicClient 'logs'
$garlicHostProcess=$null;$garlicClientProcess=$null;$garlicPassed=$false;$garlicError='';$garlicFrames=@();$garlicCommon=@();$garlicPost=@();$garlicSha=''
$garlicStarted=[DateTime]::UtcNow;$garlicEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*' -or $_.Name -in @('PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')}){$garlicEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_GARLIC','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $garlicEnvironment.ContainsKey($name)){$garlicEnvironment[$name]=$null}}
function Assert-Garlic([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Get-GarlicPurchases([string]$Text,[string]$Role){
    $rows=@([regex]::Matches($Text,('SMOKE_GARLIC225_PURCHASE role='+$Role+' index=(\d+) kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) extra=([01]) parents=(\d+),(\d+) cost=(\d+) sun=(\d+) recharge=750 HP=300 nativeSDL=1')))
    Assert-Garlic ($rows.Count -eq 5) 'Five actual paid plant purchases required on each peer.'
    $kinds=@(27,29,1130,29,1);$costs=@(25,50,100,50,50);$suns=@(475,425,325,275,225);$owners=@(1,0,1,0,0);$ids=@{};$tokens=@{}
    for($i=0;$i -lt 5;++$i){$m=$rows[$i]
        Assert-Garlic ([int]$m.Groups[1].Value -eq $i -and [int]$m.Groups[2].Value -eq $kinds[$i] -and [int]$m.Groups[6].Value -eq $owners[$i] -and [int]$m.Groups[7].Value -eq $(if($i -eq 2){1}else{0}) -and [int]$m.Groups[10].Value -eq $costs[$i] -and [int]$m.Groups[11].Value -eq $suns[$i]) 'Paid source identity/extra/cost/SUN differs.'
        Assert-Garlic ([uint32]$m.Groups[3].Value -gt 0 -and [uint32]$m.Groups[4].Value -gt 0 -and [uint32]$m.Groups[5].Value -gt 0 -and -not $ids.ContainsKey($m.Groups[3].Value) -and -not $tokens.ContainsKey($m.Groups[4].Value)) 'Actual generation/packet identity missing or reused.'
        $ids[$m.Groups[3].Value]=$true;$tokens[$m.Groups[4].Value]=$true
        Assert-Garlic ([int]$m.Groups[8].Value -eq $(if($i -eq 2){27}else{65535}) -and [int]$m.Groups[9].Value -eq $(if($i -eq 2){29}else{65535})) 'Directional27+29 ancestry differs.'
    };return $rows
}
function Get-GarlicFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    foreach($name in @('paid','wait','move','repeat','pause','stall','face_recovered','complete')){
        $path=Join-Path $Directory ('fusion_garlic_'+$name+'.bmp');Assert-Garlic (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual Garlic frame: '+$path)
        $bmp=[Drawing.Bitmap]::new($path)
        try{Assert-Garlic ($bmp.Width -eq $Width -and $bmp.Height -eq 600) 'Actual viewport dimensions differ.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]';for($x=16;$x -lt $bmp.Width;$x+=37){for($y=16;$y -lt $bmp.Height;$y+=31){[void]$colors.Add($bmp.GetPixel($x,$y).ToArgb())}}
            Assert-Garlic ($colors.Count -ge 8) 'Actual renderer frame is blank.'
        }finally{$bmp.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try{
    foreach($path in @($GameExe,(Join-Path $garlicGame 'main.pak'),(Join-Path $garlicGame 'SDL3.dll'))){Assert-Garlic (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)}
    $garlicSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($path in @($garlicHostLog,$garlicClientLog,(Join-Path $garlicHost 'appdata'),(Join-Path $garlicClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort}
    foreach($name in $garlicEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_GARLIC='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$garlicHost}else{$garlicClient};$width=if($role -eq 'host'){$garlicHostWidth}else{$garlicClientWidth};$nickname=if($role -eq 'host'){'GarlicHost'}else{'GarlicClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native Garlic two-peer paid input and source reaction; '+$garlicRoot)
    $garlicHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $garlicGame -ManifestPath (Join-Path $garlicHost 'smoke.ini') -Role host -RunId $garlicRun -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $garlicRoot 'pids.json') -Executable $GameExe -HostProcess $garlicHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $garlicHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $garlicHostProcess -Role host -TimeoutSeconds 120
    $garlicClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $garlicGame -ManifestPath (Join-Path $garlicClient 'smoke.ini') -Role client -RunId $garlicRun -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $garlicRoot 'pids.json') -Executable $GameExe -HostProcess $garlicHostProcess -ClientProcess $garlicClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $garlicHost 'result.json') -ClientResultPath (Join-Path $garlicClient 'result.json') -HostProcess $garlicHostProcess -ClientProcess $garlicClientProcess -HostLogDirectory $garlicHostLog -ClientLogDirectory $garlicClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Garlic ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Paired native results missing.'
    foreach($r in $results){$samples=if($r.role -eq 'host'){[int]$r.snapshotsSent}else{[int]$r.snapshotsApplied}
        Assert-Garlic ($r.status -eq 'pass' -and [int]$r.protocol -eq 48 -and $r.mode -eq 'fusion_validation' -and $r.scenario -eq 'fusion_validation' -and [int]$r.level -eq 1 -and [uint32]$r.seed -eq $Seed -and -not [bool]$r.skipIntro -and $samples -ge 8) 'Strict protocol48/full intro fixture failed.'
    }
    $hostText=Get-CombinedLogText $garlicHostLog;$clientText=Get-CombinedLogText $garlicClientLog
    Assert-Garlic ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol48 Fusion handshake missing.'
    $hp=@(Get-GarlicPurchases $hostText host);$cp=@(Get-GarlicPurchases $clientText client)
    for($i=0;$i -lt 5;++$i){foreach($j in 1..11){Assert-Garlic ($hp[$i].Groups[$j].Value -eq $cp[$i].Groups[$j].Value) 'Actual cross-peer paid metadata differs.'}}
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){$role=[string]$pair[0];$text=[string]$pair[1]
        Assert-Garlic ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|encode rejected') ('Native Garlic failure: '+$role)
        $acks=@([regex]::Matches($text,('SMOKE_GARLIC225_CHOICE_ACK role='+$role+' own=(\d+) kind=(\d+) extra=([01]) dense=(\d+) scroll=([-+\deE.]+)')))
        $sent=@([regex]::Matches($text,('SMOKE_GARLIC225_CHOICE_SENT role='+$role+' own=(\d+) dense=(\d+) scroll=([-+\deE.]+) centerY=([-+\deE.]+) x=([-+\deE.]+) y=([-+\deE.]+) nativeSDL=1')))
        Assert-Garlic ($acks.Count -eq 5 -and $sent.Count -eq 5) 'Five actual visible-card clicks and observed chooser ACKs required per peer.'
        $wanted=if($role -eq 'host'){@(29,10,0,1,2)}else{@(27,29,0,1,2)};$scrolled=$false
        for($i=0;$i -lt 5;++$i){$ack=$acks[$i];$click=$sent[$i];$extra=if($role -eq 'client' -and $i -gt 0){1}else{0}
            Assert-Garlic ([int]$ack.Groups[1].Value -eq $i+1 -and [int]$ack.Groups[2].Value -eq $wanted[$i] -and [int]$ack.Groups[3].Value -eq $extra -and [int]$click.Groups[1].Value -eq $i -and $ack.Groups[4].Value -eq $click.Groups[2].Value) 'Actual chosen source card differs from requested identity.'
            $cy=[double]::Parse($click.Groups[4].Value,[Globalization.CultureInfo]::InvariantCulture);$scroll=[double]::Parse($click.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture)
            Assert-Garlic ($cy -ge 158 -and $cy -le 493) 'Actual chooser click was outside the settled full-card viewport.';if($scroll -gt 0){$scrolled=$true}
        }
        Assert-Garlic $scrolled 'Garlic chooser must actually scroll to its lower-row source card.'
        foreach($i in @(0,1)){
            Assert-Garlic ($text -match ('SMOKE_GARLIC225_WAIT role='+$role+' index='+$i+' cycle=1 remaining=\d+ certain='+$i+' face=0')) 'Natural first15/10 reaction state missing.'
            Assert-Garlic ($text -match ('SMOKE_GARLIC225_MOVED role='+$role+' index='+$i+' cycle=1 elapsed=-?\d+ row=\d+ Xwalk=[01]')) 'Actual first row movement missing.'
        }
        Assert-Garlic ($text -match ('SMOKE_GARLIC225_WAIT role='+$role+' index=1 cycle=2 remaining=\d+ certain=0 face=[1-9]\d*')) 'Natural repeat while old face schedule active missing.'
        Assert-Garlic ($text -match ('SMOKE_GARLIC225_STALL role='+$role+' index=1 elapsed=\d+ frozen=[1-9]\d* face=\d+')) 'Actual freeze row-stall missing.'
        Assert-Garlic ($text -match ('SMOKE_GARLIC225_FACE role='+$role+' index=1 elapsed=\d+ frozen=[1-9]\d* face=0 imageRestored=1 independent=1')) 'Independent face recovery/native head restoration missing.'
        Assert-Garlic ($text -match ('SMOKE_GARLIC225_PAUSE role='+$role+' freshSnapshots=4 unchanged=1 nativeSDL=1')) 'Paused exact state/world proof missing.'
        Assert-Garlic ($text -match ('SMOKE_GARLIC225_ICE role='+$role+' id=\d+ cost=75 sun=150 sourceFuse=151 nativeSDL=1 postSetupTimerWrites=0')) 'Real paid natural Ice controller proof missing.'
        Assert-Garlic ($text -match ('SMOKE_GARLIC225_GLOVE_ADOPT role='+$role+' id='+$hp[1].Groups[3].Value+' token='+$hp[1].Groups[4].Value+' column=8 row=4 HP=285 replacementHP=300')) 'Canonical original Garlic move was not adopted.'
        Assert-Garlic ($text -match ('SMOKE_GARLIC225_COMPLETE role='+$role+' paidPlants=5 realRecipe=27,29,1130 firstDamage=15,10 wait=100 move=100 face=150 repeatWhileFace=1 freezeStall=1 independentFace=1 coldDamage20=1 pause=1 sun=150 money=3000 postSetupActorWrites=0 nativeBoard=1 lateObserver=NotExecuted malformedRuntime=NotExecuted')) 'Scoped native Garlic completion missing.'
    }
    foreach($i in @(0,1)){
        Assert-Garlic ($hostText -match ('SMOKE_GARLIC225_MOVE role=host index='+$i+' cycle=1 from=\d+ target=\d+ logical=\d+ sort=\d+ elapsed=0 face=150 waitElapsed=100')) 'Host exact100 wait boundary missing.'
        Assert-Garlic ($hostText -match ('SMOKE_GARLIC225_MOVED role=host index='+$i+' cycle=1 elapsed=100 row=\d+ Xwalk=1')) 'Host exact100 move and actual X walking missing.'
    }
    Assert-Garlic ($hostText -match ('SMOKE_GARLIC225_RETARGET actualOrdinarySunflower='+$hp[4].Groups[3].Value+' HP=300 waitRemaining=[1-9]\d* globalEatBlocked=1')) 'Real replacement-victim Waiting regression missing.'
    Assert-Garlic ($hostText -match ('SMOKE_GARLIC225_GLOVE id='+$hp[1].Groups[3].Value+' token='+$hp[1].Groups[4].Value+' from=2,1 to=8,4 sameGeneration=1 HP=285 sourceCellFreed=1 nativeSDL=1')) 'Actual SDL glove move missing.'
    foreach($binding in @(@(29,15,0),@(1130,10,1))){Assert-Garlic ($hostText -match ('Fusion native Garlic begin zombie=\d+ plant=\d+ kind='+$binding[0]+' damage='+$binding[1]+' certain='+$binding[2]+' wait=100 tick=\d+')) 'Actual native source bite callback missing.'}
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$garlicCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_GARLIC225_WAIT role=host');$ci=$clientText.IndexOf('SMOKE_GARLIC225_WAIT role=client');Assert-Garlic ($hi -ge 0 -and $ci -ge 0) 'Reaction snapshot boundary absent.'
    $ht=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$ct=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED);$garlicPost=@($ct|Where-Object {$ht -contains $_}|Select-Object -Unique)
    Assert-Garlic ($garlicCommon.Count -ge 8 -and $garlicPost.Count -ge 2) 'Insufficient actual common/post-reaction snapshots.'
    $garlicFrames=@(Get-GarlicFrames $garlicHost host $garlicHostWidth; Get-GarlicFrames $garlicClient client $garlicClientWidth)
    Assert-Garlic ($garlicFrames.Count -eq 16) 'Sixteen actual paired native frames required.'
    Assert-Garlic ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $garlicSha) 'Owned executable changed during run.';$garlicPassed=$true
}catch{$garlicError=$_.Exception.Message;if($_.ScriptStackTrace){$garlicError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Garlic: '+$garlicError)}
finally{
    foreach($name in $garlicEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$garlicEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $garlicClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $garlicHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$garlicPassed=$false;$garlicError+=' Owned cleanup failed.'};Ensure-Directory $garlicRoot
    $summary=[ordered]@{status=$(if($garlicPassed){'pass'}else{'fail'});error=$garlicError;fixture='FusionNativeGarlic';run_id=$garlicRun;started_utc=$garlicStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
        executable=$GameExe;executable_sha256=$garlicSha;protocol=48;native_schema=13;common_snapshot_hashes=$garlicCommon;post_reaction_common_hashes=$garlicPost;frames=$garlicFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$garlicRoot;
        limits='Two native NORMAL controls with explicitly initial HP/position only. Five paid plants, actual paid sourceIce and SDL glove move. Exact host clocks; client authoritative quarter-pixel targetY is distinguished from interpolation. SourceNPC AI, mixed terrain, late observer and runtime malformed/CRC mutation are not exercised. Pure codec tests are separate evidence.';
        visual_review='Root must view both peers actual Garlic/face/movement/freeze frames; nonblank BMP is not visual acceptance.'}
    Write-Utf8NoBom -Path (Join-Path $garlicRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if($garlicPassed){exit 0}else{exit 1}
