#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=300,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$producerHostWidth=$HostWindowWidth; $producerClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $producerHostWidth -ClientWindowWidth $producerClientWidth
if ($HostWindowWidth -ne $producerHostWidth -or $ClientWindowWidth -ne $producerClientWidth) { throw 'Shared helper changed requested widths.' }
$producerRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $producerRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $producerGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $producerRepo 'work\fusion-producer' }
$producerRunId=[Guid]::NewGuid().ToString('N'); $producerRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$producerRunId)
$producerHost=Join-Path $producerRoot 'host'; $producerClient=Join-Path $producerRoot 'client'
$producerHostLog=Join-Path $producerHost 'logs'; $producerClientLog=Join-Path $producerClient 'logs'
$producerHostProcess=$null; $producerClientProcess=$null; $producerPassed=$false; $producerError=''
$producerFrames=@(); $producerCommon=@(); $producerPost=@(); $producerExeSha=''; $producerStarted=[DateTime]::UtcNow; $producerEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_PRODUCER','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $producerEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Producer([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-ProducerFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    foreach ($name in @('chooser','picked','intro','playing','single','three','collected','crack1','crack2','held','moved','death','complete')) {
        $path=Join-Path $Directory ('fusion_producer_'+$name+'.bmp')
        Assert-Producer (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Producer ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Producer ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $producerGame 'main.pak'),(Join-Path $producerGame 'SDL3.dll'))) {
        Assert-Producer (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $producerExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($producerHostLog,$producerClientLog,(Join-Path $producerHost 'appdata'),(Join-Path $producerClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $producerEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_PRODUCER='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$producerHost} else {$producerClient}
        $width=if ($role -eq 'host') {$producerHostWidth} else {$producerClientWidth}
        $nickname=if ($role -eq 'host') {'ProducerHost'} else {'ProducerClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native producer four-kind batch: two hidden owned windows; '+$producerRoot)
    $producerHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $producerGame -ManifestPath (Join-Path $producerHost 'smoke.ini') -Role host -RunId $producerRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $producerRoot 'pids.json') -Executable $GameExe -HostProcess $producerHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $producerHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $producerHostProcess -Role host -TimeoutSeconds 120
    $producerClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $producerGame -ManifestPath (Join-Path $producerClient 'smoke.ini') -Role client -RunId $producerRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $producerRoot 'pids.json') -Executable $GameExe -HostProcess $producerHostProcess -ClientProcess $producerClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $producerHost 'result.json') -ClientResultPath (Join-Path $producerClient 'result.json') `
        -HostProcess $producerHostProcess -ClientProcess $producerClientProcess -HostLogDirectory $producerHostLog -ClientLogDirectory $producerClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Producer ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 44 -and $results[1].protocol -eq 44) 'Peer identity/protocol44 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Producer ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $producerHostLog;$clientText=Get-CombinedLogText $producerClientLog
    Assert-Producer ($hostText -match 'HELLO slot=1 protocol=44 expected=44' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol44/Fusion handshake missing.'
    $allPurchases=@();$allMoves=@();$allComplete=@();$allInitial=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Producer ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1|Producer (?:threshold|death) omitted capacity') ('Fixture/runtime failure: '+$role)
        Assert-Producer ($text -match ('PRODUCER_READY role='+$role+' nativeSDL=1 hoverObserved=1')) 'Actual native Ready interaction missing.'
        $playing=[regex]::Match($text,('PRODUCER_PLAYING role='+$role+' sun=500 boardTicksSincePlaying=(\d+) nativeBoard=1 secondWorld=0 manualSunWrites=0'))
        Assert-Producer ($playing.Success -and [int]$playing.Groups[1].Value -ge 2) 'Fresh500/full-intro/post-playing frame boundary missing.'
        $pool=[regex]::Match($text,('PRODUCER_POOL role='+$role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) ownFirst=1 unfiltered=1 nativeChooser=1'))
        Assert-Producer ($pool.Success -and [int]$pool.Groups[1].Value -ge 16 -and [int]$pool.Groups[2].Value -eq [Math]::Min(10,[int]$pool.Groups[1].Value) -and
            [int]$pool.Groups[3].Value+[int]$pool.Groups[4].Value -eq [int]$pool.Groups[2].Value) 'Canonical eligible pool/capacity/quota/own-first layout differs.'
        $purchases=@();$seen=New-Object 'System.Collections.Generic.HashSet[string]'
        foreach ($p in [regex]::Matches($text,('PRODUCER_PURCHASE role='+$role+' kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) extra=(\d+) parents=(\d+),(\d+) sun=(\d+) nativeSDL=1'))) {if ($seen.Add($p.Value)) {$purchases+=$p}}
        Assert-Producer ($purchases.Count -eq 6) 'Six real source-card purchases were not observed by each peer.'
        $expected=@(@(1,0,0,65535,65535,450),@(1033,1,1,1,1,350),@(3,0,0,65535,65535,300),@(1006,0,0,3,1,250),@(0,0,0,65535,65535,150),@(1000,0,0,0,1,100))
        for ($i=0;$i -lt 6;++$i) {
            $p=$purchases[$i];$e=$expected[$i]
            Assert-Producer ([int]$p.Groups[1].Value -eq $e[0] -and [uint32]$p.Groups[2].Value -gt 0 -and [uint32]$p.Groups[3].Value -gt 0 -and [int]$p.Groups[5].Value -eq $e[1] -and
                [int]$p.Groups[6].Value -eq $e[2] -and [int]$p.Groups[7].Value -eq $e[3] -and [int]$p.Groups[8].Value -eq $e[4] -and [int]$p.Groups[9].Value -eq $e[5]) 'Actual source prices/recipes/ownership/identity/budget differ.'
        }
        $allPurchases+=,@($purchases)
        $initial=@([regex]::Matches($text,('PRODUCER_INITIAL role='+$role+' kind=(\d+) id=(\d+) token=(\d+) observedCounter=(\d+) tick=(\d+) pending=0 flags=0 nativeStore=(\w+)')))
        Assert-Producer ($initial.Count -eq 4) 'Expected four actual initialized producer instances.'
        foreach ($kind in @(1,1033,1006,1000)) {
            $entry=@($initial | Where-Object {[int]$_.Groups[1].Value -eq $kind});Assert-Producer ($entry.Count -eq 1) 'Duplicate/missing source producer instance.';$entry=$entry[0]
            $index=if ($kind -eq 1) {0} elseif ($kind -eq 1033) {1} elseif ($kind -eq 1006) {3} else {5}
            $store=if ($kind -eq 1000) {'metadata'} else {'mLaunchCounter'}
            Assert-Producer ($entry.Groups[2].Value -eq $purchases[$index].Groups[2].Value -and $entry.Groups[3].Value -eq $purchases[$index].Groups[3].Value -and
                [int]$entry.Groups[4].Value -ge 390 -and [int]$entry.Groups[4].Value -le 701 -and $entry.Groups[6].Value -eq $store) 'Actual initial producer store/provenance/deadline bound mismatch.'
        }
        $allInitial+=,@($initial)
        $move=[regex]::Match($text,('PRODUCER_MOVE role='+$role+' kind=1006 id=(\d+) token=(\d+) from=5 to=4 sameInstance=1 heldAlive=1 pendingPreserved=1 countdownPreserved=1 flagsPreserved=1 HPNotReset=1 expectedCD=1000 observedCD=(\d+)'))
        Assert-Producer ($move.Success -and $move.Groups[1].Value -eq $purchases[3].Groups[2].Value -and $move.Groups[2].Value -eq $purchases[3].Groups[3].Value -and
            [int]$move.Groups[3].Value -gt 0 -and [int]$move.Groups[3].Value -le 1000) 'Held producer/pending-preserving native move proof missing.'
        $allMoves+=$move
        Assert-Producer ($text -match ('PRODUCER_COLLECT role='+$role+' id=\d+ before=100 after=125 value=25 nativeClientSDL=1 oneCredit=1 manualSunWrites=0') -and
            $text -match ('PRODUCER_NUT_DONE role='+$role+' id='+$move.Groups[1].Value+' token='+$move.Groups[2].Value+' flags=3 thresholdsOnce=2 deathCoins=2 bitWhileProducing=1 pendingMove=1 naturalHP4000to0=1')) 'Real native collection/SunNut natural damage-and-production completion missing.'
        $complete=[regex]::Match($text,('PRODUCER_COMPLETE role='+$role+' twin=(\d+)/(\d+) pea=(\d+)/(\d+) nut=(\d+)/(\d+) sun=125 source1NormalUses=3 source1ExtraUses=1 nutUses=1 peaUses=1 collected25=1 sourceProducerKinds=4 manualSunWrites=0 plantHealthWrites=0 plantTimerWrites=0 nativeBoard=1 secondWorld=0'))
        Assert-Producer ($complete.Success -and $complete.Groups[1].Value -eq $purchases[1].Groups[2].Value -and $complete.Groups[2].Value -eq $purchases[1].Groups[3].Value -and
            $complete.Groups[3].Value -eq $purchases[5].Groups[2].Value -and $complete.Groups[4].Value -eq $purchases[5].Groups[3].Value -and
            $complete.Groups[5].Value -eq $purchases[3].Groups[2].Value -and $complete.Groups[6].Value -eq $purchases[3].Groups[3].Value) 'Final canonical identities/ledger/packet conservation proof missing.'
        $allComplete+=$complete
        foreach ($render in [regex]::Matches($text,'PRODUCER_RENDER role=\w+ frame=\w+ freshDraws=(\d+)')) {Assert-Producer ([int]$render.Groups[1].Value -ge 2) 'Capture did not wait for actual renderer draws.'}
    }
    for ($i=0;$i -lt 6;++$i) {for ($g=1;$g -le 9;++$g) {Assert-Producer ($allPurchases[0][$i].Groups[$g].Value -eq $allPurchases[1][$i].Groups[$g].Value) 'Peer purchase identity/provenance differs.'}}
    for ($g=1;$g -le 6;++$g) {Assert-Producer ($allComplete[0].Groups[$g].Value -eq $allComplete[1].Groups[$g].Value) 'Peer final actor/token differs.'}
    $nativeInit=@([regex]::Matches($hostText,'Fusion native Producer init id=\d+ token=(\d+) kind=(\d+) hp=(\d+) initialSeconds=([0-9.eE+\-]+) counter=(\d+) interval=([0-9.]+) count=(\d+) tick=(\d+)'))
    Assert-Producer ($nativeInit.Count -eq 4) 'Four raw source-float producer initializations missing.'
    foreach ($kind in @(1,1033,1006,1000)) {
        $raw=@($nativeInit | Where-Object {[int]$_.Groups[2].Value -eq $kind});Assert-Producer ($raw.Count -eq 1) 'Duplicate raw initialization.';$raw=$raw[0]
        $seconds=[single]::Parse($raw.Groups[4].Value,[Globalization.CultureInfo]::InvariantCulture)
        Assert-Producer (-not [single]::IsNaN($seconds) -and -not [single]::IsInfinity($seconds) -and $seconds -ge 4 -and $seconds -le 7 -and
            [int]$raw.Groups[5].Value -eq [int][Math]::Floor([double]$seconds*100)+1) 'Source initial float4..7 strict floor+1 quantization differs.'
        $observed=@($allInitial[0] | Where-Object {[int]$_.Groups[1].Value -eq $kind})[0]
        Assert-Producer ($observed.Groups[3].Value -eq $raw.Groups[1].Value -and [int]$observed.Groups[5].Value -ge [int]$raw.Groups[8].Value -and
            [int]$observed.Groups[4].Value -eq [int]$raw.Groups[5].Value-([int]$observed.Groups[5].Value-[int]$raw.Groups[8].Value)) 'Observed native instance/timer does not match raw initialization token and clock.'
    }
    $triggers=@([regex]::Matches($hostText,'PRODUCER_TRIGGER kind=(\d+) id=(\d+) token=(\d+) tick=(\d+) cycle=(\d+) interval=(\d+) jitter=(-?\d+) pending=50 strictNativeCounter=1'))
    $callbacks=@([regex]::Matches($hostText,'PRODUCER_CALLBACK kind=(\d+) id=(\d+) token=(\d+) trigger=(\d+) tick=(\d+) delay=50 count=(\d+) HP=(\d+) flags=(\d+) whileBitten=(\d+)'))
    $emissions=@([regex]::Matches($hostText,'PRODUCER_EMISSION kind=(\d+) actor=(\d+) token=(\d+) reason=(\w+) tick=(\d+) count=(\d+) coinIds=([\d,]+) value=25 nativeArray=1'))
    foreach ($trigger in $triggers) {
        $kind=[int]$trigger.Groups[1].Value;$interval=if ($kind -eq 1033) {20} elseif ($kind -eq 1006) {15} else {25};$jitter=[int]$trigger.Groups[7].Value
        Assert-Producer ([int]$trigger.Groups[6].Value -eq $interval -and $jitter -ge -2 -and $jitter -le 2 -and [int]$trigger.Groups[5].Value -eq ($interval+$jitter)*100+1) 'Source periodic CSV interval/integer jitter/strict deadline differs.'
    }
    foreach ($callback in $callbacks) {
        $kind=[int]$callback.Groups[1].Value;$count=if ($kind -eq 1033) {2} else {1}
        Assert-Producer ([int]$callback.Groups[5].Value -eq [int]$callback.Groups[4].Value+50 -and [int]$callback.Groups[6].Value -eq $count) 'Source callback delay/yield differs.'
        $trigger=@($triggers | Where-Object {$_.Groups[1].Value -eq $callback.Groups[1].Value -and $_.Groups[2].Value -eq $callback.Groups[2].Value -and $_.Groups[3].Value -eq $callback.Groups[3].Value -and $_.Groups[4].Value -eq $callback.Groups[4].Value})
        $emission=@($emissions | Where-Object {$_.Groups[1].Value -eq $callback.Groups[1].Value -and $_.Groups[2].Value -eq $callback.Groups[2].Value -and $_.Groups[3].Value -eq $callback.Groups[3].Value -and $_.Groups[4].Value -eq 'periodic' -and $_.Groups[5].Value -eq $callback.Groups[5].Value})
        Assert-Producer ($trigger.Count -eq 1 -and $emission.Count -eq 1 -and [int]$emission[0].Groups[6].Value -eq $count) 'Callback is not linked to one real trigger and complete native Coin emission.'
    }
    foreach ($kind in @(1,1033,1006,1000)) {
        $minimum=if ($kind -eq 1) {1} elseif ($kind -eq 1006) {1} else {2}
        Assert-Producer (@($callbacks | Where-Object {[int]$_.Groups[1].Value -eq $kind}).Count -ge $minimum) 'Required natural source producer cycles missing.'
    }
    $allCoinIds=New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($emission in $emissions) {
        $ids=@($emission.Groups[7].Value.Split(','));Assert-Producer ($ids.Count -eq [int]$emission.Groups[6].Value) 'Emission contains incomplete/extra Coin IDs.'
        foreach ($id in $ids) {
            Assert-Producer ($allCoinIds.Add($id) -and [uint32]$id -gt 0 -and $hostText -match ('PRODUCER_COIN role=host id='+$id+' row=[024] value=25 nativeCoin=1') -and
                $clientText -match ('PRODUCER_COIN role=client id='+$id+' row=[024] value=25 nativeCoin=1')) 'Coin ID duplicated or missing actual host/client native Coin.'
        }
    }
    $rewards=@([regex]::Matches($hostText,'PRODUCER_REWARD kind=1006 id=(\d+) token=(\d+) bit=(\d+) HP=(\d+) flags=(\d+) count=1 value=25 tick=(\d+)'))
    Assert-Producer ($rewards.Count -eq 2) 'SunNut must reward exactly once per natural strict health band.'
    foreach ($bit in @(1,2)) {
        $reward=@($rewards | Where-Object {[int]$_.Groups[3].Value -eq $bit});Assert-Producer ($reward.Count -eq 1) 'Duplicate/missing SunNut reward bit.';$reward=$reward[0]
        $hp=[int]$reward.Groups[4].Value;$valid=if ($bit -eq 1) {$hp -gt 1333 -and $hp -lt 2666 -and [int]$reward.Groups[5].Value -eq 1} else {$hp -lt 1333 -and [int]$reward.Groups[5].Value -eq 3}
        Assert-Producer $valid 'SunNut reward equality/band/persistent flags differ.'
        $reason='threshold'+$bit;$emission=@($emissions | Where-Object {$_.Groups[1].Value -eq '1006' -and $_.Groups[4].Value -eq $reason -and $_.Groups[5].Value -eq $reward.Groups[6].Value})
        Assert-Producer ($emission.Count -eq 1 -and [int]$emission[0].Groups[6].Value -eq 1) 'Threshold reward is not one actual25 Coin.'
    }
    $death=@($emissions | Where-Object {$_.Groups[1].Value -eq '1006' -and $_.Groups[4].Value -eq 'death'})
    Assert-Producer ($death.Count -eq 1 -and [int]$death[0].Groups[6].Value -eq 2 -and @($callbacks | Where-Object {$_.Groups[1].Value -eq '1006' -and [int]$_.Groups[9].Value -gt 0}).Count -ge 1 -and
        $hostText -match 'PRODUCER_DEATH kind=1006[^\r\n]*previousHP=[1-4] flags=3 count=2 value=25[^\r\n]*nativeBite=1') 'Natural SunNut lethal bite/death two-Coin/periodic-during-bite proof missing.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $producerCommon=@($ap | Where-Object {$tx -contains $_} | Select-Object -Unique)
    $hostPost=$hostText.IndexOf('SMOKE_FUSION_PRODUCER_NUT_DONE role=host');$clientPost=$clientText.IndexOf('SMOKE_FUSION_PRODUCER_NUT_DONE role=client')
    Assert-Producer ($hostPost -ge 0 -and $clientPost -ge 0) 'Final natural death barrier missing.'
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hostPost) -Marker SMOKE_SNAPSHOT_TX);$apPost=@(Get-SnapshotHashes -Text $clientText.Substring($clientPost) -Marker SMOKE_SNAPSHOT_APPLIED)
    $producerPost=@($apPost | Where-Object {$txPost -contains $_} | Select-Object -Unique)
    Assert-Producer ($producerCommon.Count -ge 8 -and $producerPost.Count -ge 2) 'Insufficient common/post-death authoritative snapshots.'
    $producerFrames=@((Assert-ProducerFrames $producerHost host $producerHostWidth),(Assert-ProducerFrames $producerClient client $producerClientWidth))
    Assert-Producer ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $producerExeSha) 'Executable changed during owned run.'
    $producerPassed=$true
} catch {$producerError=$_.Exception.Message;if ($_.ScriptStackTrace) {$producerError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Producer: '+$producerError)}
finally {
    foreach ($name in $producerEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$producerEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $producerClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $producerHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$producerPassed=$false;$producerError+=' Owned cleanup failed.'}
    Ensure-Directory $producerRoot
    $summary=[ordered]@{status=$(if ($producerPassed) {'pass'} else {'fail'});error=$producerError;fixture='FusionNativeProducer';source_kinds=@(1,1033,1006,1000);
        run_id=$producerRunId;started_utc=$producerStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$producerExeSha;
        common_snapshot_hashes=$producerCommon;post_death_common_hashes=$producerPost;frames=$producerFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);
        initial_sun=500;purchase_total=400;collected_sun=25;final_sun=125;manual_sun_writes=0;native_board=$true;second_world=$false;artifacts_root=$producerRoot;
        limits='Native basic biter only. Host proves exact lethalHP<=4; client proves last sampled low-band HP/flags, canonical retirement and corresponding host two-Coin IDs, without inferring an unsampled final HP. Paired shovel/byMix death, saturation, pause and real latejoin are not covered; source boundary/malformed flags have independent unit checks.';
        visual_review='Required by Root; not inferred from nonblank BMP'}
    Write-Utf8NoBom -Path (Join-Path $producerRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($producerPassed) {exit 0} else {exit 1}
