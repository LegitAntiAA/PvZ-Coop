#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Cold','Shooter','Fire')][string]$Family = 'Shooter',
    [string]$GameExe = '', [string]$ArtifactsRoot = '',
    [ValidateRange(120,1200)][int]$TimeoutSeconds = 480,
    [uint32]$Seed = 424242
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Seed $Seed `
    -HostWindowWidth 1066 -ClientWindowWidth 800
$familyName=$Family.ToLowerInvariant(); $marker=$Family.ToUpperInvariant()+'40'
$repo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) {$GameExe=Join-Path $repo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe; $gameDirectory=Split-Path -Parent $GameExe
$guardPath=Join-Path $repo 'work\fusion\batch40-264\verified-build.json'
if (-not (Test-Path -LiteralPath $guardPath)) {throw 'Build guard missing: compile the integrated batch before running it.'}
$guard=Get-Content -LiteralPath $guardPath -Raw | ConvertFrom-Json
$exeHash=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
if ($guard.sha256 -ne $exeHash -or $guard.protocol -ne 50 -or $guard.fns -ne 15) {throw 'Batch40 executable differs from the new protocol50/FNS15 build guard.'}
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) {$ArtifactsRoot=Join-Path $repo ('work\fusion\batch40-264\'+$familyName)}
$runId=[Guid]::NewGuid().ToString('N');$runRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$runId)
$hostRoot=Join-Path $runRoot 'host';$clientRoot=Join-Path $runRoot 'client'
$hostLog=Join-Path $hostRoot 'logs';$clientLog=Join-Path $clientRoot 'logs'
$hostProcess=$null;$clientProcess=$null;$passed=$false;$errorText='';$common=@();$frames=@();$cases=@();$started=[DateTime]::UtcNow
$environment=@{}
foreach ($item in Get-ChildItem Env: | Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION*'}) {$environment[$item.Name]=$item.Value}
$enabled='PVZ_COOP_SMOKE_FUSION_'+$marker
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE',$enabled)) {if (-not $environment.ContainsKey($name)) {$environment[$name]=$null}}
function Assert-Batch40Frames([string]$directory,[string]$role,[int]$width,[int]$groups) {
    Add-Type -AssemblyName System.Drawing
    for ($group=0;$group -lt $groups;$group++) {
        $phases=@('chooser','before','flight','complete')
        if($Family -eq 'Cold' -and $group -eq 2){$phases+=@('potato_light2','potato_light1')}
        if($Family -eq 'Shooter' -and $group -eq 1){$phases+=@('tier301')}
        foreach ($phase in $phases) {
        $name='fusion_'+$familyName+'40_'+$group+'_'+$phase+'.bmp';$path=Join-Path $directory $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {throw ('Missing actual owned SDL frame: '+$path)}
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $width -or $bitmap.Height -ne 600) {throw ('Wrong owned frame dimensions: '+$path)}
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) {for ($y=16;$y -lt $bitmap.Height;$y+=31) {[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
            if ($colors.Count -lt 8) {throw ('Flat/blank native frame: '+$path)}
        } finally {$bitmap.Dispose()}
        [ordered]@{role=$role;group=$group;phase=$phase;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }}
}
try {
    foreach ($path in @($GameExe,(Join-Path $gameDirectory 'main.pak'),(Join-Path $gameDirectory 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {throw ('Runtime input missing: '+$path)}
    }
    foreach ($dir in @($hostLog,$clientLog,(Join-Path $hostRoot 'appdata'),(Join-Path $clientRoot 'appdata'))) {Ensure-Directory $dir}
    $port=Get-FreeLoopbackPort
    foreach ($name in $environment.Keys) {[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    [Environment]::SetEnvironmentVariable('PVZ_COOP_SMOKE_FUSION_NATIVE','1','Process')
    [Environment]::SetEnvironmentVariable($enabled,'1','Process')
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$hostRoot} else {$clientRoot};$width=if ($role -eq 'host') {1066} else {800}
        $nick=$Family+$role
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nick -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nick `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') `
            -ResultPath (Join-Path $root 'result.json') -SelectedPort $port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Batch40 '+$Family+' hidden owned pair: '+$runRoot)
    $hostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDirectory -ManifestPath (Join-Path $hostRoot 'smoke.ini') -Role host -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runRoot 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $hostLog -Pattern ('(?im)host listening port='+$port+'\b') -Process $hostProcess -Role host -TimeoutSeconds 120
    $clientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDirectory -ManifestPath (Join-Path $clientRoot 'smoke.ini') -Role client -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runRoot 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $clientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $hostRoot 'result.json') -ClientResultPath (Join-Path $clientRoot 'result.json') `
        -HostProcess $hostProcess -ClientProcess $clientProcess -HostLogDirectory $hostLog -ClientLogDirectory $clientLog -TimeoutSeconds $TimeoutSeconds)
    if ($results.Count -ne 2) {throw 'Expected two actual peer results.'}
    foreach ($result in $results) {
        if ($result.status -ne 'pass' -or $result.protocol -ne 50 -or $result.mode -ne 'fusion_validation' -or $result.scenario -ne 'fusion_validation' -or $result.skipIntro) {throw 'Peer result failed or does not match batch50 manifest.'}
    }
    $hostText=Get-CombinedLogText $hostLog;$clientText=Get-CombinedLogText $clientLog
    $groups=if ($Family -eq 'Fire') {1} else {3}
    foreach ($role in @('host','client')) {
        $logText=if ($role -eq 'host') {$hostText} else {$clientText}
        if ($logText -match 'SMOKE_FAIL|SMOKE_STOPPED|seq-gap|invalid snapshot|snapshot encode rejected|fatal|secondWorld=1') {throw ('Native pair failure in '+$role)}
        if ($logText -notmatch ('SMOKE_'+$marker+'_COMPLETE role='+$role+'\b')) {throw ('Missing family completion proof for '+$role)}
        for ($group=0;$group -lt $groups;$group++) {
            $case=[regex]::Match($logText,('SMOKE_'+$marker+'_CASE role='+$role+' group='+$group+'\b[^\r\n]*'))
            if (-not $case.Success) {throw ('Missing actual grouped outcome '+$role+'/'+$group)}
            $cases+=[ordered]@{role=$role;group=$group;proof=$case.Value}
        }
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$rx=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $common=@($rx|Where-Object {$tx -contains $_}|Select-Object -Unique)
    if ($common.Count -lt 8) {throw 'Fewer than eight common authoritative snapshots.'}
    $frames=@((Assert-Batch40Frames $hostRoot host 1066 $groups),(Assert-Batch40Frames $clientRoot client 800 $groups))
    $passed=$true;Write-SmokeStatus ('PASS Batch40 '+$Family+' common='+$common.Count)
} catch {
    $errorText=$_.Exception.Message;if ($_.ScriptStackTrace) {$errorText+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Batch40 '+$Family+': '+$errorText)
} finally {
    foreach ($name in $environment.Keys) {[Environment]::SetEnvironmentVariable($name,$environment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $clientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $hostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$passed=$false;$errorText+=' Owned process cleanup failed.'}
    Ensure-Directory $runRoot
    $summary=[ordered]@{status=$(if ($passed) {'pass'} else {'fail'});error=$errorText;family=$Family;fixture='Batch40-264';run_id=$runId;
        started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;sha256=$exeHash;protocol=50;fns=15;
        native_grid='10x5';source_campaign_complete=$false;factory_setup=$true;source_acquisition_tested=$false;
        common_snapshot_hashes=$common;cases=$cases;frames=$frames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$runRoot}
    Write-Utf8NoBom -Path (Join-Path $runRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($passed) {exit 0} else {exit 1}
