#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=240,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242,
    [string]$ValidateExisting='',
    [switch]$InitialWalk2
)
$ErrorActionPreference='Stop';$npcInitialWalk2=[bool]$InitialWalk2;$npcLegacyOriginLog=$false;$npcExistingRun=$ValidateExisting;$npcReadOnly=-not [string]::IsNullOrWhiteSpace($npcExistingRun);$npcHostWidth=$HostWindowWidth;$npcClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $npcHostWidth -ClientWindowWidth $npcClientWidth
if($HostWindowWidth -ne $npcHostWidth -or $ClientWindowWidth -ne $npcClientWidth){throw 'Shared helper changed requested widths.'}
$npcRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $npcRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$npcGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $npcRepo 'work\fusion-source-zombie'}
$npcRunId=[Guid]::NewGuid().ToString('N');$npcRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$npcRunId)
if($npcReadOnly){
    $npcRoot=Get-NormalizedPath $npcExistingRun;$npcPriorPath=Join-Path $npcRoot 'summary.json'
    $npcPriorHash=(Get-FileHash -LiteralPath $npcPriorPath -Algorithm SHA256).Hash
    $npcPrior=Get-Content -LiteralPath $npcPriorPath -Raw|ConvertFrom-Json
    $npcRunId=[string]$npcPrior.run_id;$GameExe=Get-NormalizedPath ([string]$npcPrior.executable);$npcGame=Split-Path -Parent $GameExe
    $npcHostWidth=[int]$npcPrior.requested_host_width;$npcClientWidth=[int]$npcPrior.requested_client_width
    $npcOriginProperty=$npcPrior.PSObject.Properties['controlled_initial_walk2'];$npcLegacyOriginLog=$null -eq $npcOriginProperty
    $npcInitialWalk2=if($npcLegacyOriginLog){$false}else{[bool]$npcOriginProperty.Value}
}
$npcHost=Join-Path $npcRoot 'host';$npcClient=Join-Path $npcRoot 'client';$npcHostLog=Join-Path $npcHost 'logs';$npcClientLog=Join-Path $npcClient 'logs'
$npcHostProcess=$null;$npcClientProcess=$null;$npcHostStopped=$false;$npcClientStopped=$false;$npcPassed=$false;$npcFullTail=$false;$npcError='';$npcExeSha='';$npcFrames=@();$npcCommon=@();$npcPost=@();$npcStarted=[DateTime]::UtcNow;$npcEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*'}){$npcEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_SOURCE_ZOMBIE','PVZ_COOP_SMOKE_FUSION_SOURCE_ZOMBIE_WALK2','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $npcEnvironment.ContainsKey($name)){$npcEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}}
function Assert-SourceNpc([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Assert-SourceNpcFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    foreach($name in @('chooser','planted','birth','contact0','contact1','armor','headless0','headless1','dying0','dying1','complete')){
        $path=Join-Path $Directory ('fusion_source_zombie_'+$name+'.bmp');Assert-SourceNpc (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try{Assert-SourceNpc ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport mismatch.';$colors=New-Object 'System.Collections.Generic.HashSet[int]';for($x=16;$x -lt $bitmap.Width;$x+=37){for($y=16;$y -lt $bitmap.Height;$y+=31){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}};Assert-SourceNpc ($colors.Count -ge 8) 'Blank actual renderer frame.'}finally{$bitmap.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try{
    foreach($path in @($GameExe,(Join-Path $npcGame 'main.pak'),(Join-Path $npcGame 'SDL3.dll'))){Assert-SourceNpc (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime missing: '+$path)}
    $npcExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    if($npcReadOnly){
        Assert-SourceNpc ($npcPrior.fixture -eq 'FusionNativeSourceZombie0And2' -and [int]$npcPrior.protocol -eq 49 -and [int]$npcPrior.native_schema -eq 14 -and [bool]$npcPrior.owned_cleanup -and (Split-Path -Leaf $npcRoot) -eq ('run_'+$npcRunId)) 'Saved fixture/protocol/run identity or owned cleanup proof differs.'
        Assert-SourceNpc ($npcExeSha -eq [string]$npcPrior.executable_sha256) 'Saved-run executable SHA256 differs from the actual executable.'
        $npcLedgerPath=Join-Path $npcRoot 'pids.json';$npcLedger=Get-Content -LiteralPath $npcLedgerPath -Raw|ConvertFrom-Json
        Assert-SourceNpc ($npcLedger.Count -eq 2 -and @($npcLedger|Select-Object -ExpandProperty role -Unique).Count -eq 2 -and @($npcLedger|Select-Object -ExpandProperty pid -Unique).Count -eq 2) 'Saved owned PID ledger must identify exactly two peers.'
        foreach($entry in $npcLedger){
            Assert-SourceNpc ($entry.role -in @('host','client') -and [int]$entry.pid -gt 0 -and (Get-NormalizedPath ([string]$entry.executable)) -eq $GameExe) 'Saved process identity/path differs.'
            $start=[DateTime]::Parse($entry.started_utc).ToUniversalTime();$begin=[DateTime]::Parse($npcPrior.started_utc).ToUniversalTime();$finish=[DateTime]::Parse($npcPrior.finished_utc).ToUniversalTime()
            Assert-SourceNpc ($start -ge $begin -and $start -lt $finish) 'Saved process start does not belong to the recorded run.'
            $live=Get-Process -Id ([int]$entry.pid) -ErrorAction SilentlyContinue
            if($null -ne $live){Assert-SourceNpc (-not ($live.StartTime.ToUniversalTime().Ticks -eq $start.Ticks -and (Get-NormalizedPath $live.Path) -eq $GameExe)) 'A saved owned process is still running; read-only validation will not stop it.'}
            $directory=if($entry.role -eq 'host'){$npcHostLog}else{$npcClientLog}
            Assert-SourceNpc (@(Get-ChildItem -LiteralPath $directory -File|Where-Object {$_.Name -match ('_'+[regex]::Escape([string]$entry.role)+'_'+[regex]::Escape([string]$entry.pid)+'\.log$')}).Count -ge 1) 'Actual process log filename does not match the owned PID ledger.'
        }
        $results=@((Get-Content -LiteralPath (Join-Path $npcHost 'result.json') -Raw|ConvertFrom-Json),(Get-Content -LiteralPath (Join-Path $npcClient 'result.json') -Raw|ConvertFrom-Json))
        $npcHostStopped=$true;$npcClientStopped=$true
    }else{
    foreach($path in @($npcHostLog,$npcClientLog,(Join-Path $npcHost 'appdata'),(Join-Path $npcClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort};foreach($name in $npcEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_SOURCE_ZOMBIE='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1';if($npcInitialWalk2){$env:PVZ_COOP_SMOKE_FUSION_SOURCE_ZOMBIE_WALK2='1'}
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$npcHost}else{$npcClient};$width=if($role -eq 'host'){$npcHostWidth}else{$npcClientWidth};$nickname=if($role -eq 'host'){'SourceNpcHost'}else{'SourceNpcClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native source NPC0/2 paid PEAs/Wallnuts; '+$npcRoot)
    $npcHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $npcGame -ManifestPath (Join-Path $npcHost 'smoke.ini') -Role host -RunId $npcRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $npcRoot 'pids.json') -Executable $GameExe -HostProcess $npcHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $npcHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $npcHostProcess -Role host -TimeoutSeconds 120
    $npcClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $npcGame -ManifestPath (Join-Path $npcClient 'smoke.ini') -Role client -RunId $npcRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $npcRoot 'pids.json') -Executable $GameExe -HostProcess $npcHostProcess -ClientProcess $npcClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $npcHost 'result.json') -ClientResultPath (Join-Path $npcClient 'result.json') -HostProcess $npcHostProcess -ClientProcess $npcClientProcess -HostLogDirectory $npcHostLog -ClientLogDirectory $npcClientLog -TimeoutSeconds $TimeoutSeconds)
    # Result files are not full-lifecycle acceptance. Close both owned peers
    # before reading/validating the complete trace, including roster teardown.
    $npcClientStopped=Stop-OwnedProcess -Process $npcClientProcess -Role client;$npcHostStopped=Stop-OwnedProcess -Process $npcHostProcess -Role host
    Assert-SourceNpc ($npcClientStopped -and $npcHostStopped) 'Owned process cleanup failed.'
    }
    Assert-SourceNpc ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Paired native results missing.'
    foreach($result in $results){$samples=if($result.role -eq 'host'){[int]$result.snapshotsSent}else{[int]$result.snapshotsApplied};Assert-SourceNpc ($result.status -eq 'pass' -and [int]$result.protocol -eq 49 -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Strict protocol49/full intro result failed.'}
    $hostText=Get-CombinedLogText $npcHostLog;$clientText=Get-CombinedLogText $npcClientLog
    Assert-SourceNpc ($hostText -match 'HELLO slot=1 protocol=49 expected=49' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Real protocol49/Fusion handshake missing.'
    Assert-SourceNpc ([regex]::Matches($hostText,'SZ260_SETUP initialSUN=600 sourceMoney=3000 oneSUNwrite=1 ambientOnly=1 initialActorHPWrites=0').Count -eq 1) 'Disclosed one-time SUN600 setup missing.'
    Assert-SourceNpc ([regex]::Matches($hostText,'SZ260_POSITION kind=[02] id=\d+ initialX=[0-9.eE+\-]+ nativeRowY=[0-9.eE+\-]+ contactGap=8 onlyInitialXWrite=1').Count -eq 2) 'Two initial-position-only source factory setups missing.'
    $originWrites=@([regex]::Matches($hostText,'SZ260_INITIAL_ORIGIN role=host epoch=(?<epoch>\d+) kind=(?<kind>[02]) id=(?<id>\d+) token=(?<token>\d+) generated=(?<generated>[0-9.eE+\-]+) used=1.5 age=0 initialOnly=1 factoryDrawConsumed=1'))
    $originCount=if($npcInitialWalk2){2}else{0};$originMode=if($npcInitialWalk2){1}else{0};$naturalClaim=if($npcInitialWalk2){0}else{1}
    Assert-SourceNpc ($originWrites.Count -eq $originCount) 'Controlled initial origin writes must be exactly2, or absent in the natural baseline.'
    if(-not $npcLegacyOriginLog){Assert-SourceNpc ([regex]::Matches($hostText,('SZ260_ORIGIN_SETUP role=host epoch=\d+ controlledInitialWalk2='+$originMode+' initialOriginWrites='+$originCount+' naturalRNGClaim='+$naturalClaim)).Count -eq 1) 'Origin setup must distinguish controlled walk2 from natural RNG coverage.'}
    $paidPeers=@();$bornPeers=@();$outcomePeers=@();$poolPeers=@();$float='[0-9.eE+\-]+'
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){
        $role=[string]$pair[0];$text=[string]$pair[1]
        Assert-SourceNpc ($text -notmatch 'Access Violation|\bSMOKE_FAIL\b|\bSMOKE_STOPPED\b|seq-gap|invalid snapshot|host state rejected|encode rejected|preflight (?:failed|rejected)|resource rejected|FUSION_LOCAL_RESOURCE|secondWorld=1|fatal:|protocol violation|queue overflow|assertion failed|unhandled exception') ('Full native lifecycle failure after cleanup: '+$role)
        $pool=[regex]::Match($text,('SZ260_POOL role='+$role+' eligible=(\d+) target=10 quotas=5,5 ownFirst=1'));Assert-SourceNpc ($pool.Success -and [int]$pool.Groups[1].Value -ge 10) 'Actual source pool/own-first chooser missing.';$poolPeers+=,[int]$pool.Groups[1].Value
        $paid=@([regex]::Matches($text,('SZ260_PURCHASE role='+$role+' epoch=(\d+) step=(\d+) kind=(\d+) extra=(\d+) owner=(\d+) id=(\d+) token=(\d+) packet=(\d+) column=(\d+) row=(\d+) cost=(\d+) sun=(\d+) hp=(\d+) cooldown=(\d+) nativeSDL=1')))
        Assert-SourceNpc ($paid.Count -eq 5) 'Five real paid native plants missing.'
        $expected=@(@(0,0,0,2,1,100,500,300,750),@(3,0,0,5,1,50,450,4000,3000),@(0,1,1,2,3,200,250,300,750),@(3,1,1,5,3,100,150,4000,3000),@(0,0,0,1,3,100,50,300,750))
        for($i=0;$i -lt 5;++$i){$buy=$paid[$i];Assert-SourceNpc ([uint32]$buy.Groups[1].Value -gt 0 -and [int]$buy.Groups[2].Value -eq $i -and [uint32]$buy.Groups[6].Value -gt 0 -and [uint32]$buy.Groups[7].Value -gt 0 -and [uint32]$buy.Groups[8].Value -gt 0) 'Actual paid source ID/token/packet missing.';foreach($column in @(3,4,5,9,10,11,12,13,14)){ $index=@(3,4,5,9,10,11,12,13,14).IndexOf($column);Assert-SourceNpc ([int]$buy.Groups[$column].Value -eq $expected[$i][$index]) 'Paid kind/variant/owner/cell/cost/SUN/HP/CD differs.'}}
        Assert-SourceNpc ($paid[0].Groups[8].Value -eq $paid[4].Groups[8].Value -and $paid[0].Groups[6].Value -ne $paid[4].Groups[6].Value -and $paid[0].Groups[7].Value -ne $paid[4].Groups[7].Value) 'Second normal PEA must use the same naturally recharged card and a fresh actor.'
        $epoch=$paid[0].Groups[1].Value
        $births=@([regex]::Matches($text,('SZ260_BIRTH role='+$role+' epoch='+$epoch+' kind=(?<kind>[02]) id=(?<id>\d+) token=(?<token>\d+) localGeneration=(?<local>\d+) speed=(?<speed>'+$float+') body=270 max=270 helm=(?<helm>\d+) helmMax=(?<helmMax>\d+) x=(?<x>'+$float+') row=(?<row>[13]) state=(?<state>walk2?) phase=(?<phase>'+$float+') ghost=(?<ghost>[01])')))
        Assert-SourceNpc ($births.Count -eq 2) 'Two actual typed0/2 births with source270/cone370 missing.'
        $pause=[regex]::Match($text,('SZ260_PAUSE role='+$role+' epoch='+$epoch+' stableMs=(\d+) snapshots=(\d+) health=270,270 helm=0,370 exactFNS=1'))
        Assert-SourceNpc ($pause.Success -and [int]$pause.Groups[1].Value -ge 400 -and [int]$pause.Groups[2].Value -ge 2) 'Actual source pose/HP/armor/position pause or exact FNS14 projection missing.'
        $resumed=$text.IndexOf('SMOKE_SZ260_RESUMED role='+$role+' epoch='+$epoch+' actualPhaseProgress=1');$acks=$text.IndexOf('SMOKE_SZ260_RESUME_ACKS role='+$role+' epoch='+$epoch+' host=1 client=1')
        Assert-SourceNpc ($resumed -gt $text.IndexOf($pause.Value) -and $acks -gt $resumed) 'Both peers must observe actual phase progress and acknowledge resume.'
        $outcomes=@()
        foreach($kind in @(0,2)){
            $born=@($births|Where-Object {[int]$_.Groups['kind'].Value -eq $kind});Assert-SourceNpc ($born.Count -eq 1) 'Duplicate/missing source kind.';$born=$born[0]
            $speed=[single]::Parse($born.Groups['speed'].Value,[Globalization.CultureInfo]::InvariantCulture);$walk=if($speed -gt [single]1.35){'walk2'}else{'walk'};$helm=if($kind -eq 2){370}else{0};$row=if($kind -eq 2){3}else{1}
            Assert-SourceNpc ($speed -ge [single]0.9 -and $speed -le [single]1.8 -and $born.Groups['state'].Value -eq $walk -and [int]$born.Groups['helm'].Value -eq $helm -and [int]$born.Groups['helmMax'].Value -eq $helm -and [int]$born.Groups['row'].Value -eq $row -and [int]$born.Groups['ghost'].Value -eq $(if($role -eq 'host'){0}else{1}) -and [uint32]$born.Groups['local'].Value -gt 0) 'Rolled source speed/walk state/armor/row/authority differs.'
            $id=$born.Groups['id'].Value;$token=$born.Groups['token'].Value;$nut=$paid[$(if($kind -eq 2){3}else{1})]
            if($npcInitialWalk2){Assert-SourceNpc ($speed -eq [single]1.5 -and $walk -eq 'walk2') 'Controlled INITIAL walk2 must reach both peer source models at speed1.5.'}
            $lock=[regex]::Match($text,('SZ260_LOCK role='+$role+' kind='+$kind+' id='+$id+' target='+$nut.Groups[6].Value+' targetToken='+$nut.Groups[7].Value+' state=attack'))
            Assert-SourceNpc ($lock.Success -and $text -match ('SZ260_MOVED role='+$role+' kind='+$kind+' id='+$id+' from='+$float+' to='+$float+' state=\w+ phase='+$float)) 'Actual locomotion/contact/generation lock missing.'
            Assert-SourceNpc ($text -match ('SZ260_BODY role='+$role+' kind='+$kind+' id='+$id+' health='+$float+' body=\d+ helm=\d+')) 'Natural source body damage missing.'
            $bleed=[regex]::Match($text,('SZ260_FLOAT_BLEED role='+$role+' kind='+$kind+' id='+$id+' health=('+$float+') body=(\d+) max=270 head=0'))
            Assert-SourceNpc $bleed.Success 'Natural headless F32 bleed missing.';$hp=[single]::Parse($bleed.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture);$mirror=if($hp -le 1){1}else{[int][Math]::Ceiling([double]$hp)}
            Assert-SourceNpc ([Math]::Abs([double]$hp-[Math]::Round([double]$hp)) -gt .001 -and [int]$bleed.Groups[2].Value -eq $mirror) 'F32 bleed was lost or integer liveness projection differs.'
            $death=[regex]::Match($text,('SZ260_DYING role='+$role+' kind='+$kind+' id='+$id+' token='+$token+' health=('+ $float +') body=\d+ rate=('+ $float +')'))
            Assert-SourceNpc ($death.Success -and [double]::Parse($death.Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture) -gt 0 -and $text -match ('SZ260_RETIRED role='+$role+' epoch='+$epoch+' kind='+$kind+' id='+$id+' token='+$token+' localGeneration='+$born.Groups['local'].Value)) 'Natural positive-rate death tail/full-generation retirement missing.'
            $outcome=[regex]::Match($text,('SZ260_OUTCOME role='+$role+' epoch='+$epoch+' kind='+$kind+' id='+$id+' token='+$token+' bites=(\d+) moved=1 locked=1 bodyHit=1 headless=1 floatBleed=1 dying=1 retired=1 hatGone=([01]) deferredHost=([01])'))
            Assert-SourceNpc ($outcome.Success -and [int]$outcome.Groups[1].Value -ge 1 -and [int]$outcome.Groups[2].Value -eq $(if($kind -eq 2){1}else{0}) -and [int]$outcome.Groups[3].Value -eq $(if($role -eq 'host'){1}else{0})) 'Bounded natural per-kind outcome missing.'
            if($role -eq 'host'){
                $deferred=[regex]::Match($text,('SZ260_DEFERRED role=host kind='+$kind+' id='+$id+' health=('+$float+') body=1 sourcePreupdateDeathNext=1'))
                Assert-SourceNpc ($deferred.Success -and [double]::Parse($deferred.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture) -gt 0 -and [double]::Parse($deferred.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture) -lt 1) 'Source pre-update deferred death boundary missing.'
                $factory=[regex]::Match($text,('Fusion source Zombie spawn id='+$id+' token='+$token+' kind='+$kind+' preset=2 speed=(?<generated>'+$float+') body=270 max=270 helm='+$helm+' walk2=[01] tick=\d+'))
                Assert-SourceNpc $factory.Success 'Actual source factory RNG sample missing.'
                if($npcInitialWalk2){
                    $initial=@($originWrites|Where-Object {$_.Groups['kind'].Value -eq [string]$kind -and $_.Groups['id'].Value -eq $id -and $_.Groups['token'].Value -eq $token -and $_.Groups['epoch'].Value -eq $epoch})
                    Assert-SourceNpc ($initial.Count -eq 1 -and $initial[0].Groups['generated'].Value -eq $factory.Groups['generated'].Value) 'Controlled setup must retain the actual consumed factory draw and canonical identity.'
                }else{Assert-SourceNpc ([single]::Parse($factory.Groups['generated'].Value,[Globalization.CultureInfo]::InvariantCulture) -eq $speed) 'Natural baseline must preserve the original factory origin draw.'}
                $callbacks=@([regex]::Matches($text,('Fusion source Zombie bite id='+$id+' kind='+$kind+' entity=1 target='+$nut.Groups[6].Value+' generation='+$nut.Groups[7].Value+' loop=\d+ seconds='+$float+' tick=\d+')))
                Assert-SourceNpc ($callbacks.Count -eq [int]$outcome.Groups[1].Value) 'Actual source animation callbacks must match the exact observed50-per-bite target loss.'
                Assert-SourceNpc ($text -match ('Fusion source Zombie spawn id='+$id+' token='+$token+' kind='+$kind+' preset=2 speed='+$float+' body=270 max=270 helm='+$helm+' walk2=[01] tick=\d+') -and $text -match ('Fusion source Zombie death id='+$id+' kind='+$kind+' health='+$float+' tick=\d+')) 'Actual explicit source factory and source death admission missing.'
            }
            if($kind -eq 2){Assert-SourceNpc ($text -match ('SZ260_ARMOR role='+$role+' kind=2 id='+$id+' helm=\d+ body=\d+ sourceHP='+$float) -and $text -match ('SZ260_HAT_GONE role='+$role+' kind=2 id='+$id+' token='+$token+' alias=\d+ body=\d+ sourceHP='+$float)) 'Cone armor damage/removal did not preserve sourcekind/token.'}
            $outcomes+=,$outcome
        }
        Assert-SourceNpc ($text -match ('SZ260_COMPLETE role='+$role+' epoch='+$epoch+' sourceKinds=0,2 purchases=5 cost=550 sun=50 money=3000 profileUnchanged=1 sourcePreset=2 pause=1 resumedBoth=1 postSetupActorWrites=0 initialXWrites=2 MC=NotExecuted shrink=NotExecuted secondWorld=0')) 'Strict source0/2 completion missing.'
        $paidPeers+=,@($paid);$bornPeers+=,@($births);$outcomePeers+=,@($outcomes)
    }
    Assert-SourceNpc ($poolPeers[0] -eq $poolPeers[1]) 'Peer eligible pool differs.'
    for($i=0;$i -lt 5;++$i){foreach($group in 1..14){Assert-SourceNpc ($paidPeers[0][$i].Groups[$group].Value -eq $paidPeers[1][$i].Groups[$group].Value) 'Actual paid identity/owner/price/HP differs between peers.'}}
    foreach($kind in @(0,2)){
        $h=@($bornPeers[0]|Where-Object {[int]$_.Groups['kind'].Value -eq $kind})[0];$c=@($bornPeers[1]|Where-Object {[int]$_.Groups['kind'].Value -eq $kind})[0]
        foreach($group in @('id','token','speed','helm','helmMax','row','state')){Assert-SourceNpc ($h.Groups[$group].Value -eq $c.Groups[$group].Value) 'Source identity/profile did not replicate exactly.'}
        # Existing native raw positions use signed truncation to quarter pixels.
        $hx=[single]::Parse($h.Groups['x'].Value,[Globalization.CultureInfo]::InvariantCulture);$cx=[double]::Parse($c.Groups['x'].Value,[Globalization.CultureInfo]::InvariantCulture)
        Assert-SourceNpc ($cx -eq [Math]::Truncate([double]([single]($hx*[single]4)))/4) 'Client authoritative position differs from actual native quarter-pixel codec.'
        $index=if($kind -eq 2){1}else{0};Assert-SourceNpc ($outcomePeers[0][$index].Groups[1].Value -eq $outcomePeers[1][$index].Groups[1].Value) 'Exact target damage/bite totals differ between peers.'
        $row=if($kind -eq 2){3}else{1};$hostShots=@([regex]::Matches($hostText,('SZ260_PEA role=host epoch=\d+ id=(\d+) row='+$row+' type=\d+'))|ForEach-Object {$_.Groups[1].Value});$clientShots=@([regex]::Matches($clientText,('SZ260_PEA role=client epoch=\d+ id=(\d+) row='+$row+' type=\d+'))|ForEach-Object {$_.Groups[1].Value})
        Assert-SourceNpc (@($clientShots|Where-Object {$hostShots -contains $_}|Select-Object -Unique).Count -ge 1) 'Natural native PEA projectiles lack shared IDs on this source NPC row.'
    }
    $sent=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$received=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_RX);$applied=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $npcCommon=@($applied|Where-Object {$sent -contains $_ -and $received -contains $_}|Select-Object -Unique);Assert-SourceNpc ($npcCommon.Count -ge 8) 'Insufficient real common native snapshots.'
    $hostStart=$hostText.IndexOf('SMOKE_SZ260_BIRTH role=host');$clientStart=$clientText.IndexOf('SMOKE_SZ260_BIRTH role=client');Assert-SourceNpc ($hostStart -ge 0 -and $clientStart -ge 0) 'Actual source birth boundary missing.'
    $postTx=@(Get-SnapshotHashes -Text $hostText.Substring($hostStart) -Marker SMOKE_SNAPSHOT_TX);$postAp=@(Get-SnapshotHashes -Text $clientText.Substring($clientStart) -Marker SMOKE_SNAPSHOT_APPLIED)
    $npcPost=@($postAp|Where-Object {$postTx -contains $_}|Select-Object -Unique);Assert-SourceNpc ($npcPost.Count -ge 8) 'Source NPC lifecycle lacks common applied snapshots.'
    $npcFrames=@((Assert-SourceNpcFrames $npcHost host $npcHostWidth),(Assert-SourceNpcFrames $npcClient client $npcClientWidth));Assert-SourceNpc ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $npcExeSha) 'Executable changed during owned run.';$npcFullTail=$true;$npcPassed=$true
}catch{$npcError=$_.Exception.Message;if($_.ScriptStackTrace){$npcError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL SourceZombie: '+$npcError)}
finally{
    if(-not $npcReadOnly){
    foreach($name in $npcEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$npcEnvironment[$name],'Process')}
    if(-not $npcClientStopped){$npcClientStopped=Stop-OwnedProcess -Process $npcClientProcess -Role client};if(-not $npcHostStopped){$npcHostStopped=Stop-OwnedProcess -Process $npcHostProcess -Role host}
    if(-not $npcClientStopped -or -not $npcHostStopped){$npcPassed=$false;$npcError+=' Owned cleanup failed.'};Ensure-Directory $npcRoot
    $summary=[ordered]@{status=$(if($npcPassed){'pass'}else{'fail'});error=$npcError;fixture='FusionNativeSourceZombie0And2';run_id=$npcRunId;started_utc=$npcStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$npcExeSha;protocol=49;native_schema=14;requested_host_width=$npcHostWidth;requested_client_width=$npcClientWidth;common_snapshot_hashes=$npcCommon;postbirth_hashes=$npcPost;frames=$npcFrames;owned_cleanup=[bool]($npcClientStopped -and $npcHostStopped);full_tail_validated_after_cleanup=$npcFullTail;artifacts_root=$npcRoot;paid_transactions=5;initial_sun=600;paid_sun=50;source_money=3000;initial_sun_writes=1;initial_actor_x_writes=2;initial_origin_writes=$(if($npcInitialWalk2){2}else{0});controlled_initial_walk2=$npcInitialWalk2;natural_origin_rng_coverage=(-not $npcInitialWalk2);postsetup_actor_writes=0;
        limits='Source0/2 default preset2 on native Day5 only. Real paid native PEAs/Wallnuts, explicit typed factory, natural root-curve adapter movement, source animation target/bite callbacks, normal PEA20 armor/body damage, source float headless bleed/deferred death, native positive-rate terminal/retirement, exact FNS14 source identity and native quarter-pixel position projection, actual pause/resume ACKs. Root must inspect source body/cone-crack/headless/death frames. ControlledInitialWalk2 explicitly replaces both origins with1.5 before the first Update, preserving/logging the original factory RNG draws; it is not natural origin RNG coverage. MC/shrink/status/difficulty4/5/source loot table/remaining92 source NPCs Not executed; no claim about unresolved Unity builtin root-motion normalization. Full native lifecycle trace is checked only after both owned processes exit.'}
    Write-Utf8NoBom -Path (Join-Path $npcRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 9)+[Environment]::NewLine)
    }else{
        if((Get-FileHash -LiteralPath $npcPriorPath -Algorithm SHA256).Hash -ne $npcPriorHash){$npcPassed=$false;$npcError+=' Original summary changed during read-only validation.'}
    }
}
if($npcReadOnly){
    $proofFiles=@($npcPriorPath,(Join-Path $npcRoot 'pids.json'),(Join-Path $npcHost 'result.json'),(Join-Path $npcClient 'result.json'))
    $proofFiles+=@(Get-ChildItem -LiteralPath $npcHostLog,$npcClientLog -File|ForEach-Object {$_.FullName})
    $proof=@($proofFiles|ForEach-Object {[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash}})
    [ordered]@{status=$(if($npcPassed){'pass'}else{'fail'});error=$npcError;validation='ReadOnlyExistingFullChecker';run_id=$npcRunId;executable=$GameExe;executable_sha256=$npcExeSha;original_summary_sha256=$npcPriorHash;original_summary_preserved=((Get-FileHash -LiteralPath $npcPriorPath -Algorithm SHA256).Hash -eq $npcPriorHash);owned_cleanup_verified=[bool]($npcHostStopped -and $npcClientStopped);full_tail_validated_after_cleanup=$npcFullTail;protocol=49;native_schema=14;controlled_initial_walk2=$npcInitialWalk2;initial_origin_writes=$(if($npcInitialWalk2){2}else{0});natural_origin_rng_coverage=(-not $npcInitialWalk2);common_snapshot_count=$npcCommon.Count;postbirth_snapshot_count=$npcPost.Count;frames=$npcFrames;proof_files=$proof;new_game_processes=0}|ConvertTo-Json -Depth 8|Write-Output
}
if($npcPassed){exit 0}else{exit 1}
