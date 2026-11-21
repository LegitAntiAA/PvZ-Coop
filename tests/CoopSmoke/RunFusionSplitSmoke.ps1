#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=360,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$splitHostWidth=$HostWindowWidth; $splitClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $splitHostWidth -ClientWindowWidth $splitClientWidth
if ($HostWindowWidth -ne $splitHostWidth -or $ClientWindowWidth -ne $splitClientWidth) { throw 'Shared helper changed requested widths.' }
$splitRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $splitRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $splitGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $splitRepo 'work\fusion-split' }
$splitRunId=[Guid]::NewGuid().ToString('N'); $splitRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$splitRunId)
$splitHost=Join-Path $splitRoot 'host'; $splitClient=Join-Path $splitRoot 'client'
$splitHostLog=Join-Path $splitHost 'logs'; $splitClientLog=Join-Path $splitClient 'logs'
$splitHostProcess=$null; $splitClientProcess=$null; $splitPassed=$false; $splitError=''
$splitFrames=@(); $splitCommon=@(); $splitPost=@(); $splitExeSha=''; $splitStarted=[DateTime]::UtcNow; $splitEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_PRODUCER','PVZ_COOP_SMOKE_FUSION_SPLIT','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $splitEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Split([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-SplitFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    foreach ($name in @('chooser','picked','intro','playing','held1090','rear1090','returned1090','combat1090','held1108','rear1108','returned1108','combat1108','held1153','rear1153','returned1153','combat1153','puffnut_crafted','puffnut_moved','puffnut_bitten','complete')) {
        $path=Join-Path $Directory ('fusion_split_'+$name+'.bmp')
        Assert-Split (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Split ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Split ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $splitGame 'main.pak'),(Join-Path $splitGame 'SDL3.dll'))) {
        Assert-Split (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $splitExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($splitHostLog,$splitClientLog,(Join-Path $splitHost 'appdata'),(Join-Path $splitClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $splitEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_SPLIT='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$splitHost} else {$splitClient}
        $width=if ($role -eq 'host') {$splitHostWidth} else {$splitClientWidth}
        $nickname=if ($role -eq 'host') {'SplitHost'} else {'SplitClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native split and passive batch: two hidden owned windows; '+$splitRoot)
    $splitHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $splitGame -ManifestPath (Join-Path $splitHost 'smoke.ini') -Role host -RunId $splitRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $splitRoot 'pids.json') -Executable $GameExe -HostProcess $splitHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $splitHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $splitHostProcess -Role host -TimeoutSeconds 120
    $splitClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $splitGame -ManifestPath (Join-Path $splitClient 'smoke.ini') -Role client -RunId $splitRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $splitRoot 'pids.json') -Executable $GameExe -HostProcess $splitHostProcess -ClientProcess $splitClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $splitHost 'result.json') -ClientResultPath (Join-Path $splitClient 'result.json') `
        -HostProcess $splitHostProcess -ClientProcess $splitClientProcess -HostLogDirectory $splitHostLog -ClientLogDirectory $splitClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Split ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 44 -and $results[1].protocol -eq 44) 'Peer identity/protocol44 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Split ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $splitHostLog;$clientText=Get-CombinedLogText $splitClientLog
    Assert-Split ($hostText -match 'HELLO slot=1 protocol=44 expected=44' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol44/Fusion handshake missing.'
    $allPurchases=@();$allCrafts=@();$allPassiveMoves=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Split ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') ('Fixture/runtime failure: '+$role)
        Assert-Split ($text -match ('SPLIT_READY role='+$role+' nativeSDL=1 hoverObserved=1') -and $text -match ('SPLIT_BUDGET role='+$role+' campaignInitialSun=500 fixtureInitialSun=1000 sunWritesAfterSetup=0 purchaseTotal=900 finalExpected=100')) 'Native Ready/full-intro/explicit one-time setup budget proof missing.'
        $pool=[regex]::Match($text,('SPLIT_POOL role='+$role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) ownFirst=1 unfiltered=1 nativeChooser=1'))
        Assert-Split ($pool.Success -and [int]$pool.Groups[1].Value -ge 16 -and [int]$pool.Groups[2].Value -eq [Math]::Min(10,[int]$pool.Groups[1].Value) -and [int]$pool.Groups[3].Value+[int]$pool.Groups[4].Value -eq [int]$pool.Groups[2].Value) 'Actual canonical chooser pool/quota/owner layout mismatch.'
        $purchases=@();$seen=New-Object 'System.Collections.Generic.HashSet[string]'
        foreach ($m in [regex]::Matches($text,('SPLIT_PURCHASE role='+$role+' kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) extra=(\d+) parents=(\d+),(\d+) sun=(\d+) nativeSDL=1'))) {if ($seen.Add($m.Value)) {$purchases+=$m}}
        Assert-Split ($purchases.Count -eq 10) 'Ten real purchases/unchanged card identities were not observed by both peers.'
        $expected=@(@(0,0,0,65535,65535,900),@(1030,1,1,0,0,700),@(1090,0,0,1030,0,600),@(1108,1,0,1090,2,450),@(6,0,0,65535,65535,450),@(1018,0,0,6,0,350),@(1019,0,0,1018,0,250),@(1153,0,0,1019,0,150),@(6,0,0,65535,65535,150),@(3,1,0,65535,65535,100))
        for ($i=0;$i -lt 10;++$i) {
            $m=$purchases[$i];$e=$expected[$i]
            Assert-Split ([int]$m.Groups[1].Value -eq $e[0] -and [uint32]$m.Groups[2].Value -gt 0 -and [uint32]$m.Groups[3].Value -gt 0 -and [int]$m.Groups[5].Value -eq $e[1] -and [int]$m.Groups[6].Value -eq $e[2] -and
                [int]$m.Groups[7].Value -eq $e[3] -and [int]$m.Groups[8].Value -eq $e[4] -and [int]$m.Groups[9].Value -eq $e[5]) 'Directed recipe/owner/card price/one-time1000 budget sequence differs.'
        }
        $allPurchases+=,@($purchases)
        foreach ($kind in @(1090,1108,1153)) {
            $index=if ($kind -eq 1090) {2} elseif ($kind -eq 1108) {3} else {7};$from=if ($kind -eq 1090) {2} elseif ($kind -eq 1108) {1} else {3};$to=if ($kind -eq 1090) {1} elseif ($kind -eq 1108) {0} else {4};$cd=if ($kind -eq 1153) {334} else {1000}
            $move=[regex]::Match($text,('SPLIT_MOVE role='+$role+' kind='+$kind+' id=(\d+) token=(\d+) fromRow='+$from+' toRow='+$to+' HP=300 sameInstance=1 countersPreserved=1 provenancePreserved=1 expectedCD='+$cd+' observedCD=(\d+)'))
            Assert-Split ($move.Success -and $move.Groups[1].Value -eq $purchases[$index].Groups[2].Value -and $move.Groups[2].Value -eq $purchases[$index].Groups[3].Value -and [int]$move.Groups[3].Value -gt 0 -and [int]$move.Groups[3].Value -le $cd) 'Preheld actual UI row move/identity/counter/cooldown proof missing.'
            $damage=if ($kind -eq 1108) {60} else {20};$a=if ($kind -eq 1153) {'5000,0,980'} else {([string](5000-$damage)+',0,1000')};$b=if ($kind -eq 1153) {'5000,1000,980'} else {('5000,'+(1000-$damage)+',1000')};$c='5000,1000,'+(1000-4*$damage)
            Assert-Split ($text -match ('SPLIT_COMBAT role='+$role+' kind='+$kind+' hits=6 A='+$a+' B='+$b+' C='+$c+' MC=5000 adjacent=5000 noSun=1 noAoE=1 noPierce=1')) 'Actual outbound/returned shield/helmet/body outcome or isolation differs.'
        }
        $craft=[regex]::Match($text,('SPLIT_PASSIVE_CRAFT role='+$role+' id=(\d+) token=(\d+) HP=2000 maxHP=2000 owner=1 parents=6,3 packet=(\d+) incomingKind=3 expectedCD=1000 observedCD=(\d+) sun=100'))
        Assert-Split ($craft.Success -and [uint32]$craft.Groups[1].Value -gt 0 -and [uint32]$craft.Groups[2].Value -gt 0 -and $craft.Groups[1].Value -ne $purchases[8].Groups[2].Value -and $craft.Groups[1].Value -ne $purchases[9].Groups[2].Value -and
            $craft.Groups[3].Value -eq $purchases[9].Groups[4].Value -and [int]$craft.Groups[4].Value -gt 334 -and [int]$craft.Groups[4].Value -le 1000) 'Real directed glove3-into6/PuffNut2000/incoming provenance/1000CD missing.'
        $allCrafts+=$craft
        $move=[regex]::Match($text,('SPLIT_PASSIVE_MOVE role='+$role+' id=(\d+) token=(\d+) col=3 row=2 HP=2000 sameInstance=1 countersPreserved=1 provenancePreserved=1 expectedCD=334 observedCD=(\d+)'))
        Assert-Split ($move.Success -and $move.Groups[1].Value -eq $craft.Groups[1].Value -and $move.Groups[2].Value -eq $craft.Groups[2].Value -and [int]$move.Groups[3].Value -gt 0 -and [int]$move.Groups[3].Value -le 334) 'Final-kind PuffNut ordinary move334 proof missing.';$allPassiveMoves+=$move
        $bite=[regex]::Match($text,('SPLIT_PASSIVE_BITE role='+$role+' id='+$craft.Groups[1].Value+' token='+$craft.Groups[2].Value+' HP=(\d+) maxHP=2000 noRootCrackStates=1 noAttack=1 noSun=1 healthWrites=0'))
        Assert-Split ($bite.Success -and [int]$bite.Groups[1].Value -gt 0 -and [int]$bite.Groups[1].Value -le 1900 -and $text -match ('SPLIT_COMPLETE role='+$role+' sun=100 normal0Uses=5 extra0Uses=1 cherryUses=1 wallnutUses=1 normal6Uses=2 splitKinds=3 passive1021=1 passiveShoveled=1 noSunReward=1 sunWritesAfterSetup=0 plantHPWrites=0 plantTimerWrites=0 nativeBoard=1 secondWorld=0')) 'PuffNut natural bite/native shovel/no hidden attacks-rewards proof missing.'
        foreach ($m in [regex]::Matches($text,'SPLIT_RENDER role=\w+ frame=\w+ freshDraws=(\d+)')) {Assert-Split ([int]$m.Groups[1].Value -ge 2) 'Capture did not await two actual rendered frames.'}
    }
    for ($i=0;$i -lt 10;++$i) {for ($g=1;$g -le 9;++$g) {Assert-Split ($allPurchases[0][$i].Groups[$g].Value -eq $allPurchases[1][$i].Groups[$g].Value) 'Canonical purchase/packet/provenance differs between peers.'}}
    for ($g=1;$g -le 3;++$g) {Assert-Split ($allCrafts[0].Groups[$g].Value -eq $allCrafts[1].Groups[$g].Value) 'Canonical PuffNut mixed identity/provenance differs.'}
    foreach ($kind in @(1090,1108,1153)) {
        $payload=if ($kind -eq 1090) {0} elseif ($kind -eq 1108) {1} else {10};$damage=if ($kind -eq 1108) {60} else {20};$flags=if ($kind -eq 1153) {0} else {1}
        Assert-Split ([regex]::Matches($hostText,('SPLIT_TARGET kind='+$kind+' index=\d+ id=\d+ row=\d+ body=5000 helm=\d+ shield=\d+ MC=[01] initialHPOnly=1 boundedNativeNormal=1 frozenGround=1')).Count -eq 5) 'Five bounded source-eligible native control actors missing.'
        $triggers=@([regex]::Matches($hostText,('SPLIT_TRIGGER kind='+$kind+' volley=(\d+) tick=(\d+) initial=(\d+) first=(\d+) second=(\d+) speed=([0-9.eE+\-]+) groups=2 physicalPerVolley=3 naturalClock=1')))
        $shots=@([regex]::Matches($hostText,('SPLIT_SHOT kind='+$kind+' id=(\d+) volley=(\d+) event=(\d+) spec=(\d+) born=(\d+) trigger=(\d+) delay=(\d+) payload='+$payload+' damage='+$damage+' phase=(\d+) motion=(\d+) nativeActor=1')))
        Assert-Split ($triggers.Count -eq 2 -and $shots.Count -eq 6 -and @($shots | ForEach-Object {$_.Groups[1].Value} | Select-Object -Unique).Count -eq 6) 'Expected exactly two natural three-physical-bullet volleys.'
        for ($v=1;$v -le 2;++$v) {
            $trigger=@($triggers | Where-Object {[int]$_.Groups[1].Value -eq $v});Assert-Split ($trigger.Count -eq 1) 'Duplicate/missing split trigger.';$trigger=$trigger[0]
            $speed=[single]::Parse($trigger.Groups[6].Value,[Globalization.CultureInfo]::InvariantCulture);Assert-Split (-not [single]::IsNaN($speed) -and -not [single]::IsInfinity($speed) -and $speed -ge [single]0.9 -and $speed -le [single]1.1) 'Invalid source active-Speed parameter.'
            $events=@(if ($kind -eq 1153) {[single]0.38333333;[single]0.6333333} else {[single]0.6666667;[single]1});$stateSpeed=if ($kind -eq 1153) {1.0} else {2.0};$deadlines=@($events | ForEach-Object {[int][Math]::Ceiling([double]$_/($stateSpeed*[double]$speed)*100)})
            Assert-Split ([int]$trigger.Groups[3].Value -eq $deadlines[1]+1 -and [int]$trigger.Groups[4].Value -eq $deadlines[0] -and [int]$trigger.Groups[5].Value -eq $deadlines[1]) 'Counter deadlines differ from independent source float32 event groups.'
            $rearId=0;$forwardId=0
            for ($spec=0;$spec -lt 3;++$spec) {
                $shot=@($shots | Where-Object {[int]$_.Groups[2].Value -eq $v -and [int]$_.Groups[4].Value -eq $spec});$event=if ($spec -eq 2) {2} else {1};$phase=if ($spec -eq 1) {0} else {1};$motion=if ($phase -eq 1) {6} else {0}
                Assert-Split ($shot.Count -eq 1 -and [int]$shot[0].Groups[3].Value -eq $event -and [int]$shot[0].Groups[6].Value -eq [int]$trigger.Groups[2].Value -and [int]$shot[0].Groups[7].Value -eq $deadlines[$event-1] -and
                    [int]$shot[0].Groups[5].Value -eq [int]$trigger.Groups[2].Value+$deadlines[$event-1] -and [int]$shot[0].Groups[8].Value -eq $phase -and [int]$shot[0].Groups[9].Value -eq $motion) 'Named-event physical emission is duplicated/early/late/wrong direction.'
                $id=$shot[0].Groups[1].Value;if ($spec -eq 0) {$rearId=[int]$id} elseif ($spec -eq 1) {$forwardId=[int]$id}
                Assert-Split ($clientText -match ('SPLIT_PHASE role=client kind='+$kind+' id='+$id+' phase='+$phase+' motion='+$motion+'[^\r\n]*hostAgeVelocityNotSerialized=1')) ('Client missed canonical initial phase/motion/ID projection: kind='+$kind+' id='+$id+' phase='+$phase+' motion='+$motion)
                $policy=if ($payload -eq 0) {'nativePeaAdaptation'} else {'sourceBox'}
                Assert-Split ($clientText -match ('SPLIT_RECT role=client kind='+$kind+' id='+$id+' phase='+$phase+' motion='+$motion+'[^\r\n]*policy='+$policy)) 'Client front/rear source rectangle/declared native-pea adaptation missing.'
            }
            Assert-Split ($rearId -lt $forwardId) 'Source first event rear-before-forward creation order differs.'
        }
        $turns=@([regex]::Matches($hostText,('SPLIT_TURN kind='+$kind+' id=(\d+) oldX=([0-9.eE+\-]+) newX=([0-9.eE+\-]+) bound=([0-9.eE+\-]+) ageBefore=(\d+) ageAfter=(\d+) vx=([0-9.eE+\-]+) phase=2 motion=0 oldNegativeStep=1 noClamp=1 ageNotReset=1')))
        Assert-Split ($turns.Count -eq 2 -and @($turns | ForEach-Object {$_.Groups[1].Value} | Select-Object -Unique).Count -eq 2) 'Exactly two second-volley rear misses must turn and return.'
        foreach ($turn in $turns) {
            $old=[double]::Parse($turn.Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture);$new=[double]::Parse($turn.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture);$bound=[double]::Parse($turn.Groups[4].Value,[Globalization.CultureInfo]::InvariantCulture);$vx=[double]::Parse($turn.Groups[7].Value,[Globalization.CultureInfo]::InvariantCulture)
            Assert-Split ([Math]::Abs($bound-(76+(-10+4.8)*(72/1.35))) -lt 0.00001 -and $old -lt $bound -and $new -lt $old -and $new -lt $bound -and [Math]::Abs($new-$old+3.2) -lt 0.00005 -and
                [Math]::Abs($vx+3.2) -lt 0.0001 -and [int]$turn.Groups[6].Value -eq [int]$turn.Groups[5].Value+1) 'Host turn used newX, clamped, refreshed velocity early or reset age.'
            $id=$turn.Groups[1].Value;$source=@($shots | Where-Object {$_.Groups[1].Value -eq $id -and [int]$_.Groups[2].Value -eq 2 -and [int]$_.Groups[4].Value -ne 1})
            Assert-Split ($source.Count -eq 1 -and $clientText -match ('SPLIT_PHASE role=client kind='+$kind+' id='+$id+' phase=2 motion=0[^\r\n]*hostAgeVelocityNotSerialized=1') -and
                $clientText -match ('SPLIT_RECT role=client kind='+$kind+' id='+$id+' phase=2 motion=0')) 'Returned canonical ID/phase/motion/rectangle absent on client.'
        }
        Assert-Split ($hostText -match ('SPLIT_OUTBOUND kind='+$kind+' case=shieldBody[^\r\n]*damage='+$damage+' flags='+$flags+' positionOnlySwap=1') -and
            $hostText -match ('SPLIT_OUTBOUND kind='+$kind+' case=helmetShield[^\r\n]*damage='+$damage+' flags='+$flags+' secondVolleyRearMiss=1')) 'Actual outbound shield-vs-helmet/body routing proof missing.'
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$splitCommon=@($ap | Where-Object {$tx -contains $_} | Select-Object -Unique)
    $hostPost=$hostText.IndexOf('SMOKE_FUSION_SPLIT_PASSIVE_BITE role=host');$clientPost=$clientText.IndexOf('SMOKE_FUSION_SPLIT_PASSIVE_BITE role=client');Assert-Split ($hostPost -ge 0 -and $clientPost -ge 0) 'Final passive native-bite barrier missing.'
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hostPost) -Marker SMOKE_SNAPSHOT_TX);$apPost=@(Get-SnapshotHashes -Text $clientText.Substring($clientPost) -Marker SMOKE_SNAPSHOT_APPLIED);$splitPost=@($apPost | Where-Object {$txPost -contains $_} | Select-Object -Unique)
    Assert-Split ($splitCommon.Count -ge 8 -and $splitPost.Count -ge 2) 'Insufficient common/postaction authoritative snapshots.'
    $splitFrames=@((Assert-SplitFrames $splitHost host $splitHostWidth),(Assert-SplitFrames $splitClient client $splitClientWidth));Assert-Split ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $splitExeSha) 'Executable changed during owned run.';$splitPassed=$true
} catch {$splitError=$_.Exception.Message;if ($_.ScriptStackTrace) {$splitError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Split: '+$splitError)}
finally {
    foreach ($name in $splitEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$splitEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $splitClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $splitHostProcess -Role host;if (-not $clientStopped -or -not $hostStopped) {$splitPassed=$false;$splitError+=' Owned cleanup failed.'}
    Ensure-Directory $splitRoot;$summary=[ordered]@{status=$(if ($splitPassed) {'pass'} else {'fail'});error=$splitError;fixture='FusionNativeSplitPassive';source_kinds=@(1090,1108,1153,1021);
        run_id=$splitRunId;started_utc=$splitStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$splitExeSha;
        common_snapshot_hashes=$splitCommon;postaction_common_hashes=$splitPost;frames=$splitFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);
        campaign_initial_sun=500;fixture_initial_sun=1000;purchase_total=900;final_sun=100;sun_writes_after_setup=0;native_board=$true;second_world=$false;artifacts_root=$splitRoot;
        limits='Native normal-zombie controls; full source zombie AI unported. Host proves age/velocity/old-negative turn step. Client native body serializes canonical ID/phase/motion/positions, not exact velocity/age. Pea0 collider/native sound/nominal muzzle are declared adaptations.';
        visual_review='Required by Root; not inferred from nonblank BMP'};Write-Utf8NoBom -Path (Join-Path $splitRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($splitPassed) {exit 0} else {exit 1}
