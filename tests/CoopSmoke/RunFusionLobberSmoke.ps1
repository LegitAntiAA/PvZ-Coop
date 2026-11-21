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
$ErrorActionPreference='Stop'
$lobHostWidth=$HostWindowWidth; $lobClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $lobHostWidth -ClientWindowWidth $lobClientWidth
if ($HostWindowWidth -ne $lobHostWidth -or $ClientWindowWidth -ne $lobClientWidth) { throw 'Shared helper changed requested widths.' }
$lobRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $lobRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $lobGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $lobRepo 'work\fusion-lobber' }
$lobRunId=[Guid]::NewGuid().ToString('N'); $lobRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$lobRunId)
$lobHost=Join-Path $lobRoot 'host'; $lobClient=Join-Path $lobRoot 'client'
$lobHostLog=Join-Path $lobHost 'logs'; $lobClientLog=Join-Path $lobClient 'logs'
$lobHostProcess=$null; $lobClientProcess=$null; $lobPassed=$false; $lobError=''
$lobFrames=@(); $lobCommon=@(); $lobPost=@(); $lobExeSha=''; $lobStarted=[DateTime]::UtcNow; $lobEnvironment=@{}; $lobOffsetGeometry=@(); $lobOffsetHits=@()
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_LOBBER','PVZ_COOP_SMOKE_FUSION_CHOMPER','PVZ_COOP_SMOKE_FUSION_DEATH','PVZ_COOP_SMOKE_FUSION_FIRE','PVZ_COOP_SMOKE_FUSION_COLD','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_PRODUCER','PVZ_COOP_SMOKE_FUSION_SPLIT','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $lobEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Lob([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-LobFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $required=@('chooser','flight','impact','complete')
    foreach ($name in $required) {
        $path=Join-Path $Directory ('fusion_lobber_'+$name+'.bmp')
        Assert-Lob (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Lob ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Lob ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $lobGame 'main.pak'),(Join-Path $lobGame 'SDL3.dll'))) {
        Assert-Lob (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $lobExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($lobHostLog,$lobClientLog,(Join-Path $lobHost 'appdata'),(Join-Path $lobClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $lobEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_LOBBER='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$lobHost} else {$lobClient}
        $width=if ($role -eq 'host') {$lobHostWidth} else {$lobClientWidth}
        $nickname=if ($role -eq 'host') {'LobberHost'} else {'LobberClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native shared Lobber cohort: two hidden owned windows; '+$lobRoot)
    $lobHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $lobGame -ManifestPath (Join-Path $lobHost 'smoke.ini') -Role host -RunId $lobRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $lobRoot 'pids.json') -Executable $GameExe -HostProcess $lobHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $lobHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $lobHostProcess -Role host -TimeoutSeconds 120
    $lobClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $lobGame -ManifestPath (Join-Path $lobClient 'smoke.ini') -Role client -RunId $lobRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $lobRoot 'pids.json') -Executable $GameExe -HostProcess $lobHostProcess -ClientProcess $lobClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $lobHost 'result.json') -ClientResultPath (Join-Path $lobClient 'result.json') `
        -HostProcess $lobHostProcess -ClientProcess $lobClientProcess -HostLogDirectory $lobHostLog -ClientLogDirectory $lobClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Lob ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 46 -and $results[1].protocol -eq 46) 'Peer identity/protocol46 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Lob ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $lobHostLog;$clientText=Get-CombinedLogText $lobClientLog
    Assert-Lob ($hostText -match 'HELLO slot=1 protocol=46 expected=46' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol46/Fusion handshake missing.'
    $buysByRole=@();$flightsByRole=@();$hitsByRole=@();$offsetsByRole=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Lob ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') ('Actual Lobber runtime failure: '+$role)
        Assert-Lob ($text -match ('LOBBER_READY role='+$role+' nativeSDL=1 hoverObserved=1') -and $text -match ('LOBBER_POOL role='+$role+' eligible=26 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1')) 'Shared real chooser/own-first/Ready missing.'
        $buys=@([regex]::Matches($text,('LOBBER_PURCHASE role='+$role+' kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) parents=(\d+),(\d+) HP=300 sun=(\d+) nativeSDL=1')))
        Assert-Lob ($buys.Count -eq 3) 'Three real paid source purchases missing.'
        $kinds=@(26,32,1116);$sun=@(400,100,25);$owners=@(0,0,1)
        for ($i=0;$i -lt 3;++$i) {Assert-Lob ([int]$buys[$i].Groups[1].Value -eq $kinds[$i] -and [int]$buys[$i].Groups[8].Value -eq $sun[$i] -and [int]$buys[$i].Groups[5].Value -eq $owners[$i] -and [uint32]$buys[$i].Groups[3].Value -gt 0) 'Real source kind/price/owner/identity differs.'}
        Assert-Lob ($buys[2].Groups[6].Value -eq '32' -and $buys[2].Groups[7].Value -eq '10' -and $buys[2].Groups[2].Value -ne $buys[1].Groups[2].Value -and $buys[2].Groups[3].Value -ne $buys[1].Groups[3].Value) 'Actual Winter directed recipe/fresh identity missing.';$buysByRole+=,@($buys)
        $flights=@([regex]::Matches($text,('LOBBER_PROJECTILE role='+$role+' kind=(\d+) id=(\d+) bullet=(\d+) damage=(\d+) motion=LOBBED x=[0-9.eE+\-]+ y=[0-9.eE+\-]+ Z=[0-9.eE+\-]+ canonicalActor=1')))
        Assert-Lob ($flights.Count -eq 9 -and @($flights|ForEach-Object {$_.Groups[2].Value}|Select-Object -Unique).Count -eq 9) 'Nine distinct actual lobbed projectile identities missing.';$flightsByRole+=,@($flights)
        foreach ($kind in @(26,32,1116)) {
            $group=@($flights|Where-Object {[int]$_.Groups[1].Value -eq $kind});$expected=if ($kind -eq 1116) {5} else {2};$payload=if ($kind -eq 26) {52} elseif ($kind -eq 32) {59} else {60};$damage=if ($kind -eq 26) {40} else {80}
            Assert-Lob ($group.Count -eq $expected) 'Wrong natural projectile count per source kind.'
            foreach ($p in $group) {Assert-Lob ([int]$p.Groups[3].Value -eq $payload -and [int]$p.Groups[4].Value -eq $damage) 'Actual projectile type/metadata differs.'}
            Assert-Lob ($text -match ('LOBBER_COMBAT role='+$role+' kind='+$kind+' hits='+$expected+' sourceSplash=1 controlsUntouched=1 sourceZombieAIUnported=1')) 'Native primary/splash/control ledger incomplete.'
        }
        $hits=@([regex]::Matches($text,('LOBBER_HIT role='+$role+' kind=(\d+) hit=(\d+) target=(\d+) damage=(\d+) body=(-?\d+) helmet=(\d+) shield=(\d+) level=(\d+) threshold=(\d+) slow=(\d+) freeze=(\d+) splashCount=(\d+) splashDamage=(\d+) tick=\d+ noHPWrites=1')))
        Assert-Lob ($hits.Count -eq 9) 'Nine natural impact ledger entries missing.';$hitsByRole+=,@($hits)
        $cabbage=@($hits|Where-Object {$_.Groups[1].Value -eq '26'});$melon=@($hits|Where-Object {$_.Groups[1].Value -eq '32'});$winter=@($hits|Where-Object {$_.Groups[1].Value -eq '1116'})
        Assert-Lob ($cabbage.Count -eq 2 -and $cabbage[0].Groups[4].Value -eq '40' -and $cabbage[0].Groups[5].Value -eq '4960' -and $cabbage[1].Groups[5].Value -eq '5000' -and $cabbage[1].Groups[6].Value -eq '1160' -and $cabbage[1].Groups[7].Value -eq '3000') 'Bare40 or Shieldless40 with native bucket/door differs.'
        Assert-Lob ($melon.Count -eq 2 -and $melon[0].Groups[6].Value -eq '1120' -and $melon[1].Groups[6].Value -eq '1040' -and $melon[0].Groups[7].Value -eq '3000' -and $melon[1].Groups[7].Value -eq '3000') 'Melon primary80 did not bypass shield and preserve helmet routing.'
        foreach ($hit in $melon) {Assert-Lob ($hit.Groups[12].Value -eq '3' -and $hit.Groups[13].Value -eq '26') 'Source two existing neighbors plus offset-only BODY NormalAll26 splash differs.'}
        $offsets=@([regex]::Matches($text,('LOBBER_OFFSET_HIT role='+$role+' kind=32 hit=(\d+) target=(\d+) body=(\d+) damage=(\d+) candidates=3 legacyWouldMiss=1 nativeBody=1 positionOnly=1')))
        Assert-Lob ($offsets.Count -eq 2 -and $offsets[0].Groups[2].Value -eq $offsets[1].Groups[2].Value) 'Two actual hits on the same offset-only native BODY identity missing.'
        for ($i=0;$i -lt 2;++$i) {
            Assert-Lob ([int]$offsets[$i].Groups[1].Value -eq $i+1 -and [int]$offsets[$i].Groups[3].Value -eq 5000-26*($i+1) -and $offsets[$i].Groups[4].Value -eq '26') 'Offset-only native BODY did not receive each source26 callback exactly once.'
            $lobOffsetHits+=[ordered]@{role=$role;hit=[int]$offsets[$i].Groups[1].Value;target_id=[int]$offsets[$i].Groups[2].Value;body=[int]$offsets[$i].Groups[3].Value;damage=26}
        };$offsetsByRole+=,@($offsets)
        $winterDamage=@(80,80,80,80,320);$winterHP=@(80,0,4920,4840,4520);$winterLevel=@(50,0,50,0,50);$winterThreshold=@(100,200,100,200,200)
        Assert-Lob ($winter.Count -eq 5 -and $winter[0].Groups[3].Value -eq $winter[1].Groups[3].Value -and $winter[2].Groups[3].Value -ne $winter[1].Groups[3].Value) 'Weak lethal2-hit then real strong-target reacquisition missing.'
        for ($i=0;$i -lt 5;++$i) {
            Assert-Lob ([int]$winter[$i].Groups[4].Value -eq $winterDamage[$i] -and [int]$winter[$i].Groups[5].Value -eq $winterHP[$i] -and [int]$winter[$i].Groups[8].Value -eq $winterLevel[$i] -and [int]$winter[$i].Groups[9].Value -eq $winterThreshold[$i] -and [int]$winter[$i].Groups[10].Value -gt 0) 'Winter pre-damage frozen multiplier/postdamage50 level order differs.'
            $mustFreeze=$i -eq 1 -or $i -ge 3;Assert-Lob ($(if ($mustFreeze) {[int]$winter[$i].Groups[11].Value -gt 0} else {$winter[$i].Groups[11].Value -eq '0'})) 'Merely chilled was confused with actually frozen.'
            Assert-Lob ([int]$winter[$i].Groups[12].Value -eq $(if ($i -ge 2) {2} else {0})) 'Weak-front versus strong-target splash scene differs.'
        }
        $retired=[regex]::Match($text,('LOBBER_DYING_RETIRED role='+$role+' id='+$winter[1].Groups[3].Value+' elapsed=(\d+) positiveRate=([01]) animationProgress=([01]) retiredBefore400=1'))
        Assert-Lob ($retired.Success -and [int]$retired.Groups[1].Value -lt 400) 'Post-lethal Add50 recreated freeze FX or prevented normal dying retirement.'
        Assert-Lob ($text -match ('LOBBER_COMPLETE role='+$role+' kinds=26,32,1116 purchases=3 initialSun=500 totalCost=475 finalSun=25 uses=1,1,1 hits=2,2,5 nativeMidpoint=1 primaryShieldless=1 splash26=1 offsetOnlyBody=1 offsetCandidates=3 sourceCenter=1 winter80Then80Then320=1 lethal160Retired=1 nativeBoard=1 secondWorld=0 HPWritesAfterSetup=0 sunWrites=0 timerWrites=0 coldWrites=0 sourceZombieAIUnported=1')) 'Shared bounded native Lobber cohort incomplete.'
    }
    for ($i=0;$i -lt 3;++$i) {for ($g=1;$g -le 8;++$g) {Assert-Lob ($buysByRole[0][$i].Groups[$g].Value -eq $buysByRole[1][$i].Groups[$g].Value) 'Canonical purchase/ancestry/owner/packet differs between peers.'}}
    $hostIDs=@($flightsByRole[0]|ForEach-Object {$_.Groups[2].Value}|Sort-Object);$clientIDs=@($flightsByRole[1]|ForEach-Object {$_.Groups[2].Value}|Sort-Object)
    Assert-Lob (($hostIDs -join ',') -eq ($clientIDs -join ',')) 'Client missed or changed actual canonical lobbed projectile IDs.'
    for ($i=0;$i -lt 9;++$i) {foreach ($g in @(1,2,3,4,5,6,7,8,9,12,13)) {Assert-Lob ($hitsByRole[0][$i].Groups[$g].Value -eq $hitsByRole[1][$i].Groups[$g].Value) 'Canonical impact/cold/armor/outcome differs between peers.'}}
    for ($i=0;$i -lt 2;++$i) {for ($g=1;$g -le 4;++$g) {Assert-Lob ($offsetsByRole[0][$i].Groups[$g].Value -eq $offsetsByRole[1][$i].Groups[$g].Value) 'Canonical offset-only BODY identity/outcome differs between peers.'}}
    $offsetGeometry=@([regex]::Matches($hostText,'LOBBER_OFFSET_GEOMETRY role=host kind=32 projectile=(\d+) target=(\d+) primary=(\d+) step=(\d+) root=([0-9.eE+\-]+),([0-9.eE+\-]+) area=([0-9.eE+\-]+),([0-9.eE+\-]+) body=(-?\d+),(-?\d+),(\d+),(\d+) legacyHit=0 correctedHit=1 candidates=3 legacyCandidates=2 existingNeighborHits=2 outsideHit=0 impactPostTick=(\d+) positionOnly=1'))
    Assert-Lob ($offsetGeometry.Count -eq 2 -and $offsetGeometry[0].Groups[1].Value -ne $offsetGeometry[1].Groups[1].Value) 'Two distinct real Melon flights with old-miss/new-hit native BODY geometry missing.'
    for ($i=0;$i -lt 2;++$i) {
        $edge=$offsetGeometry[$i];$culture=[Globalization.CultureInfo]::InvariantCulture
        $rx=[double]::Parse($edge.Groups[5].Value,$culture);$ry=[double]::Parse($edge.Groups[6].Value,$culture)
        $ax=[double]::Parse($edge.Groups[7].Value,$culture);$ay=[double]::Parse($edge.Groups[8].Value,$culture)
        Assert-Lob ([int]$edge.Groups[4].Value -eq $i+1 -and $edge.Groups[2].Value -eq $offsetsByRole[0][$i].Groups[2].Value -and [Math]::Abs($ax-$rx) -lt 0.00001 -and [Math]::Abs(($ay-$ry)-29.94011976047904) -lt 0.00001) 'Exact source rootY-.5/nativeY+.5 projection or canonical BODY identity differs.'
        $actual=[regex]::Match($hostText,('Fusion native Lobber impact projectile='+$edge.Groups[1].Value+' payload=59 primary='+$edge.Groups[3].Value+' count=3 splash=26 age=\d+ center=([0-9.eE+\-]+),([0-9.eE+\-]+) tick=(\d+) areaCenter=([0-9.eE+\-]+),([0-9.eE+\-]+)'))
        Assert-Lob ($actual.Success) 'Actual production shared splash branch did not capture all three native BODY/neighbor targets.'
        Assert-Lob ([Math]::Abs([double]::Parse($actual.Groups[1].Value,$culture)-$rx) -lt 0.001 -and [Math]::Abs([double]::Parse($actual.Groups[2].Value,$culture)-$ry) -lt 0.001 -and [Math]::Abs([double]::Parse($actual.Groups[4].Value,$culture)-$ax) -lt 0.001 -and [Math]::Abs([double]::Parse($actual.Groups[5].Value,$culture)-$ay) -lt 0.001 -and [int]$actual.Groups[3].Value+1 -eq [int]$edge.Groups[13].Value) 'Actual root/area impact diverged from the position-only native geometry setup.'
        $lobOffsetGeometry+=[ordered]@{projectile_id=[int]$edge.Groups[1].Value;target_id=[int]$edge.Groups[2].Value;primary_id=[int]$edge.Groups[3].Value;root=@($rx,$ry);area=@($ax,$ay);native_body=@([int]$edge.Groups[9].Value,[int]$edge.Groups[10].Value,[int]$edge.Groups[11].Value,[int]$edge.Groups[12].Value);legacy_hit=$false;corrected_hit=$true;candidates=3;legacy_candidates=2;existing_neighbor_hits=2;outside_hit=$false}
    }
    $fires=@([regex]::Matches($hostText,'LOBBER_FIRE kind=(\d+) id=(\d+) bullet=(\d+) damage=(\d+) fireTick=(\d+) triggerTick=(\d+) deadline=(\d+) age=(\d+) vx=([0-9.eE+\-]+) vz=([0-9.eE+\-]+) acc=([0-9.eE+\-]+) stationaryLead=1 nativeMotion=LOBBED'))
    Assert-Lob ($fires.Count -eq 9) 'Natural host source event/lead/physics births missing.'
    foreach ($fire in $fires) {Assert-Lob ([int]$fire.Groups[5].Value -eq [int]$fire.Groups[6].Value+[int]$fire.Groups[7].Value -and [double]::Parse($fire.Groups[9].Value,[Globalization.CultureInfo]::InvariantCulture) -gt 0 -and [Math]::Abs([double]::Parse($fire.Groups[11].Value,[Globalization.CultureInfo]::InvariantCulture)-0.08811377245508983) -lt 0.000001) 'Actual event tick/source gravity differs.'}
    $retire=@([regex]::Matches($hostText,'LOBBER_RETIRE role=host kind=\d+ id=\d+ lastAge=(\d+) midpointSamples=(\d+) observedTicks=\d+'))
    Assert-Lob ($retire.Count -eq 9) 'Actual nine projectile retirements missing.';foreach ($entry in $retire) {Assert-Lob ([int]$entry.Groups[1].Value -gt 35 -and [int]$entry.Groups[2].Value -gt 30) 'Flight did not exercise real native midpoint integration.'}
    $weak=@($hitsByRole[0]|Where-Object {$_.Groups[1].Value -eq '1116'})[1].Groups[3].Value
    Assert-Lob ($hostText -match ('Fusion native cold add target='+$weak+' amount=50 level=0 threshold=200 slow=\d+ freeze=400 tick=\d+ retiring=1 hp=0')) 'Actual lethal Add50 retiring-domain continuation missing.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$lobCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_FUSION_LOBBER_COMBAT role=host kind=1116');$ci=$clientText.IndexOf('SMOKE_FUSION_LOBBER_COMBAT role=client kind=1116');Assert-Lob ($hi -ge 0 -and $ci -ge 0) 'Last combat barrier missing.'
    $tp=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$cp=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED);$lobPost=@($cp|Where-Object {$tp -contains $_}|Select-Object -Unique)
    Assert-Lob ($lobCommon.Count -ge 8 -and $lobPost.Count -ge 2) 'Insufficient common/postaction authoritative snapshots.'
    $lobFrames=@((Assert-LobFrames $lobHost host $lobHostWidth),(Assert-LobFrames $lobClient client $lobClientWidth));Assert-Lob ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $lobExeSha) 'Executable changed during owned run.';$lobPassed=$true
} catch {$lobError=$_.Exception.Message;if ($_.ScriptStackTrace) {$lobError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Lobber: '+$lobError)}
finally {
    foreach ($name in $lobEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$lobEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $lobClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $lobHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$lobPassed=$false;$lobError+=' Owned cleanup failed.'}
    Ensure-Directory $lobRoot;$summary=[ordered]@{status=$(if ($lobPassed) {'pass'} else {'fail'});error=$lobError;fixture='FusionNativeLobber';source_kinds=@(26,32,1116);run_id=$lobRunId;
        started_utc=$lobStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$lobExeSha;common_snapshot_hashes=$lobCommon;postaction_common_hashes=$lobPost;frames=$lobFrames;
        owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$lobRoot;initial_sun=500;purchase_total=475;final_sun=25;sun_writes=0;plant_hp_writes=0;plant_timer_writes=0;cold_writes=0;native_board=$true;second_world=$false;
        melon_secondary_count=3;winter_secondary_counts=@(0,0,2,2,2);source_area_offset_native_y=29.94011976047904;offset_body_geometry=$lobOffsetGeometry;offset_body_hits=$lobOffsetHits;offset_only_body_verified=[bool]($lobPassed -and $lobOffsetGeometry.Count -eq 2 -and $lobOffsetHits.Count -eq 4);
        limits='Controlled native NORMAL NPCs; source zombie AI unported. Target HP/armor written once on creation, only position/standing pose later; dying poses never frozen by fixture. Actual stationary-target solve, native midpoint adaptation, payloads/armor, unchanged two upper neighbors plus one lower native BODY missed by old center, outside control, natural Winter80/80/320 and lethal160HP retirement. Moving-target lead, retired-generation fallback, near-spawn/rising collision, no-target ground splash, TTL20s and N24/25 boundary are pure source vectors, not paired runtime claims. Client never claims unsent velocity/age/aim.';
        visual_review='Root reviews actual flight/postMelon/Winter source models, native armor and cold client frames; nonblank BMP alone is not acceptance'}
    Write-Utf8NoBom -Path (Join-Path $lobRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($lobPassed) {exit 0} else {exit 1}
