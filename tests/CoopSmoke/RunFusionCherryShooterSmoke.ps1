#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateSet('PeaCherry','CherryPea')][string]$FusionDirection='PeaCherry',
    [ValidateRange(60,3600)][int]$TimeoutSeconds=180,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$csHostWidth=$HostWindowWidth; $csClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $csHostWidth -ClientWindowWidth $csClientWidth
if ($HostWindowWidth -ne $csHostWidth -or $ClientWindowWidth -ne $csClientWidth) { throw 'Shared helper changed requested widths.' }
$csRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $csRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $csGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $csRepo 'work\fusion-cherryshooter' }
$csRunId=[Guid]::NewGuid().ToString('N'); $csRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$csRunId)
$csHost=Join-Path $csRoot 'host'; $csClient=Join-Path $csRoot 'client'
$csHostLog=Join-Path $csHost 'logs'; $csClientLog=Join-Path $csClient 'logs'
$csHostProcess=$null; $csClientProcess=$null; $csPassed=$false; $csError=''; $csFrames=@(); $csCommon=@(); $csPost=@(); $csExeSha=''
$csStarted=[DateTime]::UtcNow; $csEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION',
    'PVZ_COOP_SMOKE_FUSION_CHERRY','PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER')) {
    $csEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-CsFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $names=@('chooser','picked','intro','fused','idle','targets','shoot','projectile','after','complete')
    if ($Role -eq 'client') { $names+='held' }
    foreach ($name in $names) {
        $path=Join-Path $Directory ('fusion_cs_'+$name+'.bmp')
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
    foreach ($path in @($GameExe,(Join-Path $csGame 'main.pak'),(Join-Path $csGame 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Runtime input missing: '+$path) }
    }
    $csExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($csHostLog,$csClientLog,(Join-Path $csHost 'appdata'),(Join-Path $csClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $csEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER='1'
    $env:PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION=$FusionDirection
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$csHost} else {$csClient}
        $width=if ($role -eq 'host') {$csHostWidth} else {$csClientWidth}
        $nickname=if ($role -eq 'host') {'CSHost'} else {'CSClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native CherryShooter '+$FusionDirection+': two hidden owned windows; '+$csRoot)
    $csHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $csGame -ManifestPath (Join-Path $csHost 'smoke.ini') -Role host -RunId $csRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $csRoot 'pids.json') -Executable $GameExe -HostProcess $csHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $csHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $csHostProcess -Role host -TimeoutSeconds 120
    $csClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $csGame -ManifestPath (Join-Path $csClient 'smoke.ini') -Role client -RunId $csRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $csRoot 'pids.json') -Executable $GameExe -HostProcess $csHostProcess -ClientProcess $csClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $csHost 'result.json') -ClientResultPath (Join-Path $csClient 'result.json') `
        -HostProcess $csHostProcess -ClientProcess $csClientProcess -HostLogDirectory $csHostLog -ClientLogDirectory $csClientLog -TimeoutSeconds $TimeoutSeconds)
    if ($results.Count -ne 2 -or $results[0].role -ne 'host' -or $results[1].role -ne 'client' -or $results[0].protocol -ne $results[1].protocol) { throw 'Peer identity/protocol mismatch.' }
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        if ($result.status -ne 'pass' -or $result.mode -ne 'fusion_validation' -or $result.scenario -ne 'fusion_validation' -or
            [int]$result.level -ne 1 -or [uint32]$result.seed -ne $Seed -or [bool]$result.skipIntro -or $samples -lt 8) { throw 'Not the strict full-intro native manifest run.' }
    }
    $hostText=Get-CombinedLogText $csHostLog; $clientText=Get-CombinedLogText $csClientLog
    $sun=if ($FusionDirection -eq 'PeaCherry') {100} else {150}; $cd=if ($FusionDirection -eq 'PeaCherry') {5000} else {750}
    $parents=if ($FusionDirection -eq 'PeaCherry') {'0,2'} else {'2,0'}
    $purchase=@()
    foreach ($role in @('host','client')) {
        $text=if ($role -eq 'host') {$hostText} else {$clientText}
        if ($text -match 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|fatal|secondWorld=1') { throw ('Fixture/runtime failure: '+$role) }
        if ($text -notmatch ('CS_PLAYING role='+$role+' direction='+$FusionDirection+'[^\r\n]*nativeBoard=1 secondWorld=0 sun=500') -or
            $text -notmatch ('CS_READY role='+$role+' hoverObserved=1 nativeSDL=1') -or $text -notmatch ('CS_POOL role='+$role+'[^\r\n]*unfiltered=1')) { throw 'Native chooser/Ready/full-intro proof missing.' }
        $p=[regex]::Match($text,('CS_PURCHASE role='+$role+' direction='+$FusionDirection+' old=(\d+) fused=(\d+) packet=(\d+) parents='+$parents+' owner=1 extra=1 hp=300 sun='+$sun+' sourceCD='+$cd+' parentElapsed=(\d+) parentFuse=(\d+) nativeSDL=1'))
        if (-not $p.Success) { throw ('Actual cross-peer purchase missing: '+$role) }; $purchase+=$p
        if ($role -eq 'host' -and $FusionDirection -eq 'CherryPea' -and ([int]$p.Groups[5].Value -le 0 -or [int]$p.Groups[4].Value -ge [int]$p.Groups[5].Value)) { throw 'Reverse fusion missed the real Cherry fuse.' }
        $complete=[regex]::Match($text,('CS_COMPLETE role='+$role+' direction='+$FusionDirection+'[^\r\n]*hits=(\d+)[^\r\n]*sun='+$sun+' sourceCD='+$cd+' staleNoCharge=1 nativeBoard=1 secondWorld=0'))
        if (-not $complete.Success -or [int]$complete.Groups[1].Value -lt 5) { throw ('Five-hit/shared-state completion missing: '+$role) }
        foreach ($m in [regex]::Matches($text,'CS_RENDER role=\w+ frame=\w+ firstDraw=(\d+) currentDraw=(\d+) freshDraws=(\d+)')) {
            if ([int]$m.Groups[3].Value -lt 2 -or [int]$m.Groups[2].Value-[int]$m.Groups[1].Value -ne [int]$m.Groups[3].Value) { throw 'Capture did not await actual renderer frames.' }
        }
    }
    for ($i=1;$i -le 3;++$i) { if ($purchase[0].Groups[$i].Value -ne $purchase[1].Groups[$i].Value) { throw 'Peer parent/fused/packet IDs differ.' } }
    if ($clientText -notmatch 'CS_HELD role=client[^\r\n]*snapshotEcho=1 parentNotPlaced=1 nativeSDL=1' -or
        $clientText -notmatch 'CS_STALE_SENT role=client[^\r\n]*ready=1 affordable=1 nativeAuthenticatedStream=1' -or
        $hostText -notmatch 'CS_EMPTY role=host cycle=2[^\r\n]*shots=0 forcedClock=0' -or
        [regex]::Matches($hostText,'CS_PROJECTILE id=\d+[^\r\n]*vx=3.2 damage=60 sun=0 nativeActor=1').Count -lt 5) { throw 'Held echo/empty cycles/stale identity/natural projectile proof missing.' }
    for ($hit=1;$hit -le 5;++$hit) { if ($hostText -notmatch ('CS_DAMAGE role=host hits='+$hit+'\b[^\r\n]*rear=5000 adjacent=5000 noAoE=1 noPierce=1')) { throw ('Natural armor/body transition missing: '+$hit) } }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX); $ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $csCommon=@($ap | Where-Object {$tx -contains $_} | Select-Object -Unique)
    $txPost=@(Get-SnapshotHashes -Text $hostText.Substring($hostText.IndexOf('SMOKE_FUSION_CS_PURCHASE')) -Marker SMOKE_SNAPSHOT_TX)
    $apPost=@(Get-SnapshotHashes -Text $clientText.Substring($clientText.IndexOf('SMOKE_FUSION_CS_PURCHASE')) -Marker SMOKE_SNAPSHOT_APPLIED)
    $csPost=@($apPost | Where-Object {$txPost -contains $_} | Select-Object -Unique)
    if ($csCommon.Count -lt 8 -or $csPost.Count -lt 2) { throw 'Insufficient actual common/post-purchase snapshot hashes.' }
    $csFrames=@((Assert-CsFrames $csHost host $csHostWidth),(Assert-CsFrames $csClient client $csClientWidth))
    if ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -ne $csExeSha) { throw 'Executable changed during owned run.' }
    $csPassed=$true
} catch { $csError=$_.Exception.Message; if ($_.ScriptStackTrace) {$csError+=' | '+$_.ScriptStackTrace}; Write-SmokeStatus ('FAIL CherryShooter: '+$csError) }
finally {
    foreach ($name in $csEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$csEnvironment[$name],'Process') }
    $clientStopped=Stop-OwnedProcess -Process $csClientProcess -Role client; $hostStopped=Stop-OwnedProcess -Process $csHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$csPassed=$false;$csError+=' Owned cleanup failed.'}
    Ensure-Directory $csRoot
    $summary=[ordered]@{status=$(if ($csPassed) {'pass'} else {'fail'});error=$csError;fixture='FusionNativeCherryShooter';direction=$FusionDirection;
        run_id=$csRunId;started_utc=$csStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$csExeSha;
        common_snapshot_hashes=$csCommon;post_purchase_common_hashes=$csPost;frames=$csFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);
        native_board=$true;second_world=$false;source_kind=1001;artifacts_root=$csRoot;visual_review='Required by Root; not inferred from nonblank BMP'}
    Write-Utf8NoBom -Path (Join-Path $csRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($csPassed) {exit 0} else {exit 1}
