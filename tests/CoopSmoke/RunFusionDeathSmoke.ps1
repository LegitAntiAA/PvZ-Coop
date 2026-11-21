#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=480,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$deathHostWidth=$HostWindowWidth; $deathClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $deathHostWidth -ClientWindowWidth $deathClientWidth
if ($HostWindowWidth -ne $deathHostWidth -or $ClientWindowWidth -ne $deathClientWidth) { throw 'Shared helper changed requested widths.' }
$deathRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $deathRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $deathGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $deathRepo 'work\fusion-death' }
$deathRunId=[Guid]::NewGuid().ToString('N'); $deathRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$deathRunId)
$deathHost=Join-Path $deathRoot 'host'; $deathClient=Join-Path $deathRoot 'client'
$deathHostLog=Join-Path $deathHost 'logs'; $deathClientLog=Join-Path $deathClient 'logs'
$deathHostProcess=$null; $deathClientProcess=$null; $deathPassed=$false; $deathError=''
$deathFrames=@(); $deathCommon=@(); $deathPost=@(); $deathExeSha=''; $deathStarted=[DateTime]::UtcNow; $deathEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_DEATH','PVZ_COOP_SMOKE_FUSION_FIRE','PVZ_COOP_SMOKE_FUSION_COLD','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_PRODUCER','PVZ_COOP_SMOKE_FUSION_SPLIT','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $deathEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Death([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-DeathFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $required=@('chooser','playing','low','world','terminal','complete')
    foreach ($name in $required) {
        $path=Join-Path $Directory ('fusion_death_'+$name+'.bmp')
        Assert-Death (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Death ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Death ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $deathGame 'main.pak'),(Join-Path $deathGame 'SDL3.dll'))) {
        Assert-Death (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $deathExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($deathHostLog,$deathClientLog,(Join-Path $deathHost 'appdata'),(Join-Path $deathClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $deathEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_DEATH='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$deathHost} else {$deathClient}
        $width=if ($role -eq 'host') {$deathHostWidth} else {$deathClientWidth}
        $nickname=if ($role -eq 'host') {'DeathHost'} else {'DeathClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native lifecycle/death batch: two hidden owned windows; '+$deathRoot)
    $deathHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $deathGame -ManifestPath (Join-Path $deathHost 'smoke.ini') -Role host -RunId $deathRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $deathRoot 'pids.json') -Executable $GameExe -HostProcess $deathHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $deathHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $deathHostProcess -Role host -TimeoutSeconds 120
    $deathClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $deathGame -ManifestPath (Join-Path $deathClient 'smoke.ini') -Role client -RunId $deathRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $deathRoot 'pids.json') -Executable $GameExe -HostProcess $deathHostProcess -ClientProcess $deathClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $deathHost 'result.json') -ClientResultPath (Join-Path $deathClient 'result.json') `
        -HostProcess $deathHostProcess -ClientProcess $deathClientProcess -HostLogDirectory $deathHostLog -ClientLogDirectory $deathClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Death ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 44 -and $results[1].protocol -eq 44) 'Peer identity/protocol44 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Death ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $deathHostLog;$clientText=Get-CombinedLogText $deathClientLog
    Assert-Death ($hostText -match 'HELLO slot=1 protocol=44 expected=44' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol44/Fusion handshake missing.'
    $purchaseSets=@();$moveSets=@();$baseSets=@();$worldSets=@()
    $kinds=@(3,1003,3,1003,3,1010,0,3,4,1162,2);$money=@(1450,1300,1200,900,850,825,725,625,600,300,150)
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Death ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') ('Actual death runtime failure: '+$role)
        Assert-Death ($text -match ('DEATH_READY role='+$role+' nativeSDL=1 hoverObserved=1') -and $text -match ('DEATH_POOL role='+$role+' eligible=20 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1')) 'Actual shared chooser/own-first/Ready missing.'
        Assert-Death ($text -match ('DEATH_BUDGET role='+$role+' campaignInitialSun=500 fixtureInitialSun=1500 totalCost=1350 finalExpected=150 sunWritesAfterSetup=0')) 'Explicit one-time budget/golden missing.'
        $buys=@([regex]::Matches($text,('DEATH_PURCHASE role='+$role+' step=(\d+) kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) extra=(\d+) parents=(\d+),(\d+) HP=(\d+) flags=0 sun=(\d+) nativeSDL=1')))
        Assert-Death ($buys.Count -eq 11) 'Eleven real directed purchases missing.'
        for ($i=0;$i -lt 11;++$i) {Assert-Death ([int]$buys[$i].Groups[2].Value -eq $kinds[$i] -and [int]$buys[$i].Groups[11].Value -eq $money[$i] -and [uint32]$buys[$i].Groups[4].Value -gt 0) 'Actual source kind/budget/opaque token differs.'};$purchaseSets+=,@($buys)
        $moves=@([regex]::Matches($text,('DEATH_MOVE role='+$role+' kind=(\d+) id=(\d+) token=(\d+) col=(\d+) row=(\d+) HP=(\d+) flags=(\d+) owner=(\d+) packet=(\d+) sameInstance=1 nativeSDL=1')))
        Assert-Death ($moves.Count -eq 3 -and $moves[0].Groups[1].Value -eq '1003' -and $moves[0].Groups[4].Value -eq '6' -and $moves[0].Groups[5].Value -eq '2' -and $moves[0].Groups[7].Value -eq '0' -and
            $moves[1].Groups[1].Value -eq '1010' -and $moves[1].Groups[4].Value -eq '3' -and $moves[1].Groups[5].Value -eq '2' -and $moves[1].Groups[7].Value -eq '3' -and
            $moves[2].Groups[1].Value -eq '1003' -and $moves[2].Groups[4].Value -eq '5' -and $moves[2].Groups[5].Value -eq '1') 'Real instance/flags-preserving ordinary glove moves missing.';$moveSets+=,@($moves)
        $base=[regex]::Match($text,('DEATH_BASE_HEAL role='+$role+' receiver=(\d+) before=(\d+) after=(\d+) donor=(\d+) donorHP=(\d+) healColliderEntries=2 outsideEntries=0 noPlantHPWrites=1'))
        Assert-Death ($base.Success -and [int]$base.Groups[2].Value -gt 0 -and [int]$base.Groups[2].Value -le 1300 -and [int]$base.Groups[3].Value -eq [int]$base.Groups[2].Value+2000 -and [int]$base.Groups[5].Value -gt 0 -and [int]$base.Groups[5].Value -le 1300) 'Actual base Cherry healing2/compatible outside0 differs.';$baseSets+=$base
        Assert-Death ($text -match ('DEATH_HIGHBIT role='+$role+' old=\d+ token=\d+ kind=0 wallet=150 packetReady=1 heldPreserved=1 noEffects=1 realQueuedIntent=1')) 'Real typed high-bit rejection changed actor/wallet/card/held tuple.'
        $factory=[regex]::Match($text,('DEATH_FACTORY role='+$role+' attempts=4 fresh4=2 supported1010=(\d+) token=(\d+) flags=0 preserved0=(\d+) token=(\d+) wallet=150 unsupportedCell=1,1 normalFactory=1'))
        Assert-Death $factory.Success 'Clipped ordinary factory/unsupported preservation/supported result missing.';$worldSets+=$factory
        $mines=@([regex]::Matches($text,('DEATH_WORLD role='+$role+' index=(\d+) id=(\d+) token=(\d+) kind=4 col=0 row=(\d+) HP=300 owner=0 packet=(\d+) firstRemaining=(-?\d+) fresh=1 noSunCharge=1 noPacketUse=1')))
        Assert-Death ($mines.Count -eq 2 -and $mines[0].Groups[2].Value -ne $mines[1].Groups[2].Value -and $mines[0].Groups[4].Value -eq '0' -and $mines[1].Groups[4].Value -eq '1') 'Two real clipped source4 actors/fresh identities missing.'
        foreach ($mine in $mines) {
            $id=$mine.Groups[2].Value;$rise=[regex]::Match($text,('DEATH_RISE role='+$role+' id='+$id+' firstRemaining=(-?\d+) elapsed=(\d+) state=RISING nativeClock=1'))
            $armed=[regex]::Match($text,('DEATH_ARMED role='+$role+' id='+$id+' riseObservedTicks=(\d+) nativeLight=1 noTimerWrites=1'))
            Assert-Death ($rise.Success -and $armed.Success) 'World mine skipped actual preparation/rise/armed light projection.'
            if ($role -eq 'host') {Assert-Death ([int]$rise.Groups[1].Value -gt 0 -and [int]$rise.Groups[1].Value -le 501 -and [int]$rise.Groups[2].Value -ge [int]$rise.Groups[1].Value -and [int]$rise.Groups[2].Value -le [int]$rise.Groups[1].Value+2 -and [int]$armed.Groups[1].Value -ge 82 -and [int]$armed.Groups[1].Value -le 86) 'Natural source5s/84tick rise timing differs.'}
        }
        Assert-Death ($text -match ('DEATH_POTATO_LEDGER role='+$role+' parent=\d+ victims=3 initial=4500 mid500=1 low500=1 death1800=1 after=1700 noCharred=1 flagsPreservedByMove=3')) 'Natural PotatoNut actual native damage ledger missing.'
        Assert-Death ($text -match ('DEATH_TERMINAL role='+$role+' receiver='+$base.Groups[1].Value+' token=\d+ before='+$base.Groups[3].Value+' after=4000 healEntries=2 clamp=1 actualLastkillAward=1 phaseWritesAfterSetup=0 healAfterAwardContinuation=1')) 'Real lastkill award/healing continuation/clamp missing.'
        Assert-Death ($text -match ('DEATH_COMPLETE role='+$role+' kinds=1003,1010,1162 realPurchases=11 initialSun=1500 totalCost=1350 finalSun=150 uses=1,2,2,2,2,2 naturalBites=1 world4Attempts=4 healing2and0=1 boundary1PureOnly=1 nativeBoard=1 secondWorld=0 plantHPWrites=0 plantTimerWrites=0 phaseWritesAfterSetup=0 sourceZombieAIUnported=1')) 'Common bounded death scene incomplete.'
    }
    for ($i=0;$i -lt 11;++$i) {for ($g=1;$g -le 11;++$g) {Assert-Death ($purchaseSets[0][$i].Groups[$g].Value -eq $purchaseSets[1][$i].Groups[$g].Value) 'Canonical purchase/provenance/packet differs between peers.'}}
    for ($i=0;$i -lt 3;++$i) {for ($g=1;$g -le 9;++$g) {Assert-Death ($moveSets[0][$i].Groups[$g].Value -eq $moveSets[1][$i].Groups[$g].Value) 'Native ordinary move/canonical flags differs between peers.'}}
    for ($g=1;$g -le 4;++$g) {Assert-Death ($worldSets[0].Groups[$g].Value -eq $worldSets[1].Groups[$g].Value) 'World result/preserved actor canonical identity differs.'}
    $donor=$purchaseSets[0][1].Groups[3].Value;$donorToken=$purchaseSets[0][1].Groups[4].Value;$receiver=$purchaseSets[0][3].Groups[3].Value;$receiverToken=$purchaseSets[0][3].Groups[4].Value
    $potato=$purchaseSets[0][5].Groups[3].Value;$potatoToken=$purchaseSets[0][5].Groups[4].Value;$mine=$purchaseSets[0][9].Groups[3].Value;$mineToken=$purchaseSets[0][9].Groups[4].Value
    foreach ($pair in @(@($donor,$donorToken,1003),@($potato,$potatoToken,1010),@($mine,$mineToken,1162))) {
        Assert-Death ([regex]::Matches($hostText,('Fusion native Death remove id='+$pair[0]+' token='+$pair[1]+' kind='+$pair[2]+' reason=7 tick=\d+')).Count -eq 1) 'Canonical shovel did not dispatch exactly once for a death hybrid.'
    }
    $bands=@([regex]::Matches($hostText,('Fusion native Death threshold id='+$potato+' token='+$potatoToken+' kind=1010 hp=(\d+) flags=(\d+) damage=500 tick=\d+')))
    Assert-Death ($bands.Count -eq 2 -and $bands[0].Groups[2].Value -eq '1' -and [int]$bands[0].Groups[1].Value -lt 2666 -and [int]$bands[0].Groups[1].Value -gt 1333 -and $bands[1].Groups[2].Value -eq '3' -and [int]$bands[1].Groups[1].Value -lt 1333) 'Strict consumed500 bands repeated/skipped/wrong ordering.'
    $heals=@([regex]::Matches($hostText,('Fusion native Death heal id='+$receiver+' token='+$receiverToken+' kind=1003 collider=([01]) before=(\d+) after=(\d+) max=4000 tick=\d+')))
    Assert-Death ($heals.Count -eq 4 -and ((@($heals|ForEach-Object {$_.Groups[1].Value})) -join ',') -eq '0,1,0,1') 'Collider entries were deduplicated or duplicated.'
    Assert-Death ([int]$heals[0].Groups[2].Value -eq [int]$baseSets[0].Groups[2].Value -and [int]$heals[1].Groups[3].Value -eq [int]$baseSets[0].Groups[3].Value -and $heals[2].Groups[3].Value -eq '4000' -and $heals[3].Groups[2].Value -eq '4000' -and $heals[3].Groups[3].Value -eq '4000') 'Healing amount/clamp across both callbacks differs.'
    Assert-Death ([regex]::Matches($hostText,('Fusion native Death heal id='+$donor+' ')).Count -eq 0) 'Compatible receiver outside the actual source circle was incorrectly healed.'
    $spawns=@([regex]::Matches($hostText,('Fusion native Death spawn source='+$mine+' column=(\d+) row=(\d+) success=(\d+) child=(\d+) kind=(\d+) preparation=(-?\d+) tick=\d+')))
    Assert-Death ($spawns.Count -eq 4) 'Clipped3x3 must attempt exactly four cells.'
    $expectedCells=@('0,0,1,4,501','0,1,1,4,501','1,0,1,1010,-1','1,1,0,65535,-1')
    for ($i=0;$i -lt 4;++$i) {$parts=@($spawns[$i].Groups[1].Value,$spawns[$i].Groups[2].Value,$spawns[$i].Groups[3].Value,$spawns[$i].Groups[5].Value,$spawns[$i].Groups[6].Value);Assert-Death (($parts -join ',') -eq $expectedCells[$i]) 'Outer-column/inner-row clipping/ordinary factory result differs.'}
    Assert-Death ($hostText -match 'DEATH_HIGHBIT_SENT target=\d+ low16=\d+ packet=\d+ validLow16Recipe1030=1 ready=1 sufficientSun=1' -and $hostText -match 'Fusion native command slot=0 action=5 sequence=\d+ applied=0 column=1 row=1 packet=\d+') 'Deliberate high-bit Plant intent never reached/rejected by the actual authenticated native handler.'
    Assert-Death ($hostText -match 'DEATH_TERMINAL_SETUP guard=\d+ HP=1500 row=0 currentWave=\d+ numWaves=\d+ initialFixtureCondition=1 futurePhaseWrites=0' -and
        [regex]::Matches($hostText,'DEATH_TERMINAL_TARGET id=\d+ row=[02] HP=\d+ x=[0-9.eE+\-]+ positionOnly=1 noHPWrites=1').Count -eq 10) 'Initial terminal regression condition/real ten-actor lastkill scene missing.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$deathCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_FUSION_DEATH_TERMINAL role=host');$ci=$clientText.IndexOf('SMOKE_FUSION_DEATH_TERMINAL role=client');Assert-Death ($hi -ge 0 -and $ci -ge 0) 'Final healing terminal barrier missing.'
    $tp=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$cp=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED);$deathPost=@($cp|Where-Object {$tp -contains $_}|Select-Object -Unique)
    Assert-Death ($deathCommon.Count -ge 8 -and $deathPost.Count -ge 2) 'Insufficient common/postaction authoritative snapshots.'
    $deathFrames=@((Assert-DeathFrames $deathHost host $deathHostWidth),(Assert-DeathFrames $deathClient client $deathClientWidth));Assert-Death ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $deathExeSha) 'Executable changed during owned run.';$deathPassed=$true
} catch {$deathError=$_.Exception.Message;if ($_.ScriptStackTrace) {$deathError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Death: '+$deathError)}
finally {
    foreach ($name in $deathEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$deathEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $deathClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $deathHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$deathPassed=$false;$deathError+=' Owned cleanup failed.'}
    Ensure-Directory $deathRoot;$summary=[ordered]@{status=$(if ($deathPassed) {'pass'} else {'fail'});error=$deathError;fixture='FusionNativeDeathFamilies';source_kinds=@(1003,1010,1162);run_id=$deathRunId;
        started_utc=$deathStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$deathExeSha;common_snapshot_hashes=$deathCommon;postaction_common_hashes=$deathPost;frames=$deathFrames;
        owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$deathRoot;campaign_initial_sun=500;fixture_initial_sun=1500;purchase_total=1350;final_sun=150;sun_writes_after_setup=0;plant_hp_writes=0;plant_timer_writes=0;native_board=$true;second_world=$false;
        limits='Controlled native NORMAL NPCs, source zombie AI unported. One explicit initial final-wave/guard regression condition; actual lastkill must award before healing continuation. Runtime healing2/0, boundary1 pure tests only. No after-setup plant HP/timer/flags or phase writes. Capacity exhaustion/rare postconstruction RNG rollback, other death reasons, real late join and unimplemented source families are not runtime-covered.';
        visual_review='Root reviews actual low/cracks/world/terminal client frames; nonblank BMP alone is not acceptance'}
    Write-Utf8NoBom -Path (Join-Path $deathRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($deathPassed) {exit 0} else {exit 1}
