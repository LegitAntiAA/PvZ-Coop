#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=240,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$puffPeaHostWidth=$HostWindowWidth; $puffPeaClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $puffPeaHostWidth -ClientWindowWidth $puffPeaClientWidth
if ($HostWindowWidth -ne $puffPeaHostWidth -or $ClientWindowWidth -ne $puffPeaClientWidth) { throw 'Shared helper changed requested widths.' }
$puffPeaRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $puffPeaRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $puffPeaGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $puffPeaRepo 'work\fusion-puff-pea' }
$puffPeaRunId=[Guid]::NewGuid().ToString('N'); $puffPeaRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$puffPeaRunId)
$puffPeaHost=Join-Path $puffPeaRoot 'host'; $puffPeaClient=Join-Path $puffPeaRoot 'client'
$puffPeaHostLog=Join-Path $puffPeaHost 'logs'; $puffPeaClientLog=Join-Path $puffPeaClient 'logs'
$puffPeaHostProcess=$null; $puffPeaClientProcess=$null; $puffPeaPassed=$false; $puffPeaError=''
$puffPeaFrames=@(); $puffPeaCommon=@(); $puffPeaPost=@(); $puffPeaExeSha=''; $puffPeaStarted=[DateTime]::UtcNow; $puffPeaEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $puffPeaEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-PuffPea([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-PuffPeaFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    foreach ($name in @('chooser','picked','intro','playing','moved1018','moved1019','model1019','held1030','gatling','moved1065','model1065','projectile1018','projectile1019','projectile1065','combat1018','combat1019','combat1065','complete')) {
        $path=Join-Path $Directory ('fusion_puff_pea_'+$name+'.bmp')
        Assert-PuffPea (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-PuffPea ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-PuffPea ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $puffPeaGame 'main.pak'),(Join-Path $puffPeaGame 'SDL3.dll'))) {
        Assert-PuffPea (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $puffPeaExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($puffPeaHostLog,$puffPeaClientLog,(Join-Path $puffPeaHost 'appdata'),(Join-Path $puffPeaClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $puffPeaEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_PUFF_PEA='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$puffPeaHost} else {$puffPeaClient}
        $width=if ($role -eq 'host') {$puffPeaHostWidth} else {$puffPeaClientWidth}
        $nickname=if ($role -eq 'host') {'PuffPeaHost'} else {'PuffPeaClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native PuffPea three-profile batch: two hidden owned windows; '+$puffPeaRoot)
    $puffPeaHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $puffPeaGame -ManifestPath (Join-Path $puffPeaHost 'smoke.ini') -Role host -RunId $puffPeaRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $puffPeaRoot 'pids.json') -Executable $GameExe -HostProcess $puffPeaHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $puffPeaHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $puffPeaHostProcess -Role host -TimeoutSeconds 120
    $puffPeaClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $puffPeaGame -ManifestPath (Join-Path $puffPeaClient 'smoke.ini') -Role client -RunId $puffPeaRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $puffPeaRoot 'pids.json') -Executable $GameExe -HostProcess $puffPeaHostProcess -ClientProcess $puffPeaClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $puffPeaHost 'result.json') -ClientResultPath (Join-Path $puffPeaClient 'result.json') `
        -HostProcess $puffPeaHostProcess -ClientProcess $puffPeaClientProcess -HostLogDirectory $puffPeaHostLog -ClientLogDirectory $puffPeaClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-PuffPea ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 44 -and $results[1].protocol -eq 44) 'Peer identity/protocol44 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-PuffPea ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $puffPeaHostLog;$clientText=Get-CombinedLogText $puffPeaClientLog
    Assert-PuffPea ($hostText -match 'HELLO slot=1 protocol=44 expected=44' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol44/Fusion handshake missing.'
    $allPurchases=@();$allMoves=@();$allCrafts=@();$allComplete=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-PuffPea ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') ('Fixture/runtime failure: '+$role)
        Assert-PuffPea ($text -match ('PUFF_PEA_READY role='+$role+' nativeSDL=1 hoverObserved=1')) 'Actual native Ready interaction missing.'
        $playing=[regex]::Match($text,('PUFF_PEA_PLAYING role='+$role+' sun=500 nativeBoard=1 secondWorld=0 boardTicksSincePlaying=(\d+) sunWrites=0'))
        Assert-PuffPea ($playing.Success -and [int]$playing.Groups[1].Value -ge 2) 'Fresh500/actual two-tick post-intro frame boundary missing.'
        $pool=[regex]::Match($text,('PUFF_PEA_POOL role='+$role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) ownFirst=1 unfiltered=1 nativeChooser=1'))
        Assert-PuffPea ($pool.Success -and [int]$pool.Groups[1].Value -ge 16 -and [int]$pool.Groups[2].Value -eq [Math]::Min(10,[int]$pool.Groups[1].Value) -and
            [int]$pool.Groups[3].Value+[int]$pool.Groups[4].Value -eq [int]$pool.Groups[2].Value) 'Live chooser pool/capacity/quota/own-first mismatch.'
        $purchases=@();$seen=New-Object 'System.Collections.Generic.HashSet[string]'
        foreach ($purchase in [regex]::Matches($text,('PUFF_PEA_PURCHASE role='+$role+' kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) extra=(\d+) parents=(\d+),(\d+) sun=(\d+) sourceCD=750 nativeSDL=1'))) {
            if ($seen.Add($purchase.Value)) {$purchases+=$purchase}
        }
        Assert-PuffPea ($purchases.Count -eq 5) 'Five real purchases were not observed by each peer.'
        $expected=@(@(6,0,0,65535,65535,500),@(1018,1,0,6,0,400),@(1019,1,0,1018,0,300),@(0,1,0,65535,65535,200),@(1030,0,1,0,0,0))
        for ($i=0;$i -lt 5;++$i) {
            $p=$purchases[$i];$e=$expected[$i]
            Assert-PuffPea ([int]$p.Groups[1].Value -eq $e[0] -and [uint32]$p.Groups[2].Value -gt 0 -and [uint32]$p.Groups[3].Value -gt 0 -and
                [int]$p.Groups[5].Value -eq $e[1] -and [int]$p.Groups[6].Value -eq $e[2] -and [int]$p.Groups[7].Value -eq $e[3] -and
                [int]$p.Groups[8].Value -eq $e[4] -and [int]$p.Groups[9].Value -eq $e[5]) 'Real free-Puff/Pea/extra-Pea price, provenance or recipe sequence differs.'
        }
        $allPurchases+=,@($purchases)
        $moves=@()
        foreach ($kind in @(1018,1019,1065)) {
            $move=[regex]::Match($text,('PUFF_PEA_MOVE role='+$role+' kind='+$kind+' id=(\d+) token=(\d+) from=(\d+) to=(\d+) sameInstance=1 HP=300 countersPreserved=1 provenancePreserved=1 expectedCD=334 observedCD=(\d+) deltaTicks=(\d+) nativeClientGlove=1'))
            $from=if ($kind -eq 1019) {1} else {0};$to=1-$from
            Assert-PuffPea ($move.Success -and [int]$move.Groups[3].Value -eq $from -and [int]$move.Groups[4].Value -eq $to -and
                [int]$move.Groups[5].Value -gt 0 -and [int]$move.Groups[5].Value -le 334) 'Same-instance native glove move/334 family cooldown missing.'
            if ($role -eq 'host') {Assert-PuffPea ([int]$move.Groups[5].Value -ge 334-[int]$move.Groups[6].Value) 'Host move used a shorter cooldown than source334.'}
            if ($kind -ne 1065) {$index=if ($kind -eq 1018) {1} else {2};Assert-PuffPea ($move.Groups[1].Value -eq $purchases[$index].Groups[2].Value -and $move.Groups[2].Value -eq $purchases[$index].Groups[3].Value) 'Move changed the purchased source ID/token.'}
            $moves+=$move
        }
        $allMoves+=,@($moves)
        $craft=[regex]::Match($text,('PUFF_PEA_CRAFT role='+$role+' kind=1065 id=(\d+) token=(\d+) owner=0 parents=1019,1030 sun=0 incomingKind=1030 expectedCD=1000 observedCD=(\d+) nativeClientGlove=1'))
        Assert-PuffPea ($craft.Success -and [uint32]$craft.Groups[1].Value -gt 0 -and [uint32]$craft.Groups[2].Value -gt 0 -and
            $craft.Groups[1].Value -ne $purchases[2].Groups[2].Value -and $craft.Groups[1].Value -ne $purchases[4].Groups[2].Value -and
            [int]$craft.Groups[3].Value -gt 334 -and [int]$craft.Groups[3].Value -le 1000 -and
            $craft.Groups[1].Value -eq $moves[2].Groups[1].Value -and $craft.Groups[2].Value -eq $moves[2].Groups[2].Value) 'Glove fusion lost incoming1030 ancestry/owner/cooldown or final actor identity.'
        $allCrafts+=$craft
        Assert-PuffPea ($text -match ('PUFF_PEA_RECHARGED role='+$role+' incoming1030CD=1000 naturalZero=1 noTimerWrites=1')) 'Natural1000 cooldown before final334 move missing.'
        $base=5000
        foreach ($kind in @(1018,1019,1065)) {
            $hits=if ($kind -eq 1018) {2} elseif ($kind -eq 1019) {4} else {8};$base-=20*$hits
            Assert-PuffPea ($text -match ('PUFF_PEA_COMBAT role='+$role+' kind='+$kind+' hits='+$hits+' front='+$base+' rear=5000 MC=5000 adjacent=5000 damage=20 noAoE=1 noSun=1 noPierce=1')) 'Peer natural volley damage/filter/isolation proof missing.'
        }
        $complete=[regex]::Match($text,('PUFF_PEA_COMPLETE role='+$role+' kind=1065 id=(\d+) token=(\d+) sun=0 puffUses=1 normalUses=3 extraUses=1 sourceCD=750 gloveCD=0 threeProfiles=1 sunWrites=0 plantHealthWrites=0 plantTimerWrites=0 nativeBoard=1 secondWorld=0'))
        Assert-PuffPea ($complete.Success -and $complete.Groups[1].Value -eq $craft.Groups[1].Value -and $complete.Groups[2].Value -eq $craft.Groups[2].Value) 'Final actor/currency/card/cooldown conservation proof missing.'
        $allComplete+=$complete
        foreach ($render in [regex]::Matches($text,'PUFF_PEA_RENDER role=\w+ frame=\w+ freshDraws=(\d+)')) {Assert-PuffPea ([int]$render.Groups[1].Value -ge 2) 'Renderer capture did not wait for actual draws.'}
    }
    for ($i=0;$i -lt 5;++$i) {for ($g=1;$g -le 9;++$g) {Assert-PuffPea ($allPurchases[0][$i].Groups[$g].Value -eq $allPurchases[1][$i].Groups[$g].Value) 'Peers disagree on purchased canonical identity/provenance.'}}
    for ($i=0;$i -lt 3;++$i) {for ($g=1;$g -le 4;++$g) {Assert-PuffPea ($allMoves[0][$i].Groups[$g].Value -eq $allMoves[1][$i].Groups[$g].Value) 'Peers disagree on moved canonical identity/cell.'}}
    for ($g=1;$g -le 2;++$g) {Assert-PuffPea ($allCrafts[0].Groups[$g].Value -eq $allCrafts[1].Groups[$g].Value -and $allComplete[0].Groups[$g].Value -eq $allComplete[1].Groups[$g].Value) 'Peers disagree on final glove recipe identity.'}
    Assert-PuffPea ([regex]::Matches($hostText,'PUFF_PEA_TARGET index=\d+ id=\d+ HP=5000 MC=[01] adjacent=[01] boundedNativeTarget=1 initialHPOnly=1').Count -eq 4) 'Native normal/rear/mindcontrol/adjacent controls missing.'
    foreach ($kind in @(1018,1019,1065)) {
        $events=@(if ($kind -eq 1018) {[single]0.38333333} elseif ($kind -eq 1019) {[single]0.38333333;[single]0.6666667} else {[single]0.5;[single]0.7;[single]0.9;[single]1.1})
        $state=if ($kind -eq 1065) {'shoot_g'} else {'shoot'};$stateSpeed=if ($kind -eq 1065) {[double]([single]1.3)} else {1.0}
        $triggers=@([regex]::Matches($hostText,('PUFF_PEA_TRIGGER kind='+$kind+' volley=(\d+) tick=(\d+) initial=(\d+) events=(\d+) delays=([\d,]+) speed=([0-9.eE+\-]+) stateSpeed=([0-9.eE+\-]+) state='+$state+' naturalClock=1')))
        $shots=@([regex]::Matches($hostText,('PUFF_PEA_SHOT kind='+$kind+' id=(\d+) volley=(\d+) event=(\d+) born=(\d+) trigger=(\d+) delay=(\d+) payload=10 damage=20 sun=0 vx=3.2 nativeActor=1')))
        Assert-PuffPea ($triggers.Count -eq 2 -and $shots.Count -eq 2*$events.Count -and @($shots | ForEach-Object {$_.Groups[1].Value} | Select-Object -Unique).Count -eq 2*$events.Count) 'Expected exactly two natural1/2/4-event volleys with unique projectile10 IDs.'
        for ($v=1;$v -le 2;++$v) {
            $trigger=@($triggers | Where-Object {[int]$_.Groups[1].Value -eq $v});Assert-PuffPea ($trigger.Count -eq 1) 'Duplicate/missing natural trigger.';$trigger=$trigger[0]
            $speed=[single]::Parse($trigger.Groups[6].Value,[Globalization.CultureInfo]::InvariantCulture)
            $actualState=[double]::Parse($trigger.Groups[7].Value,[Globalization.CultureInfo]::InvariantCulture)
            Assert-PuffPea (-not [single]::IsNaN($speed) -and -not [single]::IsInfinity($speed) -and $speed -ge [single]0.9 -and $speed -le [single]1.1 -and
                [Math]::Abs($actualState-$stateSpeed) -lt 1e-15 -and [int]$trigger.Groups[4].Value -eq $events.Count) 'Source active-Speed/stateSpeed/event-count differs.'
            $deadlines=@($events | ForEach-Object {[int][Math]::Ceiling([double]$_/($stateSpeed*[double]$speed)*100)})
            Assert-PuffPea ($trigger.Groups[5].Value -eq ($deadlines -join ',') -and [int]$trigger.Groups[3].Value -eq $deadlines[-1]+1) 'Actual countdown differs from independent source float32 event deadlines.'
            for ($e=1;$e -le $events.Count;++$e) {
                $shot=@($shots | Where-Object {[int]$_.Groups[2].Value -eq $v -and [int]$_.Groups[3].Value -eq $e})
                Assert-PuffPea ($shot.Count -eq 1 -and [int]$shot[0].Groups[5].Value -eq [int]$trigger.Groups[2].Value -and [int]$shot[0].Groups[6].Value -eq $deadlines[$e-1] -and
                    [int]$shot[0].Groups[4].Value -eq [int]$trigger.Groups[2].Value+$deadlines[$e-1]) 'Projectile event is early/late/duplicated/canceled.'
            }
        }
        Assert-PuffPea ($hostText -match ('PUFF_PEA_RANGE kind='+$kind+'[^\r\n]*distance=256 beyond240=1 positionOnly=1') -and
            $hostText -match ('PUFF_PEA_DRAIN kind='+$kind+' afterFirstFullVolleyHit=1 beforeSecondFirstShot=1 targetPositionOnly=1 noHPWrites=1 noClockWrites=1')) 'Long-range first volley/explicit position-only drain boundary missing.'
        $long=[regex]::Match($hostText,('PUFF_PEA_LONG_HIT kind='+$kind+' id=(\d+) lastAliveAge=(\d+) survived75and76=1 frontHP=(\d+) oneRetirementOne20Hit=1'))
        Assert-PuffPea ($long.Success -and [int]$long.Groups[2].Value -ge 76 -and $hostText -match ('PUFF_PEA_AGE kind='+$kind+' id='+$long.Groups[1].Value+' age=75 alive=1') -and
            $hostText -match ('PUFF_PEA_AGE kind='+$kind+' id='+$long.Groups[1].Value+' age=76 alive=1') -and @($shots | Where-Object {$_.Groups[1].Value -eq $long.Groups[1].Value -and [int]$_.Groups[2].Value -eq 1}).Count -eq 1) 'Same first-volley projectile did not survive75/76 and then hit once at long range.'
        Assert-PuffPea ($hostText -match ('PUFF_PEA_DAMAGE kind='+$kind+' hits='+(2*$events.Count)+' damage=20[^\r\n]*rear=5000 MC=5000 adjacent=5000 noSun=1 noAoE=1 noPierce=1 healthWrites=0')) 'Authoritative damage/filter/isolation proof missing.'
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $puffPeaCommon=@($ap | Where-Object {$tx -contains $_} | Select-Object -Unique)
    $hostPost=$hostText.IndexOf('SMOKE_FUSION_PUFF_PEA_COMBAT role=host kind=1065');$clientPost=$clientText.IndexOf('SMOKE_FUSION_PUFF_PEA_COMBAT role=client kind=1065')
    Assert-PuffPea ($hostPost -ge 0 -and $clientPost -ge 0) 'Final combat barrier missing.'
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hostPost) -Marker SMOKE_SNAPSHOT_TX);$apPost=@(Get-SnapshotHashes -Text $clientText.Substring($clientPost) -Marker SMOKE_SNAPSHOT_APPLIED)
    $puffPeaPost=@($apPost | Where-Object {$txPost -contains $_} | Select-Object -Unique)
    Assert-PuffPea ($puffPeaCommon.Count -ge 8 -and $puffPeaPost.Count -ge 2) 'Insufficient common/post-combat authoritative snapshot hashes.'
    $puffPeaFrames=@((Assert-PuffPeaFrames $puffPeaHost host $puffPeaHostWidth),(Assert-PuffPeaFrames $puffPeaClient client $puffPeaClientWidth))
    Assert-PuffPea ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $puffPeaExeSha) 'Executable changed during owned run.'
    $puffPeaPassed=$true
} catch {$puffPeaError=$_.Exception.Message;if ($_.ScriptStackTrace) {$puffPeaError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL PuffPea: '+$puffPeaError)}
finally {
    foreach ($name in $puffPeaEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$puffPeaEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $puffPeaClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $puffPeaHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$puffPeaPassed=$false;$puffPeaError+=' Owned cleanup failed.'}
    Ensure-Directory $puffPeaRoot
    $summary=[ordered]@{status=$(if ($puffPeaPassed) {'pass'} else {'fail'});error=$puffPeaError;fixture='FusionNativePuffPea';source_kinds=@(1018,1019,1065);
        run_id=$puffPeaRunId;started_utc=$puffPeaStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$puffPeaExeSha;
        common_snapshot_hashes=$puffPeaCommon;post_combat_common_hashes=$puffPeaPost;frames=$puffPeaFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);
        initial_sun=500;sun_writes=0;native_board=$true;second_world=$false;artifacts_root=$puffPeaRoot;
        limits='Native normal-zombie controls; source zombie AI and full Unity animation parity are not covered. Front actor repositioned only after first full long volley hits.';
        visual_review='Required by Root; not inferred from nonblank BMP'}
    Write-Utf8NoBom -Path (Join-Path $puffPeaRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($puffPeaPassed) {exit 0} else {exit 1}
