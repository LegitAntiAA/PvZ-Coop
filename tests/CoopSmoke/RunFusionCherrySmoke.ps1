#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateRange(60,3600)][int]$TimeoutSeconds = 180,
    [ValidateRange(0,65535)][int]$Port = 0,
    [ValidateSet(800,1066)][int]$HostWindowWidth = 1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth = 1066,
    [uint32]$Seed = 424242
)

$ErrorActionPreference = 'Stop'
$cherryRequestedHostWidth = $HostWindowWidth
$cherryRequestedClientWidth = $ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed `
    -HostWindowWidth $cherryRequestedHostWidth -ClientWindowWidth $cherryRequestedClientWidth
if ($HostWindowWidth -ne $cherryRequestedHostWidth -or $ClientWindowWidth -ne $cherryRequestedClientWidth) {
    throw 'Shared smoke helper changed the requested Cherry window widths.'
}
$cherryRepo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe = Join-Path $cherryRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$cherryGameDirectory = Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot = Join-Path $cherryRepo 'work\fusion-cherry' }
$cherryRunId = [Guid]::NewGuid().ToString('N')
$cherryRunRoot = Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_' + $cherryRunId)
$cherryHostRoot = Join-Path $cherryRunRoot 'host'
$cherryClientRoot = Join-Path $cherryRunRoot 'client'
$cherryHostLog = Join-Path $cherryHostRoot 'logs'
$cherryClientLog = Join-Path $cherryClientRoot 'logs'
$cherryHostProcess = $null; $cherryClientProcess = $null
$cherryPassed = $false; $cherryError = ''; $cherryCommon = @(); $cherryPostPurchase = @(); $cherryFrames = @()
$cherryStarted = [DateTime]::UtcNow
$cherryEnvironment = @{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_CHERRY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES',
    'PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER')) {
    $cherryEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}

function Assert-CherryFrames {
    param([string]$Directory,[string]$Role,[int]$Width)
    Add-Type -AssemblyName System.Drawing
    $names = @('chooser','picked','intro','targets','planted','after','complete')
    if ($Role -eq 'client') { $names += 'held' }
    foreach ($name in $names) {
        $path = Join-Path $Directory ('fusion_cherry_' + $name + '.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual Cherry native frame: ' + $path) }
        $bitmap = [Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $Width -or $bitmap.Height -ne 600) { throw 'Cherry native renderer dimensions do not match the requested window.' }
            $colors = New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) {
                for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) }
            }
            if ($colors.Count -lt 8) { throw ('Blank/flat Cherry native frame: ' + $path) }
        } finally { $bitmap.Dispose() }
        [ordered]@{ role=$Role; frame=$name; path=$path; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    }
}

try {
    foreach ($path in @($GameExe,(Join-Path $cherryGameDirectory 'main.pak'),(Join-Path $cherryGameDirectory 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Cherry runtime input missing: ' + $path) }
    }
    foreach ($directory in @($cherryHostLog,$cherryClientLog,(Join-Path $cherryHostRoot 'appdata'),(Join-Path $cherryClientRoot 'appdata'))) { Ensure-Directory $directory }
    if ($Port -eq 0) { $Port = Get-FreeLoopbackPort }
    foreach ($name in $cherryEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE = '1'; $env:PVZ_COOP_SMOKE_FUSION_CHERRY = '1'
    foreach ($role in @('host','client')) {
        $root = if ($role -eq 'host') { $cherryHostRoot } else { $cherryClientRoot }
        $width = if ($role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth }
        $nickname = if ($role -eq 'host') { 'CherryHost' } else { 'CherryClient' }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') `
            -ResultPath (Join-Path $root 'result.json') -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native Cherry: source-normal2 / co-op10, two owned windows; ' + $cherryRunRoot)
    $cherryHostProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $cherryGameDirectory `
        -ManifestPath (Join-Path $cherryHostRoot 'smoke.ini') -Role host -RunId $cherryRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $cherryRunRoot 'pids.json') -Executable $GameExe -HostProcess $cherryHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $cherryHostLog -Pattern ('(?im)host listening port=' + $Port + '\b') -Process $cherryHostProcess -Role host -TimeoutSeconds 120
    $cherryClientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $cherryGameDirectory `
        -ManifestPath (Join-Path $cherryClientRoot 'smoke.ini') -Role client -RunId $cherryRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $cherryRunRoot 'pids.json') -Executable $GameExe -HostProcess $cherryHostProcess -ClientProcess $cherryClientProcess
    $results = @(Wait-ForResults -HostResultPath (Join-Path $cherryHostRoot 'result.json') -ClientResultPath (Join-Path $cherryClientRoot 'result.json') `
        -HostProcess $cherryHostProcess -ClientProcess $cherryClientProcess -HostLogDirectory $cherryHostLog -ClientLogDirectory $cherryClientLog -TimeoutSeconds $TimeoutSeconds)
    foreach ($result in $results) {
        if ([string]$result.status -ne 'pass' -or [string]$result.mode -ne 'fusion_validation' -or [string]$result.scenario -ne 'fusion_validation' -or
            [int]$result.level -ne 1 -or [uint32]$result.seed -ne $Seed -or [bool]$result.skipIntro) { throw 'Cherry result is not the strict native manifest run.' }
        $samples = if ([string]$result.role -eq 'host') { [int]$result.snapshotsSent } else { [int]$result.snapshotsApplied }
        if ($samples -lt 8) { throw 'Cherry peer has fewer than eight actual native snapshots.' }
    }
    if ($results.Count -ne 2 -or $results[0].role -ne 'host' -or $results[1].role -ne 'client' -or $results[0].protocol -ne $results[1].protocol) { throw 'Cherry peer result/protocol identity mismatch.' }
    $hostText = Get-CombinedLogText $cherryHostLog; $clientText = Get-CombinedLogText $cherryClientLog
    foreach ($role in @('host','client')) {
        $text = if ($role -eq 'host') { $hostText } else { $clientText }
        if ($text -match 'SMOKE_FAIL|SMOKE_STOPPED|rejected|seq-gap|invalid snapshot|fatal|secondWorld=1') { throw ('Cherry run contains a rejection/failure: ' + $role) }
        if ($text -notmatch ('CHERRY_PLAYING role=' + $role + '[^\r\n]*coopDifficulty=10 sourceDifficulty=2 nativeBoard=1 secondWorld=0') -or
            $text -notmatch ('CHERRY_COMPLETE role=' + $role + '[^\r\n]*sun=200 sourceCD=5000 cases=8 nativeBoard=1 secondWorld=0 collider=nativeRectAdaptation sourceCampaignComplete=0')) { throw 'Cherry source/coop difficulty or same-world completion proof missing.' }
        $draw = [regex]::Match($text,('CHERRY_PLANTED_RENDER role=' + $role + ' firstDraw=(\d+) currentDraw=(\d+) freshDraws=(\d+)'))
        if (-not $draw.Success -or [int]$draw.Groups[3].Value -lt 2 -or
            [int]$draw.Groups[2].Value-[int]$draw.Groups[1].Value -ne [int]$draw.Groups[3].Value) { throw ('Cherry planted capture did not wait for two actual renderer frames: ' + $role) }
        foreach ($case in @('inside','shield')) {
            if ($text -notmatch ('CHERRY_ARMOR role=' + $role + ' after=0 case=' + $case + ' id=\d+ body=1 helmetGroup=1 helmetForceHidden=0 shieldGroup=3 shieldForceHidden=0 imageAndTrackMatch=1 expectedDoorImageStage=0')) {
                throw ('Cherry initial native armor tracks/images are not visible: ' + $role + '/' + $case)
            }
        }
        if ($text -notmatch ('CHERRY_ARMOR role=' + $role + ' after=1 case=shield id=\d+ body=1 helmetGroup=-1 helmetForceHidden=[01] shieldGroup=3 shieldForceHidden=0 imageAndTrackMatch=1 expectedDoorImageStage=1')) {
            throw ('Cherry native helmet loss/door damage-stage rendering is missing: ' + $role)
        }
    }
    if ($hostText -notmatch 'CHERRY_ARMOR_INDEX current=1200000000 maximum=2000000000 helmet=1 shield=1 actualNativeGetters=1 restoredBeforeSnapshot=1' -or
        $hostText -notmatch 'CHERRY_ARMOR_INDEX current=2147483647 maximum=2147483647 helmet=0 shield=0 actualNativeGetters=1 restoredBeforeSnapshot=1') {
        throw 'Real native armor getters failed the valid large-health regression vectors.'
    }
    $freeze = [regex]::Matches($hostText,'CHERRY_FREEZE case=(\w+) id=\d+ body=1 nativeWalkingRate=([0-9.]+) initialOriginalRate=[0-9.]+ originalRate=([0-9.]+) liveRate=0\.000000 velocity=0\.000000 rateSource=nativeWalking fixtureControl=1')
    if ($freeze.Count -ne 8 -or @($freeze | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique).Count -ne 8) { throw 'Cherry controls did not preserve eight actual native walking rates before freezing.' }
    foreach ($entry in $freeze) {
        $nativeRate=[double]::Parse($entry.Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture)
        $originalRate=[double]::Parse($entry.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture)
        if ($nativeRate -le 0 -or $nativeRate -ne $originalRate) { throw 'Cherry freeze changed or invented the native original animation rate.' }
    }
    $purchasePattern = 'CHERRY_PURCHASE role=\w+ plant=(\d+) packet=(\d+) kind=2 owner=1 extra=1 column=9 row=2 price=300 sun=200 sourceCD=5000 nativeActor=1 nativeSDL=1'
    $hostPurchase = [regex]::Match($hostText,$purchasePattern); $clientPurchase = [regex]::Match($clientText,$purchasePattern)
    if (-not $hostPurchase.Success -or -not $clientPurchase.Success -or $hostPurchase.Groups[1].Value -ne $clientPurchase.Groups[1].Value -or
        $hostPurchase.Groups[2].Value -ne $clientPurchase.Groups[2].Value -or $clientText -notmatch 'CHERRY_HELD role=client[^\r\n]*charge=0 cooldown=0 snapshotEcho=1 nativeSDL=1') { throw 'Cherry purchase/held identity or no-premature-charge proof missing.' }
    $fuse = [regex]::Match($hostText,'CHERRY_FUSE role=host initial=(\d+) observedTicks=(\d+) totalTicks=100 clock=nativeBoard')
    if (-not $fuse.Success -or [int]$fuse.Groups[1].Value -lt 1 -or [int]$fuse.Groups[1].Value -gt 100 -or
        [Math]::Abs([int]$fuse.Groups[1].Value-[int]$fuse.Groups[2].Value) -gt 1 -or
        [regex]::Matches($hostText,'Fusion native Cherry explosion[^\r\n]*column=9 row=2 countdown=0 sourceDifficulty=2').Count -ne 1) { throw 'Cherry native100tick single-fire proof missing.' }
    $cases = [ordered]@{ inside=@(1,-1,-1,-1); outside=@(0,270,0,0); row_minus=@(1,-1,-1,-1); row_plus=@(1,-1,-1,-1);
        row_two_away=@(0,270,0,0); mindcontrol=@(0,270,0,0); highbody=@(0,3200,0,0); shield=@(0,1200,0,2200) }
    foreach ($case in $cases.Keys) {
        $pattern = 'CHERRY_RESULT role=\w+ case=' + $case + ' id=(\d+) dead=([01]) body=(\d+) first=(\d+) second=(\d+) mind=([01]) firstType=(\d+) secondType=(\d+) nativeActor=1 sourceDifficulty=2'
        $a=[regex]::Match($hostText,$pattern); $b=[regex]::Match($clientText,$pattern); $expected=$cases[$case]
        if (-not $a.Success -or -not $b.Success -or $a.Groups[1].Value -ne $b.Groups[1].Value) { throw ('Cherry peer target identity missing: ' + $case) }
        foreach ($row in @($a,$b)) {
            if ([int]$row.Groups[2].Value -ne $expected[0]) { throw ('Cherry target death mismatch: ' + $case) }
            if (-not $expected[0]) {
                for ($i=1;$i -le 3;++$i) { if ([int]$row.Groups[$i+2].Value -ne $expected[$i]) { throw ('Cherry target armor/body mismatch: ' + $case) } }
            }
        }
    }
    if ([regex]::Matches($hostText,'Fusion native Cherry impact[^\r\n]*sourceDifficulty=2').Count -ne 5 -or
        [regex]::Matches($hostText,'Fusion native Cherry impact[^\r\n]*instantBurn=1 dead=1 sourceDifficulty=2').Count -ne 3 -or
        $hostText -notmatch 'Cherry impact[^\r\n]*body=2000 first=1000 second=4000 afterBody=1200 afterFirst=0 afterSecond=2200 instantBurn=0 dead=0' -or
        $clientText -notmatch 'presentation effect applied kind=particle' -or
        [regex]::Matches($clientText,'presentation effect applied kind=reanim').Count -lt 3) { throw 'Actual native Charred/armor mutation or client standalone effects proof missing.' }
    $ladderPattern='CHERRY_LADDER role=\w+ inside=(\d+) removed=1 outside=(\d+) retained=1 sourceCollider=1 nativeGridItems=1'
    $a=[regex]::Match($hostText,$ladderPattern);$b=[regex]::Match($clientText,$ladderPattern)
    if (-not $a.Success -or -not $b.Success -or $a.Groups[1].Value -ne $b.Groups[1].Value -or $a.Groups[2].Value -ne $b.Groups[2].Value) { throw 'Native ladder identities/outcomes differ.' }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$applied=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $cherryCommon=@($applied | Where-Object { $tx -contains $_ } | Select-Object -Unique)
    if ($cherryCommon.Count -lt 8) { throw 'Cherry has fewer than eight matching native snapshot hashes.' }
    $postTx=@(Get-SnapshotHashes -Text $hostText.Substring($hostText.IndexOf('SMOKE_FUSION_CHERRY_PURCHASE')) -Marker SMOKE_SNAPSHOT_TX)
    $postApplied=@(Get-SnapshotHashes -Text $clientText.Substring($clientText.IndexOf('SMOKE_FUSION_CHERRY_PURCHASE')) -Marker SMOKE_SNAPSHOT_APPLIED)
    $cherryPostPurchase=@($postApplied | Where-Object { $postTx -contains $_ } | Select-Object -Unique)
    if ($cherryPostPurchase.Count -lt 2) { throw 'Cherry has fewer than two shared post-purchase snapshot hashes.' }
    $cherryFrames=@((Assert-CherryFrames $cherryHostRoot host $HostWindowWidth),(Assert-CherryFrames $cherryClientRoot client $ClientWindowWidth))
    $cherryPassed=$true
} catch { $cherryError=$_.Exception.Message; if ($_.ScriptStackTrace) { $cherryError+=' | '+$_.ScriptStackTrace }; Write-SmokeStatus ('FAIL native Cherry: '+$cherryError) }
finally {
    foreach ($name in $cherryEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$cherryEnvironment[$name],'Process') }
    $clientStopped=Stop-OwnedProcess -Process $cherryClientProcess -Role client
    $hostStopped=Stop-OwnedProcess -Process $cherryHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) { $cherryPassed=$false;$cherryError+=' Owned process cleanup failed.' }
    Ensure-Directory $cherryRunRoot
    $summary=[ordered]@{ status=$(if ($cherryPassed) {'pass'} else {'fail'});error=$cherryError;fixture='FusionNativeCherry';run_id=$cherryRunId;
        started_utc=$cherryStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;
        coop_difficulty=10;source_difficulty=2;native_grid='10x5';source_kind=2;second_world=$false;source_campaign_complete=$false;
        collider='nativeRectAdaptation';common_snapshot_hashes=$cherryCommon;post_purchase_common_hashes=$cherryPostPurchase;
        frames=$cherryFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$cherryRunRoot }
    Write-Utf8NoBom -Path (Join-Path $cherryRunRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($cherryPassed) { exit 0 } else { exit 1 }
