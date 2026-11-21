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
$fireHostWidth=$HostWindowWidth; $fireClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $fireHostWidth -ClientWindowWidth $fireClientWidth
if ($HostWindowWidth -ne $fireHostWidth -or $ClientWindowWidth -ne $fireClientWidth) { throw 'Shared helper changed requested widths.' }
$fireRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $fireRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $fireGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $fireRepo 'work\fusion-fire' }
$fireRunId=[Guid]::NewGuid().ToString('N'); $fireRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$fireRunId)
$fireHost=Join-Path $fireRoot 'host'; $fireClient=Join-Path $fireRoot 'client'
$fireHostLog=Join-Path $fireHost 'logs'; $fireClientLog=Join-Path $fireClient 'logs'
$fireHostProcess=$null; $fireClientProcess=$null; $firePassed=$false; $fireError=''
$fireFrames=@(); $fireCommon=@(); $firePost=@(); $fireExeSha=''; $fireStarted=[DateTime]::UtcNow; $fireEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_FIRE','PVZ_COOP_SMOKE_FUSION_COLD','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_PRODUCER','PVZ_COOP_SMOKE_FUSION_SPLIT','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $fireEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Fire([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-FireFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $required=@('chooser','playing','complete')
    if ($Role -eq 'client') { $required+=@('hot','moved') }
    foreach ($name in $required) {
        $path=Join-Path $Directory ('fusion_fire_'+$name+'.bmp')
        Assert-Fire (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Fire ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Fire ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $fireGame 'main.pak'),(Join-Path $fireGame 'SDL3.dll'))) {
        Assert-Fire (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $fireExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($fireHostLog,$fireClientLog,(Join-Path $fireHost 'appdata'),(Join-Path $fireClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $fireEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_FIRE='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$fireHost} else {$fireClient}
        $width=if ($role -eq 'host') {$fireHostWidth} else {$fireClientWidth}
        $nickname=if ($role -eq 'host') {'FireHost'} else {'FireClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native Torchwood/fire batch: two hidden owned windows; '+$fireRoot)
    $fireHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $fireGame -ManifestPath (Join-Path $fireHost 'smoke.ini') -Role host -RunId $fireRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $fireRoot 'pids.json') -Executable $GameExe -HostProcess $fireHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $fireHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $fireHostProcess -Role host -TimeoutSeconds 120
    $fireClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $fireGame -ManifestPath (Join-Path $fireClient 'smoke.ini') -Role client -RunId $fireRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $fireRoot 'pids.json') -Executable $GameExe -HostProcess $fireHostProcess -ClientProcess $fireClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $fireHost 'result.json') -ClientResultPath (Join-Path $fireClient 'result.json') `
        -HostProcess $fireHostProcess -ClientProcess $fireClientProcess -HostLogDirectory $fireHostLog -ClientLogDirectory $fireClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Fire ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 44 -and $results[1].protocol -eq 44) 'Peer identity/protocol44 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Fire ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $fireHostLog;$clientText=Get-CombinedLogText $fireClientLog
    Assert-Fire ($hostText -match 'HELLO slot=1 protocol=44 expected=44' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol44/Fusion handshake missing.'
    $purchaseSets=@();$moveSets=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Fire ($text -notmatch 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') ('Actual fire runtime failure: '+$role)
        Assert-Fire ($text -match ('FIRE_READY role='+$role+' nativeSDL=1 hoverObserved=1') -and $text -match ('FIRE_POOL role='+$role+' eligible=20 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1 unfiltered=1')) 'Actual chooser/own-first/shared Ready missing.'
        Assert-Fire ($text -match ('FIRE_BUDGET role='+$role+' campaignInitialSun=500 fixtureInitialSun=525 totalCost=525 finalExpected=0 sunWritesAfterSetup=0')) 'Explicit one-time fixture budget/golden missing.'
        $buys=@([regex]::Matches($text,('FIRE_PURCHASE role='+$role+' kind=(\d+) id=(\d+) token=(\d+) packet=(\d+) owner=(\d+) parents=(\d+),(\d+) sun=(\d+) HP=300 nativeSDL=1')))
        Assert-Fire ($buys.Count -eq 4) 'Four real native card purchases missing.'
        $kinds=@(0,18,18,1034);$owners=@(0,1,1,0);$money=@(425,250,75,0)
        for ($i=0;$i -lt 4;++$i) {Assert-Fire ([int]$buys[$i].Groups[1].Value -eq $kinds[$i] -and [int]$buys[$i].Groups[5].Value -eq $owners[$i] -and [int]$buys[$i].Groups[8].Value -eq $money[$i] -and [uint32]$buys[$i].Groups[3].Value -gt 0) 'Actual purchase kind/owner/cost/token differs.'}
        Assert-Fire ($buys[1].Groups[4].Value -eq $buys[2].Groups[4].Value -and $buys[0].Groups[2].Value -ne $buys[3].Groups[2].Value -and $buys[3].Groups[6].Value -eq '0' -and $buys[3].Groups[7].Value -eq '10') 'Real shared Torch card cooldown/Pea+Ice ancestry missing.'
        $purchaseSets+=,@($buys)
        $move=[regex]::Match($text,('FIRE_TORCH_MOVE role='+$role+' id=(\d+) token=(\d+) owner=(\d+) packet=(\d+) col=5 HP=300 sameInstance=1 heldOriginalActive=1'))
        Assert-Fire ($move.Success -and $move.Groups[1].Value -eq $buys[1].Groups[2].Value -and $move.Groups[2].Value -eq $buys[1].Groups[3].Value -and $move.Groups[3].Value -eq '1' -and $move.Groups[4].Value -eq $buys[1].Groups[4].Value) 'Actual SDL moved-Torch identity/provenance proof missing.';$moveSets+=$move
        Assert-Fire ($text -match ('FIRE_COMPLETE role='+$role+' source18=1 realPurchases=4 initialSun=525 totalCost=525 finalSun=0 peaUses=1 torchUses=2 iceUses=1 hotCases=4 snowMeltCase=1 sameTorchMoved=1 nativeBoard=1 secondWorld=0 sunWritesAfterSetup=0 plantHPWrites=0 plantTimerWrites=0 sourceZombieAIUnported=1')) 'Full native fire conservation/coverage completion missing.'
        foreach ($m in [regex]::Matches($text,'FIRE_RENDER role=\w+ frame=\w+ freshDraws=(\d+)')) {Assert-Fire ([int]$m.Groups[1].Value -ge 2) 'Capture did not await fresh actual native draw.'}
    }
    for ($i=0;$i -lt 4;++$i) {for ($g=1;$g -le 8;++$g) {Assert-Fire ($purchaseSets[0][$i].Groups[$g].Value -eq $purchaseSets[1][$i].Groups[$g].Value) 'Canonical packet/owner/parent/actor identity differs between peers.'}}
    $torch1=$purchaseSets[0][1].Groups[2].Value;$token1=$purchaseSets[0][1].Groups[3].Value;$torch2=$purchaseSets[0][2].Groups[2].Value;$token2=$purchaseSets[0][2].Groups[3].Value
    foreach ($case in 0..4) {
        $n=@(0,2,1,4,1)[$case];$splash=@(0,13,13,10,13)[$case]
        $born=@([regex]::Matches($hostText,('FIRE_BORN case='+$case+' id=(\d+) source=(\d+) age=(\d+) tick=(\d+) x=([0-9.eE+\-]+) nativeNaturalFire=1 initialKind=(\d+)')))
        $yellow=@([regex]::Matches($hostText,('FIRE_YELLOW case='+$case+' id=(\d+) age=(\d+) tick=(\d+) kind=0 damage=40 fire=1 stamp=(\d+) token=(\d+) vx=([0-9.eE+\-]+) sameIdentity=([01]) ageTrajectoryPreserved=1')))
        Assert-Fire ($born.Count -eq 1 -and $yellow.Count -eq 1) 'Exactly one naturally acquired projectile per controlled fire cohort is required.'
        $born=$born[0];$yellow=$yellow[0];$id=$yellow.Groups[1].Value
        $speed=[single]::Parse($yellow.Groups[6].Value,[Globalization.CultureInfo]::InvariantCulture);$expectedSpeed=if ($case -eq 4) {[single]3.2} else {[single]3.33}
        Assert-Fire ([Math]::Abs($speed-$expectedSpeed) -lt 0.0001 -and $hostText -match ('FIRE_SECOND case='+$case+' id='+$id+' secondTorch='+$torch2+' damage=40 fire=1 nonstack=1')) 'Base/timed velocity or second-Torch nonstacking proof missing.'
        if ($case -lt 4) {
            Assert-Fire ($born.Groups[1].Value -eq $id -and $born.Groups[2].Value -eq '0' -and $born.Groups[6].Value -eq '0' -and $yellow.Groups[4].Value -eq $torch1 -and $yellow.Groups[5].Value -eq $token1 -and $yellow.Groups[7].Value -eq '1' -and
                [int]$yellow.Groups[2].Value-[int]$born.Groups[3].Value -eq [int]$yellow.Groups[3].Value-[int]$born.Groups[4].Value) 'Yellow conversion changed native ID/age instead of mutating the same Pea.'
            Assert-Fire ($clientText -match ('FIRE_SEEN case='+$case+' id='+$id+' kind=0 damage=20 fire=0 stamp=0 type=0 hostAgeNotSerialized=1')) 'Client missed the original unheated native Pea.'
        } else {
            Assert-Fire ($born.Groups[2].Value -eq '1034' -and $born.Groups[6].Value -eq '15' -and $id -ne $born.Groups[1].Value -and $yellow.Groups[4].Value -eq $torch2 -and $yellow.Groups[5].Value -eq $token2 -and $yellow.Groups[7].Value -eq '0') 'Snow melt did not create an ordinary fresh projectile before its later heat.'
            $melt=[regex]::Match($hostText,('FIRE_MELT old='+$born.Groups[1].Value+' fresh='+$id+' oldObservedAge=(\d+) freshAge=(\d+) torch='+$torch1+' token='+$token1+' damage=20 fire=0 vx=3.2 copiedTrajectory=1 newIdentity=1'))
            Assert-Fire ($melt.Success -and [int]$melt.Groups[2].Value -gt 0 -and [int]$melt.Groups[2].Value -lt [int]$melt.Groups[1].Value -and
                $hostText -match ('Fusion native Torchwood melt torch='+$torch1+' token='+$token1+' old='+$born.Groups[1].Value+' new='+$id+' damage=20 age=0 motion=0 special=0')) 'Actual new native actor/fresh age0 source factory proof missing.'
            Assert-Fire ($hostText -match ('FIRE_SAME_TORCH id='+$id+' torch='+$torch1+' token='+$token1+' movedCol=5 actualOverlap=1 kind=0 damage=20 fire=0 sameStampIgnored=1')) 'Moved same Torch reheated or never overlapped its stamped replacement.'
            Assert-Fire ($clientText -match ('FIRE_SEEN case=4 id='+$born.Groups[1].Value+' kind=15 damage=20 fire=0 stamp=0 type=0 hostAgeNotSerialized=1') -and
                $clientText -match ('FIRE_SEEN case=4 id='+$id+' kind=0 damage=20 fire=0 stamp='+$torch1+' type=0 hostAgeNotSerialized=1')) 'Client missed canonical snow->new unheated actor/stamp projection.'
        }
        Assert-Fire ($clientText -match ('FIRE_SEEN case='+$case+' id='+$id+' kind=0 damage=40 fire=1 stamp='+$yellow.Groups[4].Value+' type=6 hostAgeNotSerialized=1')) 'Client hot native FIREBALL/metadata projection missing.'
        Assert-Fire ([regex]::Matches($hostText,('Fusion native Torchwood yellow torch=\d+ token=\d+ projectile='+$id+' damage=40 ')).Count -eq 1) 'Two ordinary Torches stacked/reconverted a hot actor.'
        $impact=@([regex]::Matches($hostText,('Fusion native hot impact projectile='+$id+' primary=(\d+) damage=40 secondaryCount='+$n+' secondaryDamage='+$splash+' shieldAfter=(\d+)')))
        Assert-Fire ($impact.Count -eq 1 -and $(if ($case -eq 0) {[int]$impact[0].Groups[2].Value -ne 0} else {[int]$impact[0].Groups[2].Value -eq 0})) 'Actual post-primary shield eligibility/N1,N2,N4 distributed splash differs.'
        $outcomes=@()
        foreach ($role in @('host','client')) {
            $text=if ($role -eq 'host') {$hostText} else {$clientText}
            Assert-Fire ($text -match ('FIRE_HIT role='+$role+' case='+$case+' projectile='+$id+' primary='+$impact[0].Groups[1].Value+' damage=40 secondaryCount='+$n+' secondaryDamage='+$splash+' shieldAfter=\d+ warmThreshold200=[01] noFrozenDamageMultiplier=1 noFourthOutside=1 adjacentUnchanged=1') -and
                $text -match ('FIRE_CASE_DONE role='+$role+' case='+$case+' projectile='+$id+' hot40=1 N='+$n+' secondaryDamage='+$splash+' noSun=1 nativeBoard=1')) 'One common native fire cohort incomplete.'
            $rows=@([regex]::Matches($text,('FIRE_OUTCOME role='+$role+' case='+$case+' index=(\d+) id=(\d+) body=(\d+) shield=(\d+) level=(\d+) threshold=(\d+) slow=(\d+) freeze=(\d+)')))
            Assert-Fire ($rows.Count -eq 7) 'Seven canonical primary/secondary/outside/adjacent controls missing.';$outcomes+=,@($rows)
            for ($i=0;$i -lt 7;++$i) {
                $expectedBody=if ($i -eq 0) {if ($case -eq 0) {5000} elseif ($case -eq 1) {4980} else {4960}} elseif ($i -le $n) {5000-$splash} else {5000}
                $expectedShield=if ($i -eq 0 -and $case -eq 0) {40} else {0}
                Assert-Fire ([int]$rows[$i].Groups[3].Value -eq $expectedBody -and [int]$rows[$i].Groups[4].Value -eq $expectedShield) 'Actual hot direct/secondary shield/body damage vector differs.'
                if ($case -lt 4 -and ($i -eq 0 -or $i -le $n)) {Assert-Fire ($rows[$i].Groups[5].Value -eq '0' -and $rows[$i].Groups[6].Value -eq '200' -and $rows[$i].Groups[7].Value -eq '0' -and $rows[$i].Groups[8].Value -eq '0') 'Warm did not clear real/source cold while retaining threshold200.'}
            }
        }
        for ($i=0;$i -lt 7;++$i) {for ($g=1;$g -le 4;++$g) {Assert-Fire ($outcomes[0][$i].Groups[$g].Value -eq $outcomes[1][$i].Groups[$g].Value) 'Canonical peer control IDs/HP/armor differs.'}}
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED);$fireCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_FUSION_FIRE_CASE_DONE role=host case=4');$ci=$clientText.IndexOf('SMOKE_FUSION_FIRE_CASE_DONE role=client case=4');Assert-Fire ($hi -ge 0 -and $ci -ge 0) 'Final common native fire barrier missing.'
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$apPost=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED);$firePost=@($apPost|Where-Object {$txPost -contains $_}|Select-Object -Unique)
    Assert-Fire ($fireCommon.Count -ge 8 -and $firePost.Count -ge 2) 'Insufficient ordinary transport common/postaction snapshots.'
    $fireFrames=@((Assert-FireFrames $fireHost host $fireHostWidth),(Assert-FireFrames $fireClient client $fireClientWidth));Assert-Fire ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $fireExeSha) 'Executable changed during owned run.';$firePassed=$true
} catch {$fireError=$_.Exception.Message;if ($_.ScriptStackTrace) {$fireError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Fire: '+$fireError)}
finally {
    foreach ($name in $fireEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$fireEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $fireClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $fireHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$firePassed=$false;$fireError+=' Owned cleanup failed.'}
    Ensure-Directory $fireRoot;$summary=[ordered]@{status=$(if ($firePassed) {'pass'} else {'fail'});error=$fireError;fixture='FusionNativeTorchwoodFire';source_kind=18;
        run_id=$fireRunId;started_utc=$fireStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$fireExeSha;common_snapshot_hashes=$fireCommon;postaction_common_hashes=$firePost;
        frames=$fireFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$fireRoot;campaign_initial_sun=500;fixture_initial_sun=525;purchase_total=525;final_sun=0;sun_writes_after_setup=0;native_board=$true;second_world=$false;
        limits='Controlled stationary native NORMAL actors with one explicit initial cold30/threshold200/slow1400/freeze400 setup, not earned source progression or source zombie AI. Runtime covers Pea0/Snow15 and ordinary Torch18; nonconvertible Cherry1/Puff10/sun8/rolling2/IceSpark18, hot rear, advanced Torch, Travel/buffs and multi-collider families remain pure-rule/deferred coverage. No late-join or heard-audio claim.';
        visual_review='Root reviews real client native frames; nonblank BMP is not full visual acceptance'}
    Write-Utf8NoBom -Path (Join-Path $fireRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($firePassed) {exit 0} else {exit 1}
