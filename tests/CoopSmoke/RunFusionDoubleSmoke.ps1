#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateSet(1030,1008)][int]$FusionKind=1030,
    [ValidateRange(60,3600)][int]$TimeoutSeconds=180,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$doubleHostWidth=$HostWindowWidth; $doubleClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $doubleHostWidth -ClientWindowWidth $doubleClientWidth
if ($HostWindowWidth -ne $doubleHostWidth -or $ClientWindowWidth -ne $doubleClientWidth) { throw 'Shared helper changed requested widths.' }
$doubleRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $doubleRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $doubleGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $doubleRepo 'work\fusion-double' }
$doubleRunId=[Guid]::NewGuid().ToString('N'); $doubleRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$doubleRunId)
$doubleHost=Join-Path $doubleRoot 'host'; $doubleClient=Join-Path $doubleRoot 'client'
$doubleHostLog=Join-Path $doubleHost 'logs'; $doubleClientLog=Join-Path $doubleClient 'logs'
$doubleHostProcess=$null; $doubleClientProcess=$null; $doublePassed=$false; $doubleError=''; $doubleFrames=@(); $doubleCommon=@(); $doublePost=@(); $doubleExeSha=''
$doubleStarted=[DateTime]::UtcNow; $doubleEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH',
    'PVZ_COOP_SMOKE_FUSION_CHERRY','PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER')) {
    $doubleEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-DoubleFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $names=@('chooser','picked','intro','playing','intermediate','fused','projectile','combat','complete')
    if ($Role -eq 'client') { $names+='held' }
    foreach ($name in $names) {
        $path=Join-Path $Directory ('fusion_double_'+$name+'.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual renderer frame: '+$path) }
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $Width -or $bitmap.Height -ne 600) { throw 'Actual viewport dimensions mismatch.' }
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            if ($colors.Count -lt 8) { throw ('Blank renderer frame: '+$path) }
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $doubleGame 'main.pak'),(Join-Path $doubleGame 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Runtime input missing: '+$path) }
    }
    $doubleExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($doubleHostLog,$doubleClientLog,(Join-Path $doubleHost 'appdata'),(Join-Path $doubleClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $doubleEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_DOUBLE='1'; $env:PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND=[string]$FusionKind
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$doubleHost} else {$doubleClient}
        $width=if ($role -eq 'host') {$doubleHostWidth} else {$doubleClientWidth}
        $nickname=if ($role -eq 'host') {'DoubleHost'} else {'DoubleClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native double '+$FusionKind+': two hidden owned windows; '+$doubleRoot)
    $doubleHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $doubleGame -ManifestPath (Join-Path $doubleHost 'smoke.ini') -Role host -RunId $doubleRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $doubleRoot 'pids.json') -Executable $GameExe -HostProcess $doubleHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $doubleHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $doubleHostProcess -Role host -TimeoutSeconds 120
    $doubleClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $doubleGame -ManifestPath (Join-Path $doubleClient 'smoke.ini') -Role client -RunId $doubleRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $doubleRoot 'pids.json') -Executable $GameExe -HostProcess $doubleHostProcess -ClientProcess $doubleClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $doubleHost 'result.json') -ClientResultPath (Join-Path $doubleClient 'result.json') `
        -HostProcess $doubleHostProcess -ClientProcess $doubleClientProcess -HostLogDirectory $doubleHostLog -ClientLogDirectory $doubleClientLog -TimeoutSeconds $TimeoutSeconds)
    if ($results.Count -ne 2 -or $results[0].role -ne 'host' -or $results[1].role -ne 'client' -or $results[0].protocol -ne $results[1].protocol) { throw 'Peer identity/protocol mismatch.' }
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        if ($result.status -ne 'pass' -or $result.mode -ne 'fusion_validation' -or $result.scenario -ne 'fusion_validation' -or
            [int]$result.level -ne 1 -or [uint32]$result.seed -ne $Seed -or [bool]$result.skipIntro -or $samples -lt 8) { throw 'Not a strict full-intro native manifest run.' }
    }
    $hostText=Get-CombinedLogText $doubleHostLog; $clientText=Get-CombinedLogText $doubleClientLog
    $sun=if ($FusionKind -eq 1030) {200} else {50}; $damage=if ($FusionKind -eq 1030) {20} else {60}
    $bullet=if ($FusionKind -eq 1030) {0} else {1}; $cd=if ($FusionKind -eq 1030) {750} else {5000}
    $owner=if ($FusionKind -eq 1030) {1} else {0}; $parents=if ($FusionKind -eq 1030) {'0,0'} else {'1030,2'}
    $purchases=@(); $intermediates=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        if ($text -match 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') { throw ('Fixture/runtime failure: '+$role) }
        if ($text -notmatch ('DOUBLE_PLAYING role='+$role+' kind='+$FusionKind+'[^\r\n]*sun=500 nativeBoard=1 secondWorld=0') -or
            $text -notmatch ('DOUBLE_READY role='+$role+' nativeSDL=1 hoverObserved=1')) { throw 'Native chooser/Ready/full-intro proof missing.' }
        $pool=[regex]::Match($text,('DOUBLE_POOL role='+$role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+)[^\r\n]*unfiltered=1 nativeChooser=1'))
        if (-not $pool.Success -or [int]$pool.Groups[1].Value -lt 16 -or [int]$pool.Groups[2].Value -ne [Math]::Min(10,[int]$pool.Groups[1].Value) -or
            [int]$pool.Groups[3].Value+[int]$pool.Groups[4].Value -ne [int]$pool.Groups[2].Value) { throw 'Real eligible pool/capacity/quota mismatch.' }
        if ($text -notmatch ('DOUBLE_PARENT role='+$role+'[^\r\n]*owner=0 price=100 sun=400 sourceCD=750 nativeSDL=1')) { throw 'Actual ordinary Pea parent purchase missing.' }
        $intermediate=[regex]::Match($text,('DOUBLE_INTERMEDIATE role='+$role+' old=(\d+) id=(\d+) packet=(\d+) owner=1 extra=1 parents=0,0 price=200 sun=200 sourceCD=750 bothPeerAck=1'))
        if (-not $intermediate.Success -or $intermediate.Groups[1].Value -eq $intermediate.Groups[2].Value) { throw 'Both-peer actual1030 intermediate proof missing.' }; $intermediates+=$intermediate
        $purchase=[regex]::Match($text,('DOUBLE_PURCHASE role='+$role+' kind='+$FusionKind+' parent=(\d+) intermediate=(\d+) final=(\d+) packet=(\d+) owner='+$owner+' parents='+$parents+' sun='+$sun+' sourceCD='+$cd+' noTemporaryCherry=1 nativeSDL=1'))
        if (-not $purchase.Success -or $purchase.Groups[2].Value -ne $intermediate.Groups[2].Value -or
            (($FusionKind -eq 1030) -ne ($purchase.Groups[2].Value -eq $purchase.Groups[3].Value))) { throw 'Actual final recipe/owner/currency/recharge proof missing.' }; $purchases+=$purchase
        if ($text -notmatch ('DOUBLE_DAMAGE role='+$role+' kind='+$FusionKind+' hits=4 front='+(5000-4*$damage)+' rear=5000 adjacent=5000 damage='+$damage+' noAoE=1 noSun=1 noPierce=1 sun='+$sun+' healthWrites=0') -or
            $text -notmatch ('DOUBLE_COMPLETE role='+$role+' kind='+$FusionKind+'[^\r\n]*hits=4 damage='+$damage+'[^\r\n]*sun='+$sun+' sourceCD='+$cd+' twoEventsPerVolley=1 volleys=2 nativeBoard=1 secondWorld=0 healthWritesAfterSetup=0 timerWrites=0')) { throw 'Natural four-hit/shared-state completion missing.' }
        foreach ($m in [regex]::Matches($text,'DOUBLE_RENDER role=\w+ frame=\w+ freshDraws=(\d+)')) { if ([int]$m.Groups[1].Value -lt 2) { throw 'Capture did not await actual renderer frames.' } }
    }
    for ($i=1;$i -le 4;++$i) { if ($purchases[0].Groups[$i].Value -ne $purchases[1].Groups[$i].Value) { throw 'Peer plant/packet IDs differ.' } }
    if ($clientText -notmatch 'DOUBLE_HELD role=client[^\r\n]*extra=1 price=200 sun=400 snapshotEcho=1 nativeSDL=1' -or
        [regex]::Matches($hostText,'DOUBLE_SETUP index=\d+ id=\d+ row=\d+ body=5000 helm=0 shield=0 staged=1').Count -ne 3 -or
        $hostText -notmatch 'DOUBLE_PARK role=host afterHits=4 positionOnly=1 countersUnchanged=1 preventFurtherAcquisition=1') { throw 'Client pickup echo/real target setup/bounded combat tail missing.' }
    $triggers=@([regex]::Matches($hostText,('DOUBLE_TRIGGER role=host kind='+$FusionKind+' volley=(\d+) tick=(\d+) counter=(\d+) initial=(\d+) first=(\d+) second=(\d+) speed=([0-9.eE+\-]+) nominalDeadlines=27,54 nativeClock=1 timerWrites=0')))
    $shots=@([regex]::Matches($hostText,('DOUBLE_PROJECTILE role=host kind='+$FusionKind+' id=(\d+) volley=(\d+) event=(\d+) fireTick=(\d+) triggerTick=(\d+) deadline=(\d+) age=(\d+) bullet='+$bullet+' damage='+$damage+' sun=0 vx=3.2 nativeActor=1')))
    if ($triggers.Count -ne 2 -or $shots.Count -ne 4 -or @($shots | ForEach-Object {$_.Groups[1].Value} | Select-Object -Unique).Count -ne 4) { throw 'Expected two real two-event volleys/four unique projectiles.' }
    for ($v=1;$v -le 2;++$v) {
        $trigger=@($triggers | Where-Object {[int]$_.Groups[1].Value -eq $v}); if ($trigger.Count -ne 1) { throw 'Duplicate/missing native trigger.' }; $trigger=$trigger[0]
        $speed=[single]::Parse($trigger.Groups[7].Value,[Globalization.CultureInfo]::InvariantCulture)
        if ([single]::IsNaN($speed) -or [single]::IsInfinity($speed) -or $speed -lt [single]0.9 -or $speed -gt [single]1.1) { throw 'Invalid source animation speed.' }
        $deadlines=@([int][Math]::Ceiling([double]([single]0.53333336)/(2*[double]$speed)*100),[int][Math]::Ceiling([double]([single]1.0666667)/(2*[double]$speed)*100))
        if ([int]$trigger.Groups[5].Value -ne $deadlines[0] -or [int]$trigger.Groups[6].Value -ne $deadlines[1] -or [int]$trigger.Groups[4].Value -ne $deadlines[1]+1) { throw 'Trigger does not match independent source float32 deadlines.' }
        for ($e=1;$e -le 2;++$e) {
            $shot=@($shots | Where-Object {[int]$_.Groups[2].Value -eq $v -and [int]$_.Groups[3].Value -eq $e})
            if ($shot.Count -ne 1 -or [int]$shot[0].Groups[5].Value -ne [int]$trigger.Groups[2].Value -or [int]$shot[0].Groups[6].Value -ne $deadlines[$e-1] -or
                [int]$shot[0].Groups[4].Value -ne [int]$trigger.Groups[2].Value+$deadlines[$e-1]) { throw 'Actual native projectile fire tick is early/late/duplicated.' }
        }
    }
    for ($hit=1;$hit -le 4;++$hit) { if ($hostText -notmatch ('DOUBLE_DAMAGE role=host kind='+$FusionKind+' hits='+$hit+' front='+(5000-$hit*$damage)+' rear=5000 adjacent=5000')) { throw 'Individual host damage transition missing.' } }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX); $ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $doubleCommon=@($ap | Where-Object {$tx -contains $_} | Select-Object -Unique)
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hostText.IndexOf('SMOKE_FUSION_DOUBLE_DAMAGE role=host kind='+$FusionKind+' hits=4')) -Marker SMOKE_SNAPSHOT_TX)
    $apPost=@(Get-SnapshotHashes -Text $clientText.Substring($clientText.IndexOf('SMOKE_FUSION_DOUBLE_DAMAGE role=client kind='+$FusionKind+' hits=4')) -Marker SMOKE_SNAPSHOT_APPLIED)
    $doublePost=@($apPost | Where-Object {$txPost -contains $_} | Select-Object -Unique)
    if ($doubleCommon.Count -lt 8 -or $doublePost.Count -lt 2) { throw 'Insufficient actual common/post-combat snapshot hashes.' }
    $doubleFrames=@((Assert-DoubleFrames $doubleHost host $doubleHostWidth),(Assert-DoubleFrames $doubleClient client $doubleClientWidth))
    if ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -ne $doubleExeSha) { throw 'Executable changed during owned run.' }
    $doublePassed=$true
} catch { $doubleError=$_.Exception.Message; if ($_.ScriptStackTrace) {$doubleError+=' | '+$_.ScriptStackTrace}; Write-SmokeStatus ('FAIL double: '+$doubleError) }
finally {
    foreach ($name in $doubleEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$doubleEnvironment[$name],'Process') }
    $clientStopped=Stop-OwnedProcess -Process $doubleClientProcess -Role client; $hostStopped=Stop-OwnedProcess -Process $doubleHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$doublePassed=$false;$doubleError+=' Owned cleanup failed.'}
    Ensure-Directory $doubleRoot
    $summary=[ordered]@{status=$(if ($doublePassed) {'pass'} else {'fail'});error=$doubleError;fixture='FusionNativeDouble';source_kind=$FusionKind;
        run_id=$doubleRunId;started_utc=$doubleStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$doubleExeSha;
        common_snapshot_hashes=$doubleCommon;post_combat_common_hashes=$doublePost;frames=$doubleFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);
        native_board=$true;second_world=$false;artifacts_root=$doubleRoot;visual_review='Required by Root; not inferred from nonblank BMP'}
    Write-Utf8NoBom -Path (Join-Path $doubleRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($doublePassed) {exit 0} else {exit 1}
