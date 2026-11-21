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
$chompHostWidth=$HostWindowWidth; $chompClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $chompHostWidth -ClientWindowWidth $chompClientWidth
if ($HostWindowWidth -ne $chompHostWidth -or $ClientWindowWidth -ne $chompClientWidth) { throw 'Shared helper changed requested widths.' }
$chompRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $chompRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $chompGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $chompRepo 'work\fusion-chomper' }
$chompRunId=[Guid]::NewGuid().ToString('N'); $chompRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$chompRunId)
$chompHost=Join-Path $chompRoot 'host'; $chompClient=Join-Path $chompRoot 'client'
$chompHostLog=Join-Path $chompHost 'logs'; $chompClientLog=Join-Path $chompClient 'logs'
$chompHostProcess=$null; $chompClientProcess=$null; $chompPassed=$false; $chompError=''
$chompFrames=@(); $chompCommon=@(); $chompPost=@(); $chompExeSha=''; $chompStarted=[DateTime]::UtcNow; $chompEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_CHOMPER','PVZ_COOP_SMOKE_FUSION_DEATH','PVZ_COOP_SMOKE_FUSION_FIRE','PVZ_COOP_SMOKE_FUSION_COLD','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_PRODUCER','PVZ_COOP_SMOKE_FUSION_SPLIT','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $chompEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Chomp([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-ChompFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $required=@('chooser','playing','chew','reward','recovered','terminal','complete')
    foreach ($name in $required) {
        $path=Join-Path $Directory ('fusion_chomper_'+$name+'.bmp')
        Assert-Chomp (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Chomp ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Chomp ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $chompGame 'main.pak'),(Join-Path $chompGame 'SDL3.dll'))) {
        Assert-Chomp (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $chompExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($chompHostLog,$chompClientLog,(Join-Path $chompHost 'appdata'),(Join-Path $chompClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $chompEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_CHOMPER='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$chompHost} else {$chompClient}
        $width=if ($role -eq 'host') {$chompHostWidth} else {$chompClientWidth}
        $nickname=if ($role -eq 'host') {'ChomperHost'} else {'ChomperClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native shared Chomper cohort: two hidden owned windows; '+$chompRoot)
    $chompHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $chompGame -ManifestPath (Join-Path $chompHost 'smoke.ini') -Role host -RunId $chompRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $chompRoot 'pids.json') -Executable $GameExe -HostProcess $chompHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $chompHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $chompHostProcess -Role host -TimeoutSeconds 120
    $chompClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $chompGame -ManifestPath (Join-Path $chompClient 'smoke.ini') -Role client -RunId $chompRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $chompRoot 'pids.json') -Executable $GameExe -HostProcess $chompHostProcess -ClientProcess $chompClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $chompHost 'result.json') -ClientResultPath (Join-Path $chompClient 'result.json') `
        -HostProcess $chompHostProcess -ClientProcess $chompClientProcess -HostLogDirectory $chompHostLog -ClientLogDirectory $chompClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Chomp ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 44 -and $results[1].protocol -eq 44) 'Peer identity/protocol44 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Chomp ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $chompHostLog;$clientText=Get-CombinedLogText $chompClientLog
    Assert-Chomp ($hostText -match 'HELLO slot=1 protocol=44 expected=44' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol44/Fusion handshake missing.'
    $purchaseSets=@();$moveSets=@();$coinSets=@()
    $expectedKinds=@(5,1012,5,1014,5,1016);$expectedMoney=@(850,800,650,600,450,300)
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Chomp ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') ('Actual Chomper runtime failure: '+$role)
        Assert-Chomp ($text -match ('CHOMPER_READY role='+$role+' nativeSDL=1 hoverObserved=1') -and $text -match ('CHOMPER_POOL role='+$role+' eligible=20 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1')) 'Actual shared chooser/own-first/Ready missing.'
        Assert-Chomp ($text -match ('CHOMPER_BUDGET role='+$role+' campaignInitialSun=500 fixtureInitialSun=1000 totalCost=700 finalExpected=300 sunWritesAfterSetup=0')) 'Explicit one-time wallet/golden missing.'
        $buys=@([regex]::Matches($text,('CHOMPER_PURCHASE role='+$role+' step=(\d+) kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) parents=(\d+),(\d+) HP=(\d+) sun=(\d+) nativeSDL=1')))
        Assert-Chomp ($buys.Count -eq 6) 'Six actual directed purchases missing.'
        for ($i=0;$i -lt 6;++$i) {
            Assert-Chomp ([int]$buys[$i].Groups[1].Value -eq $i -and [int]$buys[$i].Groups[2].Value -eq $expectedKinds[$i] -and [int]$buys[$i].Groups[10].Value -eq $expectedMoney[$i] -and [uint32]$buys[$i].Groups[4].Value -gt 0) 'Actual kind/budget/stable token differs.'
            if ($i%2 -eq 1) {Assert-Chomp ($buys[$i].Groups[6].Value -eq '1' -and $buys[$i].Groups[7].Value -eq '5' -and $buys[$i].Groups[8].Value -eq @('3','1','2')[[int](($i-1)/2)]) 'Actual incoming owner/directional ancestry differs.'}
        };$purchaseSets+=,@($buys)
        foreach ($kind in @(1012,1014,1016)) {foreach ($pose in @('idle','bite','chew','swallow')) {
            Assert-Chomp ($text -match ('CHOMPER_POSE role='+$role+' kind='+$kind+' id=\d+ token=\d+ state='+$pose+' elapsed=[0-9.eE+\-]+ canonicalCache=1 clientNoLocalClock=1')) ('Missing source phase cache projection: '+$role+' kind'+$kind+' '+$pose)
        }}
        $moves=@([regex]::Matches($text,('CHOMPER_MOVE role='+$role+' kind=(\d+) id=(\d+) token=(\d+) col=(\d+) row=(\d+) HP=(\d+) owner=(\d+) packet=(\d+) sameInstance=1 digestNotReset=1 nativeSDL=1')))
        Assert-Chomp ($moves.Count -eq 2 -and $moves[0].Groups[1].Value -eq '1012' -and $moves[0].Groups[4].Value -eq '3' -and $moves[0].Groups[5].Value -eq '0' -and $moves[1].Groups[1].Value -eq '1014' -and $moves[1].Groups[4].Value -eq '7' -and $moves[1].Groups[5].Value -eq '2') 'Two real instance-preserving digest/pending UI moves missing.';$moveSets+=,@($moves)
        $coins=@([regex]::Matches($text,('CHOMPER_COIN role='+$role+' id=(\d+) actor=(\d+) token=(\d+) value=25 col=7 row=2 x=[0-9.eE+\-]+ y=[0-9.eE+\-]+ elapsedFromSwallow=(\d+) tick=\d+')))
        Assert-Chomp ($coins.Count -eq 4 -and @($coins|ForEach-Object {$_.Groups[1].Value}|Select-Object -Unique).Count -eq 4) 'Exactly four different native25 Coins missing.';$coinSets+=,@($coins)
        foreach ($coin in $coins) {Assert-Chomp ($coin.Groups[2].Value -eq $buys[3].Groups[3].Value -and $coin.Groups[3].Value -eq $buys[3].Groups[4].Value) 'Coin reward lost actual SunChomper identity.'}
        $visibleSuns=@([regex]::Matches($text,('CHOMPER_SUN_READY role='+$role+' id=(\d+) observedAge=(\d+) nativeAge=(\d+) scale=([0-9.eE+\-]+) targetScale=([0-9.eE+\-]+) nativeSunReanim=1 attached=1 visibleImageTrack=1')))
        Assert-Chomp ($visibleSuns.Count -eq 4) 'Reward capture did not wait for four live native Sun attachments/image tracks.'
        foreach ($sun in $visibleSuns) {
            $scale=[double]::Parse($sun.Groups[4].Value,[Globalization.CultureInfo]::InvariantCulture);$targetScale=[double]::Parse($sun.Groups[5].Value,[Globalization.CultureInfo]::InvariantCulture)
            Assert-Chomp ([int]$sun.Groups[2].Value -ge 80 -and $targetScale -gt 0 -and $scale+0.001 -ge $targetScale -and @($coins|Where-Object {$_.Groups[1].Value -eq $sun.Groups[1].Value}).Count -eq 1) 'Native Sun checkpoint age/actual scale/identity differs.'
            if ($role -eq 'host') {Assert-Chomp ([int]$sun.Groups[3].Value -ge 80) 'Host Sun did not wait actual native Coin age80.'}
        }
        Assert-Chomp ($text -match ('CHOMPER_REWARD role='+$role+' actor=\d+ token=\d+ count=4 value=25 total=100 currentCol=7 currentRow=2 wallet=300 duplicate=0 pendingHeldAlive=1')) 'Current-cell delayed4x25 reward missing.'
        Assert-Chomp ($text -match ('CHOMPER_RECOVERED role='+$role+' id='+$buys[1].Groups[3].Value+' token='+$buys[1].Groups[4].Value+' HP=4000 twoNatural40s=1 clamp=1 noHPWrites=1')) 'Natural NutChomper final recovery/clamp not projected.'
        Assert-Chomp ($text -match ('CHOMPER_BLAST role='+$role+' victim=\d+ initialHP=1500 dying=1 sourceCherry1800=1 tick=\d+')) 'First real CherryChomper source blast did not affect its separate native victim.'
        foreach ($terminal in @(0,1)) {Assert-Chomp ($text -match ('CHOMPER_FX role='+$role+' terminal='+$terminal+' livePowie=[1-9]\d* nativeEffectHolder=1 tick=\d+')) 'Actual native particle holder missed first or terminal blast.'}
        Assert-Chomp ($text -match ('CHOMPER_TERMINAL role='+$role+' actor='+$buys[5].Groups[3].Value+' token='+$buys[5].Groups[4].Value+' actualLastkillAward=1 liveNativeFX=1 continuationAfterAward=1 terminalTargetDamageNotClaimed=1 phaseWritesAfterSetup=0')) 'Actual final native award/accepted continuation/FX missing.'
        Assert-Chomp ($text -match ('CHOMPER_COMPLETE role='+$role+' kinds=1012,1014,1016 purchases=6 initialSun=1000 totalCost=700 finalSun=300 uses=3,1,1,1 naturalDigest=4000 heal1000AndClamp=1 coins4x25=1 gloveMoves=2 terminalAdmission=1 nativeBoard=1 secondWorld=0 plantHPWrites=0 plantTimerWrites=0 phaseWritesAfterSetup=0 sourceZombieAIUnported=1')) 'Shared native Chomper cohort incomplete.'
        foreach ($kind in @(1012,1014,1016)) {
            $bites=@([regex]::Matches($text,('CHOMPER_BITE role='+$role+' kind='+$kind+' id=\d+ token=\d+ cycle=(\d+) observedElapsed=\d+ actualDigest=-?\d+ tick=\d+ hostExact=[01]')))
            $expected=if ($kind -eq 1014) {1} else {2};Assert-Chomp ($bites.Count -eq $expected) 'Actual natural meal count differs.'
        }
    }
    for ($i=0;$i -lt 6;++$i) {for ($g=1;$g -le 10;++$g) {Assert-Chomp ($purchaseSets[0][$i].Groups[$g].Value -eq $purchaseSets[1][$i].Groups[$g].Value) 'Canonical purchase/ancestry/owner/packet differs between peers.'}}
    for ($i=0;$i -lt 2;++$i) {for ($g=1;$g -le 8;++$g) {Assert-Chomp ($moveSets[0][$i].Groups[$g].Value -eq $moveSets[1][$i].Groups[$g].Value) 'Canonical ordinary move/HP/provenance differs between peers.'}}
    $hostCoinIds=@($coinSets[0]|ForEach-Object {$_.Groups[1].Value}|Sort-Object);$clientCoinIds=@($coinSets[1]|ForEach-Object {$_.Groups[1].Value}|Sort-Object)
    Assert-Chomp (($hostCoinIds -join ',') -eq ($clientCoinIds -join ',')) 'Authoritative reward Coin IDs differ.'
    foreach ($coin in $coinSets[0]) {Assert-Chomp ($coin.Groups[4].Value -eq '200') 'Reward did not wait actual200 native ticks.'}
    $locks=@([regex]::Matches($hostText,'CHOMPER_LOCK kind=(\d+) id=(\d+) token=(\d+) cycle=(\d+) target=(\d+) nativeGeneration=(\d+) speed=([0-9.eE+\-]+) biteTicks=(\d+) chewTicks=(\d+) swallowTicks=(\d+) tick=(\d+)'))
    Assert-Chomp ($locks.Count -eq 5 -and @($locks|ForEach-Object {$_.Groups[5].Value}|Select-Object -Unique).Count -eq 5) 'Five genuine generation-locked targets missing.'
    foreach ($lock in $locks) {
        $kind=[int]$lock.Groups[1].Value;$id=$lock.Groups[2].Value;$cycle=$lock.Groups[4].Value
        $speed=[double][single]::Parse($lock.Groups[7].Value,[Globalization.CultureInfo]::InvariantCulture)
        Assert-Chomp ($speed -ge [double][single]0.9 -and $speed -le [double][single]1.1 -and [uint32]$lock.Groups[6].Value -gt 0) 'Source animation speed/native generation invalid.'
        if ($kind -eq 1012) {$bite=[double][single]1.2166667;$chew=[double][single]1.9166666;$swallow=1.0;$multiplier=[double][single]0.8}
        else {$bite=0.65;$chew=0.85;$swallow=2.0833333;$multiplier=0.8}
        $biteTicks=[int][Math]::Ceiling($bite/$speed*100);$chewTicks=[int][Math]::Ceiling($chew/$speed*100);$swallowTicks=[int][Math]::Ceiling($swallow/($multiplier*$speed)*100)
        Assert-Chomp ([int]$lock.Groups[8].Value -eq $biteTicks -and [int]$lock.Groups[9].Value -eq $chewTicks -and [int]$lock.Groups[10].Value -eq $swallowTicks) 'Source/profile promoted-float phase deadline differs.'
        $biteLog=[regex]::Match($hostText,('CHOMPER_BITE role=host kind='+$kind+' id='+$id+' token=\d+ cycle='+$cycle+' observedElapsed=(\d+) actualDigest=4000 tick=(\d+) hostExact=1'))
        Assert-Chomp ($biteLog.Success -and [int]$biteLog.Groups[1].Value -eq $biteTicks -and [int]$biteLog.Groups[2].Value-[int]$lock.Groups[11].Value -eq $biteTicks) 'Actual native bite did not occur on the source deadline.'
    }
    $swallows=@([regex]::Matches($hostText,'CHOMPER_SWALLOW role=host kind=(\d+) id=\d+ token=\d+ cycle=\d+ elapsedFromBite=(\d+) HP=\d+ phaseCache=[0-9.eE+\-]+ tick=\d+'))
    Assert-Chomp ($swallows.Count -eq 4) 'Four complete natural40s digests missing (Nut twice, Sun and Cherry once).'
    foreach ($swallow in $swallows) {Assert-Chomp ($swallow.Groups[2].Value -eq '4000') 'Actual digest countdown is not40 seconds.'}
    $heals=@([regex]::Matches($hostText,'CHOMPER_HEAL cycle=(\d+) id=\d+ token=\d+ before=(\d+) after=(\d+) clamp=([01]) tick=\d+ naturalDigestTicks=4000'))
    Assert-Chomp ($heals.Count -eq 2 -and $heals[0].Groups[1].Value -eq '1' -and [int]$heals[0].Groups[2].Value -gt 0 -and [int]$heals[0].Groups[2].Value -le 2500 -and [int]$heals[0].Groups[3].Value -eq [int]$heals[0].Groups[2].Value+1000 -and $heals[0].Groups[4].Value -eq '0' -and $heals[1].Groups[1].Value -eq '2' -and [int]$heals[1].Groups[2].Value -gt 3000 -and $heals[1].Groups[3].Value -eq '4000' -and $heals[1].Groups[4].Value -eq '1') 'Natural Recover1000 then max clamp differs.'
    $sunId=$purchaseSets[0][3].Groups[3].Value;$sunToken=$purchaseSets[0][3].Groups[4].Value
    Assert-Chomp (@([regex]::Matches($hostText,('Fusion native Chomper reward scheduled id='+$sunId+' token='+$sunToken+' kind=1014 pending=200 count=4 tick=\d+'))).Count -eq 1 -and
        @([regex]::Matches($hostText,('Fusion native Chomper reward id='+$sunId+' token='+$sunToken+' kind=1014 count=4 value=25 emitted=1 capacityOmit=0 col=7 row=2 tick=\d+'))).Count -eq 1) 'Delayed Sun reward was not exactly one consumed successful native callback.'
    $blasts=@([regex]::Matches($hostText,'Fusion native Death blast policy=0 row=4 center=[0-9.eE+\-]+,[0-9.eE+\-]+ damage=1800 shake=1 tick=\d+ awarded=([01])'))
    Assert-Chomp ($blasts.Count -eq 2 -and $blasts[0].Groups[1].Value -eq '0' -and $blasts[1].Groups[1].Value -eq '1') 'First normal and actual post-award Cherry continuations missing.'
    Assert-Chomp ($hostText -match 'CHOMPER_TERMINAL_SETUP currentWave=\d+ numWaves=\d+ guard=\d+ initialFixtureCondition=1 phaseWritesAfterSetup=0' -and @([regex]::Matches($hostText,'CHOMPER_NPC index=\d+ id=\d+ row=[024] initialHP=\d+ actualNativeFamily=normal sourceZombieAIUnported=1')).Count -eq 6) 'Explicit initial terminal/native NPC setup missing.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$chompCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_FUSION_CHOMPER_TERMINAL role=host');$ci=$clientText.IndexOf('SMOKE_FUSION_CHOMPER_TERMINAL role=client');Assert-Chomp ($hi -ge 0 -and $ci -ge 0) 'Final native terminal barrier missing.'
    $tp=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$cp=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED);$chompPost=@($cp|Where-Object {$tp -contains $_}|Select-Object -Unique)
    Assert-Chomp ($chompCommon.Count -ge 8 -and $chompPost.Count -ge 2) 'Insufficient common/postaction authoritative snapshots.'
    $chompFrames=@((Assert-ChompFrames $chompHost host $chompHostWidth),(Assert-ChompFrames $chompClient client $chompClientWidth));Assert-Chomp ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $chompExeSha) 'Executable changed during owned run.';$chompPassed=$true
} catch {$chompError=$_.Exception.Message;if ($_.ScriptStackTrace) {$chompError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Chomper: '+$chompError)}
finally {
    foreach ($name in $chompEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$chompEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $chompClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $chompHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$chompPassed=$false;$chompError+=' Owned cleanup failed.'}
    Ensure-Directory $chompRoot;$summary=[ordered]@{status=$(if ($chompPassed) {'pass'} else {'fail'});error=$chompError;fixture='FusionNativeChomperCohort';source_kinds=@(1012,1014,1016);run_id=$chompRunId;
        started_utc=$chompStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$chompExeSha;common_snapshot_hashes=$chompCommon;postaction_common_hashes=$chompPost;frames=$chompFrames;
        owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$chompRoot;campaign_initial_sun=500;fixture_initial_sun=1000;purchase_total=700;final_sun=300;sun_writes_after_setup=0;plant_hp_writes=0;plant_timer_writes=0;native_board=$true;second_world=$false;
        limits='Controlled native NORMAL NPCs, source zombie AI unported. One explicit initial final-wave condition; actual lastkill awards before the accepted Cherry continuation. First separate1500HP victim dies to actual Cherry1800; terminal target already swallowed so terminal target damage is not claimed. Native state/cached source poses/Coin IDs are client assertions; exact native counters and generation locks are host-only. No runtime capacity exhaustion, removal cancellation, exactHP1333/2666 visual hysteresis, unsupported families or real late join/reconnect.';
        visual_review='Root reviews actual chew/cracks/reward/recovered/terminal client frames; nonblank BMP alone is not acceptance'}
    Write-Utf8NoBom -Path (Join-Path $chompRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($chompPassed) {exit 0} else {exit 1}
