#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Shooter','Action','Economy')][string]$Family = 'Shooter',
    [string]$GameExe = '', [string]$ArtifactsRoot = '',
    [ValidateRange(180,1200)][int]$TimeoutSeconds = 480,
    [uint32]$Seed = 424242
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Seed $Seed `
    -HostWindowWidth 1066 -ClientWindowWidth 800
$spec = switch ($Family) {
    'Shooter' { @{ Marker='EXTENDED_SHOOTER40'; Prefix='fusion_extended_shooter40_'; Groups=4; Plants=15; Phases=@('chooser','before','flight','complete'); GroupedFrames=$true } }
    'Action' { @{ Marker='ACTION266'; Prefix='fusion_action266_'; Groups=1; Plants=20; Phases=@('chooser','before','flight','complete'); GroupedFrames=$true } }
    'Economy' { @{ Marker='ECONOMY266'; Prefix='fusion_economy266_'; Groups=1; Plants=5; Phases=@('chooser','before','production','grown','skill','complete'); GroupedFrames=$false } }
}
$repo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $repo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $gameDirectory=Split-Path -Parent $GameExe
$guardPath=Join-Path $repo 'work\fusion\batch40-266\verified-build.json'
if (-not (Test-Path -LiteralPath $guardPath -PathType Leaf)) { throw 'Batch266 build guard missing; build and verify the integrated sources first.' }
$guard=Get-Content -LiteralPath $guardPath -Raw | ConvertFrom-Json
$exeHash=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
if ($guard.sha256 -ne $exeHash -or $guard.protocol -ne 51 -or $guard.fns -ne 16) { throw 'Batch266 executable differs from the protocol51/FNS16 SHA256 build guard.' }
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $repo ('work\fusion\batch40-266\'+$Family.ToLowerInvariant()) }
$runId=[Guid]::NewGuid().ToString('N'); $runRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$runId)
$hostRoot=Join-Path $runRoot 'host'; $clientRoot=Join-Path $runRoot 'client'
$hostLog=Join-Path $hostRoot 'logs'; $clientLog=Join-Path $clientRoot 'logs'
$hostProcess=$null; $clientProcess=$null; $passed=$false; $errorText=''
$common=@(); $frames=@(); $cases=@(); $started=[DateTime]::UtcNow
$environment=@{}
foreach ($item in Get-ChildItem Env: | Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION*'}) { $environment[$item.Name]=$item.Value }
$enabled='PVZ_COOP_SMOKE_FUSION_'+$spec.Marker
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE',$enabled)) { if (-not $environment.ContainsKey($name)) { $environment[$name]=$null } }

function Assert-Batch266NoFailure([string]$text,[string]$role) {
    $pattern='(?im)\bSMOKE_(?:FAIL|STOPPED)\b|\bseq-gap\b|invalid snapshot|snapshot[^\r\n]*reject|validation[^\r\n]*reject|native Fusion[^\r\n]*reject|\bfatal\b|secondWorld=1|Access Violation|unhandled exception|assertion failed|protocol violation|queue overflow|version mismatch'
    $match=[regex]::Match($text,$pattern)
    if ($match.Success) { throw ('Batch266 '+$role+' failure: '+$match.Value) }
}
# Wait-ForResults resolves this helper at runtime. Keep the standard owned
# process/result polling, with the stricter native snapshot gate throughout.
function Assert-NoLiveFailure([string]$HostLogDirectory,[string]$ClientLogDirectory) {
    Assert-Batch266NoFailure (Get-CombinedLogText $HostLogDirectory) host
    Assert-Batch266NoFailure (Get-CombinedLogText $ClientLogDirectory) client
}
function Assert-Batch266Frames([string]$directory,[string]$role,[int]$width) {
    Add-Type -AssemblyName System.Drawing
    for ($group=0; $group -lt $spec.Groups; $group++) {
        foreach ($phase in $spec.Phases) {
            $stem=$spec.Prefix+$(if ($spec.GroupedFrames) { [string]$group+'_' } else { '' })+$phase
            $path=Join-Path $directory ($stem+'.bmp')
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual owned SDL frame: '+$path) }
            $bitmap=[Drawing.Bitmap]::new($path)
            try {
                if ($bitmap.Width -ne $width -or $bitmap.Height -ne 600) { throw ('Wrong owned frame dimensions: '+$path) }
                $colors=New-Object 'System.Collections.Generic.HashSet[int]'
                for ($x=16; $x -lt $bitmap.Width; $x+=37) { for ($y=16; $y -lt $bitmap.Height; $y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
                if ($colors.Count -lt 8) { throw ('Flat/blank native frame: '+$path) }
                $colorCount=$colors.Count
            } finally { $bitmap.Dispose() }
            [ordered]@{role=$role;group=$(if ($spec.GroupedFrames) {$group} else {$null});phase=$phase;path=$path;width=$width;height=600;sampled_colors=$colorCount;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
        }
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $gameDirectory 'main.pak'),(Join-Path $gameDirectory 'SDL3.dll'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Runtime input missing: '+$path) }
    }
    foreach ($dir in @($hostLog,$clientLog,(Join-Path $hostRoot 'appdata'),(Join-Path $clientRoot 'appdata'))) { Ensure-Directory $dir }
    $port=Get-FreeLoopbackPort
    foreach ($name in $environment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    [Environment]::SetEnvironmentVariable('PVZ_COOP_SMOKE_FUSION_NATIVE','1','Process')
    [Environment]::SetEnvironmentVariable($enabled,'1','Process')
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$hostRoot} else {$clientRoot}; $width=if ($role -eq 'host') {1066} else {800}
        $nick=$Family+$role
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nick -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nick `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') `
            -ResultPath (Join-Path $root 'result.json') -SelectedPort $port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Batch266 '+$Family+' hidden owned pair: '+$runRoot)
    # Exactly two owned processes; Start-SmokeProcess records path/start time,
    # hidden HWND ownership and per-process SDL capture destinations.
    $hostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDirectory -ManifestPath (Join-Path $hostRoot 'smoke.ini') -Role host -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runRoot 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $hostLog -Pattern ('(?im)host listening port='+$port+'\b') -Process $hostProcess -Role host -TimeoutSeconds 120
    $clientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDirectory -ManifestPath (Join-Path $clientRoot 'smoke.ini') -Role client -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runRoot 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $clientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $hostRoot 'result.json') -ClientResultPath (Join-Path $clientRoot 'result.json') `
        -HostProcess $hostProcess -ClientProcess $clientProcess -HostLogDirectory $hostLog -ClientLogDirectory $clientLog -TimeoutSeconds $TimeoutSeconds)
    if ($results.Count -ne 2) { throw 'Expected two actual peer results.' }
    for ($i=0; $i -lt 2; $i++) {
        $role=if ($i -eq 0) {'host'} else {'client'}
        $result=$results[$i]
        if ($result.status -ne 'pass' -or $result.role -ne $role -or $result.protocol -ne 51 -or
            $result.mode -ne 'fusion_validation' -or $result.scenario -ne 'fusion_validation' -or
            $result.level -ne 1 -or $result.seed -ne $Seed -or $result.skipIntro) { throw ('Batch266 '+$role+' result differs from the guarded manifest.') }
        $snapshotProperty=if ($role -eq 'host') {'snapshotsSent'} else {'snapshotsApplied'}
        if ([int](Get-RequiredProperty -Object $result -Name $snapshotProperty -Role $role) -lt 8) { throw ('Batch266 '+$role+' result has fewer than eight snapshots.') }
        # These native fixtures return before the generic scenario action hook.
        # Their CASE/COMPLETE barriers below own combat/paid-action acceptance.
    }
    $hostText=Get-CombinedLogText $hostLog; $clientText=Get-CombinedLogText $clientLog
    foreach ($role in @('host','client')) {
        $logText=if ($role -eq 'host') {$hostText} else {$clientText}
        Assert-Batch266NoFailure $logText $role
        $complete=[regex]::Match($logText,('(?m)SMOKE_'+$spec.Marker+'_COMPLETE role='+$role+'\b[^\r\n]*'))
        if (-not $complete.Success) { throw ('Missing Batch266 completion proof for '+$role) }
        for ($group=0; $group -lt $spec.Groups; $group++) {
            $groupPart=if ($Family -eq 'Economy') {''} else {' group='+$group+'\b'}
            $case=[regex]::Match($logText,('(?m)SMOKE_'+$spec.Marker+'_CASE role='+$role+'\b'+$groupPart+'[^\r\n]*'))
            if (-not $case.Success -or $case.Value -notmatch '\bpostSetupActorWrites=0\b') { throw ('Missing actual grouped outcome '+$role+'/'+$group) }
            $cases+=[ordered]@{role=$role;group=$(if ($Family -eq 'Economy') {$null} else {$group});proof=$case.Value;completion=$complete.Value}
        }
        if ($Family -eq 'Shooter' -and $logText -notmatch ('SMOKE_EXTENDED_SHOOTER40_MONEY role='+$role+'\b[^\r\n]*ledger=2000[^\r\n]*clientCommand=1')) { throw ('Missing client Gold upgrade proof for '+$role) }
        if ($Family -eq 'Economy' -and $complete.Value -notmatch '\bpaidClientActions=3\b') { throw ('Missing Economy three-action paid proof for '+$role) }
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX); $rx=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $common=@($rx | Where-Object {$tx -contains $_} | Select-Object -Unique)
    if ($common.Count -lt 8) { throw 'Fewer than eight common authoritative snapshots.' }
    # Append each stream separately: summary.frames must be a flat object array.
    $frames+=@(Assert-Batch266Frames $hostRoot host 1066)
    $frames+=@(Assert-Batch266Frames $clientRoot client 800)
    $passed=$true; Write-SmokeStatus ('PASS Batch266 '+$Family+' common='+$common.Count)
} catch {
    $errorText=$_.Exception.Message; if ($_.ScriptStackTrace) {$errorText+=' | '+$_.ScriptStackTrace}
    Write-SmokeStatus ('FAIL Batch266 '+$Family+': '+$errorText)
} finally {
    foreach ($name in $environment.Keys) { [Environment]::SetEnvironmentVariable($name,$environment[$name],'Process') }
    # The shared helper checks PID + executable + start time before closing.
    $clientStopped=Stop-OwnedProcess -Process $clientProcess -Role client
    $hostStopped=Stop-OwnedProcess -Process $hostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) {$passed=$false; $errorText+=' Owned process cleanup failed.'}
    Ensure-Directory $runRoot
    $expectedPaid=if ($Family -eq 'Shooter') {1} elseif ($Family -eq 'Economy') {3} else {0}
    $acquisition=[ordered]@{full_recipe_acquisition_tested=$false;fixture_uses_initial_factory=$true;paid_client_actions_expected=$expectedPaid;paid_client_actions_verified=$(if ($passed) {$expectedPaid} else {0});scope=$(if ($Family -eq 'Shooter') {'MoneySign1134-to-1135 fresh UID/provenance, fee1000, native Sun25 and supershoot'} elseif ($Family -eq 'Economy') {'Three client MoneySign actions, refunds8/2/2 and native skills'} else {'Initial factories only; source acquisition not executed'})}
    $summary=[ordered]@{status=$(if ($passed) {'pass'} else {'fail'});error=$errorText;family=$Family;fixture='Batch40-266';run_id=$runId;
        started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;sha256=$exeHash;protocol=51;fns=16;
        native_grid='10x5';source_plants_expected=$spec.Plants;source_campaign_complete=$false;factory_outcome_observed=($cases.Count -gt 0);source_acquisition=$acquisition;
        common_snapshot_hashes=$common;cases=$cases;frames=@($frames);owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$runRoot}
    Write-Utf8NoBom -Path (Join-Path $runRoot 'summary.json') -Text (($summary | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if ($passed) {exit 0} else {exit 1}
