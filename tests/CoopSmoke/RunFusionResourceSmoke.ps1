#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateRange(90,600)][int]$TimeoutSeconds = 180,
    [ValidateRange(0,65535)][int]$Port = 0,
    [ValidateSet(800,1066)][int]$HostWindowWidth = 1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth = 800,
    [uint32]$Seed = 424242,
    [switch]$PreflightOnly,
    [string]$ValidateExisting = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed `
    -HostWindowWidth $HostWindowWidth -ClientWindowWidth $ClientWindowWidth

function Assert-Resource([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Test-ResourcePeerResult($Result,[string]$Role,[uint32]$ExpectedSeed) {
    if ($null -eq $Result) { return $false }
    try {
        $count=if($Role -eq 'host'){[int]$Result.snapshotsSent}else{[int]$Result.snapshotsApplied}
        return ($Result.status -eq 'pass' -and $Result.role -eq $Role -and $Result.mode -eq 'fusion_validation' -and
            $Result.scenario -eq 'fusion_validation' -and [int]$Result.level -eq 1 -and [uint32]$Result.seed -eq $ExpectedSeed -and
            [int]$Result.protocol -eq 48 -and -not $Result.skipIntro -and $count -ge 8)
    } catch { return $false }
}
function Assert-ResourceOutcome([string]$RunRoot,[uint32]$Seed,[int]$ClientWindowWidth,[bool]$Restored) {
    $hostRoot=Join-Path $RunRoot 'host';$clientRoot=Join-Path $RunRoot 'client'
    $hostLog=Join-Path $hostRoot 'logs';$clientLog=Join-Path $clientRoot 'logs'
    Assert-NoLiveFailure -HostLogDirectory $hostLog -ClientLogDirectory $clientLog
    $hr=Read-JsonIfComplete (Join-Path $hostRoot 'result.json');$cr=Read-JsonIfComplete (Join-Path $clientRoot 'result.json')
    Assert-Resource (Test-ResourcePeerResult $hr 'host' $Seed) 'Invalid or incomplete resource result: host'
    Assert-Resource (Test-ResourcePeerResult $cr 'client' $Seed) 'Invalid or incomplete resource result: client'
    $h=Get-CombinedLogText $hostLog;$c=Get-CombinedLogText $clientLog
    Assert-Resource ($h -match 'HELLO slot=1 protocol=48 expected=48' -and $c -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Matching protocol48/Fusion handshake missing.'
    $repick=[regex]::Match($c,'SMOKE_FUSION_RESOURCE_REPICK role=client packet=(\d+) samples=\d+ nativeSDL=1')
    $retry=[regex]::Match($c,'SMOKE_FUSION_RESOURCE_RETRY role=client packet=(\d+) parent=\d+ nativeSDL=1 hostFileRestored=1 repickEcho=1')
    Assert-Resource ($repick.Success -and $retry.Success -and $repick.Groups[1].Value -eq $retry.Groups[1].Value) 'Real SDL repick/fresh authoritative held echo before retry missing.'
    Assert-Resource ($restored -and $h -match 'SMOKE_FUSION_RESOURCE_POSITIVE[^\r\n]*kind=1000 parents=0,1 owner=1[^\r\n]*price=100 sun=300 recharge=750') 'Host restored real paid retry proof missing.'
    Assert-Resource ($c -match 'snapshot resources unavailable[^\r\n]*plant kind=1000[^\r\n]*cannot open[^\r\n]*peasunflower[\\/]animation.json' -and $c -match 'host state rejected context=FUSION_LOCAL_RESOURCE') 'Real client resource-admission failure path missing.'
    Assert-Resource ($c -match 'SMOKE_FUSION_RESOURCE_CLIENT_PROBE[^\r\n]*actualQueuedSnapshot=1 FNSValid=1 readerUnchanged=1 worldBytesUnchanged=1 parentAlive=1 liveBoard=1 stillConnected=1 applied=0 deferred=1') 'Client atomic negative proof missing.'
    Assert-Resource ($c -match 'SMOKE_FUSION_RESOURCE_DEFERRED[^\r\n]*boardAliveBeforeTeardown=1' -and $c -match 'SMOKE_FUSION_RESOURCE_TEARDOWN[^\r\n]*boardGone=1 selector=1 infoDialog=1 deferred=1 actualDraws=2') 'Deferred local-resource teardown/UI proof missing.'
    $tx=@(Get-SnapshotHashes -Text $h -Marker SMOKE_SNAPSHOT_TX);$applied=@(Get-SnapshotHashes -Text $c -Marker SMOKE_SNAPSHOT_APPLIED)
    $common=@($applied | Where-Object { $tx -contains $_ } | Select-Object -Unique)
    Assert-Resource ($common.Count -ge 8) 'Fewer than eight common pre-failure native hashes.'
    Add-Type -AssemblyName System.Drawing
    foreach ($frame in @((Join-Path $clientRoot 'fusion_resource_before.bmp'),(Join-Path $clientRoot 'fusion_resource_error.bmp'))) {
        Assert-Resource (Test-Path -LiteralPath $frame -PathType Leaf) ('Missing actual SDL frame: '+$frame)
        $bitmap=[Drawing.Bitmap]::new($frame)
        try {
            Assert-Resource ($bitmap.Width -eq $ClientWindowWidth -and $bitmap.Height -eq 600) 'Actual client viewport mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for($x=16;$x -lt $bitmap.Width;$x+=37){for($y=16;$y -lt $bitmap.Height;$y+=31){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
            Assert-Resource ($colors.Count -ge 8) ('Blank resource frame: '+$frame)
        } finally { $bitmap.Dispose() }
    }
    return [pscustomobject]@{common=@($common);hostSnapshots=[int]$hr.snapshotsSent;clientSnapshots=[int]$cr.snapshotsApplied;frameCount=2}
}

$repo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if (-not $GameExe) { $GameExe = Join-Path $repo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$game = Split-Path -Parent $GameExe
if (-not $ArtifactsRoot) { $ArtifactsRoot = Join-Path $repo 'work\fusion\resource229-runs' }
$ArtifactsRoot = Get-NormalizedPath $ArtifactsRoot
$installedTarget = Join-Path $game 'fusion\pilot\peasunflower\animation.json'
foreach ($path in @($GameExe,$installedTarget,(Join-Path $game 'main.pak'),(Join-Path $game 'dependency.pak'),(Join-Path $game 'SDL3.dll'))) {
    Assert-Resource (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
}
Assert-Resource ([IO.Path]::GetPathRoot($game) -eq [IO.Path]::GetPathRoot($ArtifactsRoot)) 'Isolated hardlinks require the source runtime volume.'
$exeHash = (Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
$targetHash = (Get-FileHash -LiteralPath $installedTarget -Algorithm SHA256).Hash
if ($PreflightOnly) {
    [ordered]@{preflight='Passed';exe=$GameExe;exe_sha256=$exeHash;target=$installedTarget;target_sha256=$targetHash;spawned=0;renamed=0;copied=0} | ConvertTo-Json
    return
}

if ($ValidateExisting) {
    Assert-Resource (-not $PreflightOnly) 'ValidateExisting and PreflightOnly are mutually exclusive.'
    $existingRoot=Get-NormalizedPath $ValidateExisting
    $summaryPath=Join-Path $existingRoot 'summary.json'
    $oldSummaryHash=(Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash
    $old=Read-JsonIfComplete $summaryPath
    Assert-Resource ($null -ne $old) 'Saved resource summary is missing or incomplete.'
    Assert-Resource (Test-SafeRunRoot -RunRoot $existingRoot -ArtifactsRoot (Split-Path -Parent $existingRoot) -RunId ([string]$old.run_id)) 'Unsafe existing resource run root.'
    $postPath=Join-Path $existingRoot 'postvalidation.json'
    Assert-Resource (-not (Test-Path -LiteralPath $postPath)) 'Refusing to overwrite existing postvalidation evidence.'
    $validated=$false;$validationError='';$identity=@();$outcome=$null
    try {
        Assert-Resource ($old.status -eq 'fail' -and $old.error -eq 'Owned peer exited before negative-test results.' -and
            $old.fixture -eq 'FusionRealMissingResourceAtomicity' -and $old.owned_cleanup) 'Saved run is not the known completed-peer watch-loop failure.'
        Assert-Resource ($exeHash -eq $old.executable_sha256 -and $targetHash -eq $old.target_sha256 -and
            (Get-NormalizedPath $old.installed_target) -eq $installedTarget) 'Installed runtime identity differs from the saved run.'
        $identity+= [pscustomobject]@{role='installed';kind='exe';path=$GameExe;sha256=$exeHash}
        $identity+= [pscustomobject]@{role='installed';kind='target';path=$installedTarget;sha256=$targetHash}
        Assert-Resource (@($old.owned_targets).Count -eq 2) 'Expected exactly two saved owned targets.'
        foreach($role in @('host','client')) {
            $runtime=Join-Path $existingRoot ($role+'-runtime')
            $cloneExe=Join-Path $runtime ([IO.Path]::GetFileName($GameExe))
            $cloneHash=(Get-FileHash -LiteralPath $cloneExe -Algorithm SHA256).Hash
            Assert-Resource ($cloneHash -eq $old.executable_sha256) ('Saved clone executable hash differs: '+$role)
            $identity+= [pscustomobject]@{role=$role;kind='exe';path=$cloneExe;sha256=$cloneHash}
            $targets=@($old.owned_targets | Where-Object {$_.role -eq $role})
            Assert-Resource ($targets.Count -eq 1) ('Ambiguous saved owned target: '+$role)
            $target=Join-Path $runtime 'fusion\pilot\peasunflower\animation.json'
            Assert-Resource ((Get-NormalizedPath $targets[0].path) -eq $target -and
                (Get-NormalizedPath $targets[0].disabled) -eq ($target+'.resource229-disabled') -and
                -not (Test-Path -LiteralPath $targets[0].disabled)) ('Saved target was not restored at the expected owned path: '+$role)
            $cloneTargetHash=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
            Assert-Resource ($cloneTargetHash -eq $old.target_sha256) ('Saved clone target hash differs: '+$role)
            $identity+= [pscustomobject]@{role=$role;kind='target';path=$target;sha256=$cloneTargetHash}
        }
        Assert-Resource (Test-Path -LiteralPath (Join-Path $existingRoot 'host-restored.marker') -PathType Leaf) 'Actual host restoration marker missing.'
        $outcome=Assert-ResourceOutcome -RunRoot $existingRoot -Seed $Seed -ClientWindowWidth $ClientWindowWidth -Restored $true
        $validated=$true
    } catch { $validationError=$_.Exception.Message }
    $summaryUnchanged=(Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash -eq $oldSummaryHash
    if(-not $summaryUnchanged){$validated=$false;$validationError+=' Original summary hash changed.'}
    $post=[ordered]@{status=$(if($validated){'pass'}else{'fail'});validation='existing-artifacts-only';error=$validationError;
        original_harness_status=$old.status;original_harness_error=$old.error;original_summary_sha256=$oldSummaryHash;original_summary_unchanged=$summaryUnchanged;
        executable_and_target_identities=@($identity);outcome=$outcome;owned_cleanup=$old.owned_cleanup;spawned=0;staged=0;renamed=0;
        runtime_rng_equality='Not executed';scope='Same final result/log/hash/frame assertions; observed completed 58w run, no new game execution.'}
    Write-Utf8NoBom -Path $postPath -Text (($post | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
    Assert-Resource $validated ('Saved resource validation failed: '+$validationError)
    Write-Output ('Fusion resource saved-artifact validation Passed; evidence: '+$postPath)
    return
}

$runId = [Guid]::NewGuid().ToString('N')
$runRoot = Join-Path $ArtifactsRoot ('run_'+$runId)
Assert-Resource (-not (Test-Path -LiteralPath $runRoot)) 'Run root must be fresh.'
Assert-Resource (Test-SafeRunRoot -RunRoot $runRoot -ArtifactsRoot $ArtifactsRoot -RunId $runId) 'Unsafe resource run root.'
$started = [DateTime]::UtcNow
$hostProcess = $null; $clientProcess = $null
$pass = $false; $errorText = ''; $restored = $false; $common = @()
$ownedTargets = @(); $inputFiles = New-Object 'System.Collections.Generic.List[object]'; $inputHashes = @{}
$savedEnv = @{}
$envNames = @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_RESOURCE','PVZ_COOP_SMOKE_FUSION_RESOURCE_ROOT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT','PVZ_COOP_SMOKE_STARTUP_VIDEO')
foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'PVZ_COOP_SMOKE_FUSION_*' })) { $envNames += $item.Name }
foreach ($name in @($envNames | Select-Object -Unique)) { $savedEnv[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }

function Assert-OwnedPath([string]$Path) {
    $full = Get-NormalizedPath $Path
    Assert-Resource ($full.StartsWith($runRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) ('Path escaped owned run root: '+$full)
    return $full
}
function Link-ResourceFile([string]$Source,[string]$Destination) {
    $destinationPath = Assert-OwnedPath $Destination
    if (-not $inputHashes.ContainsKey($Source)) { $inputHashes[$Source]=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash }
    [void](New-Item -ItemType HardLink -Path $destinationPath -Target $Source)
    $inputFiles.Add([ordered]@{source=$Source;clone=$destinationPath;sha256=$inputHashes[$Source]})
}
function Stage-ResourceTree([string]$Source,[string]$Destination,[bool]$CopyBytes=$false) {
    [void](Assert-OwnedPath $Destination); Ensure-Directory $Destination
    foreach ($item in @(Get-ChildItem -LiteralPath $Source -Force)) {
        Assert-Resource (-not ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) ('Unexpected source reparse point: '+$item.FullName)
        $next = Join-Path $Destination $item.Name
        if ($item.PSIsContainer) { Stage-ResourceTree $item.FullName $next $CopyBytes }
        elseif ($CopyBytes) { Copy-Item -LiteralPath $item.FullName -Destination $next }
        else { Link-ResourceFile $item.FullName $next }
    }
}
function Stage-ResourceRuntime([string]$Destination) {
    Ensure-Directory $Destination
    foreach ($file in @(Get-ChildItem -LiteralPath $game -File)) {
        if ($file.Extension -in @('.exe','.dll','.pak') -or $file.Name -in @('Cursor13.png','Cursor13.bmp')) {
            Link-ResourceFile $file.FullName (Join-Path $Destination $file.Name)
        }
    }
    foreach ($name in @('fusion','properties','dependency','extension','licenses')) {
        $source = Join-Path $game $name
        if (Test-Path -LiteralPath $source -PathType Container) { Stage-ResourceTree $source (Join-Path $Destination $name) }
    }
    # Native compiled caches may be writable. Their bytes are independent.
    $compiled = Join-Path $game 'compiled'
    if (Test-Path -LiteralPath $compiled -PathType Container) { Stage-ResourceTree $compiled (Join-Path $Destination 'compiled') $true }
}
function Restore-ResourceTarget($Target) {
    [void](Assert-OwnedPath $Target.path); [void](Assert-OwnedPath $Target.disabled)
    if (Test-Path -LiteralPath $Target.disabled -PathType Leaf) {
        Assert-Resource (-not (Test-Path -LiteralPath $Target.path)) 'Both original and disabled owned resource names exist.'
        Move-Item -LiteralPath $Target.disabled -Destination $Target.path
    }
    Assert-Resource ((Get-FileHash -LiteralPath $Target.path -Algorithm SHA256).Hash -eq $targetHash) 'Restored clone target hash changed.'
}
try {
    Ensure-Directory $runRoot
    foreach ($name in $savedEnv.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_RESOURCE='1'
    $env:PVZ_COOP_SMOKE_FUSION_RESOURCE_ROOT=$runRoot; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($role in @('host','client')) {
        $root = Join-Path $runRoot $role; $runtime = Join-Path $runRoot ($role+'-runtime')
        foreach ($dir in @($root,(Join-Path $root 'logs'),(Join-Path $root 'appdata'))) { Ensure-Directory $dir }
        Stage-ResourceRuntime $runtime
        $cloneExe = Join-Path $runtime ([IO.Path]::GetFileName($GameExe))
        Assert-Resource ((Get-FileHash -LiteralPath $cloneExe -Algorithm SHA256).Hash -eq $exeHash) 'Clone executable hash changed.'
        $target = Join-Path $runtime 'fusion\pilot\peasunflower\animation.json'
        Assert-Resource ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -eq $targetHash) 'Clone target hash changed before rename.'
        $disabled = $target+'.resource229-disabled'
        [void](Assert-OwnedPath $target); [void](Assert-OwnedPath $disabled)
        Move-Item -LiteralPath $target -Destination $disabled
        $ownedTargets += [pscustomobject]@{role=$role;path=$target;disabled=$disabled}
        $width = if ($role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname ('Resource'+$role) -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname ('Resource'+$role) `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') `
            -ResultPath (Join-Path $root 'result.json') -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 `
            -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    $hostRoot=Join-Path $runRoot 'host';$clientRoot=Join-Path $runRoot 'client'
    $hostLog=Join-Path $hostRoot 'logs';$clientLog=Join-Path $clientRoot 'logs'
    $hostExe=Join-Path (Join-Path $runRoot 'host-runtime') ([IO.Path]::GetFileName($GameExe))
    $clientExe=Join-Path (Join-Path $runRoot 'client-runtime') ([IO.Path]::GetFileName($GameExe))
    $hostProcess=Start-SmokeProcess -Executable $hostExe -WorkingDirectory (Split-Path -Parent $hostExe) -ManifestPath (Join-Path $hostRoot 'smoke.ini') -Role host -RunId $runId -EnableUnicodeChat $false
    Set-SmokeWindowLayout -Process $hostProcess -Role host -X 0 -Y 0 -Width $HostWindowWidth -Height 600
    Wait-ForLogPattern -LogDirectory $hostLog -Pattern ('host listening port='+$Port+'\b') -Process $hostProcess -Role host -TimeoutSeconds 120
    $clientProcess=Start-SmokeProcess -Executable $clientExe -WorkingDirectory (Split-Path -Parent $clientExe) -ManifestPath (Join-Path $clientRoot 'smoke.ini') -Role client -RunId $runId -EnableUnicodeChat $false
    Set-SmokeWindowLayout -Process $clientProcess -Role client -X $HostWindowWidth -Y 0 -Width $ClientWindowWidth -Height 600
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Assert-NoLiveFailure -HostLogDirectory $hostLog -ClientLogDirectory $clientLog
        $h=Get-CombinedLogText $hostLog;$c=Get-CombinedLogText $clientLog
        if ($h -match 'placement resource admission rejected kind=1000[^\r\n]*cannot open[^\r\n]*peasunflower[\\/]animation.json') {
            $marker=Join-Path $runRoot 'host-rejection-observed.marker'
            if (-not (Test-Path -LiteralPath $marker)) { Write-Utf8NoBom $marker 'actual host resource rejection observed' }
        }
        if (-not $restored -and $h -match 'SMOKE_FUSION_RESOURCE_REJECT role=host' -and $c -match 'SMOKE_FUSION_RESOURCE_REJECT role=client') {
            Restore-ResourceTarget $ownedTargets[0]
            Write-Utf8NoBom (Join-Path $runRoot 'host-restored.marker') 'host clone restored; client clone remains missing'
            $restored=$true
        }
        $hr=Read-JsonIfComplete (Join-Path $hostRoot 'result.json');$cr=Read-JsonIfComplete (Join-Path $clientRoot 'result.json')
        if ($null -ne $hr -and $null -ne $cr) { break }
        Assert-Resource ((-not $hostProcess.HasExited) -or (Test-ResourcePeerResult $hr 'host' $Seed)) 'Owned host exited without its own complete PASS result.'
        Assert-Resource ((-not $clientProcess.HasExited) -or (Test-ResourcePeerResult $cr 'client' $Seed)) 'Owned client exited without its own complete PASS result.'
        Start-Sleep -Milliseconds 100
    }
    Assert-Resource ($null -ne $hr -and $null -ne $cr) 'Resource scenario timed out.'
    $outcome=Assert-ResourceOutcome -RunRoot $runRoot -Seed $Seed -ClientWindowWidth $ClientWindowWidth -Restored $restored
    $common=@($outcome.common)
    $pass=$true
} catch { $errorText=$_.Exception.Message; Write-Warning $errorText }
finally {
    $cleanClient=Stop-OwnedProcess -Process $clientProcess -Role client
    $cleanHost=Stop-OwnedProcess -Process $hostProcess -Role host
    foreach ($target in $ownedTargets) { try { Restore-ResourceTarget $target } catch { $pass=$false;$errorText+=' Restore: '+$_.Exception.Message } }
    foreach ($item in $inputFiles) {
        if ((Get-FileHash -LiteralPath $item.source -Algorithm SHA256).Hash -ne $item.sha256 -or (Get-FileHash -LiteralPath $item.clone -Algorithm SHA256).Hash -ne $item.sha256) {
            $pass=$false;$errorText+=' Immutable clone hash mismatch: '+$item.clone;break
        }
    }
    if ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -ne $exeHash -or (Get-FileHash -LiteralPath $installedTarget -Algorithm SHA256).Hash -ne $targetHash) {
        $pass=$false;$errorText+=' Installed runtime/target hash changed.'
    }
    foreach ($name in $savedEnv.Keys) { [Environment]::SetEnvironmentVariable($name,$savedEnv[$name],'Process') }
    if (-not $cleanClient -or -not $cleanHost) { $pass=$false;$errorText+=' Owned cleanup failed.' }
    Ensure-Directory $runRoot
    $summary=[ordered]@{status=$(if($pass){'pass'}else{'fail'});error=$errorText;run_id=$runId;started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
        executable_sha256=$exeHash;installed_target=$installedTarget;target_sha256=$targetHash;owned_targets=$ownedTargets;common_prefailure_hashes=@($common);owned_cleanup=($cleanClient -and $cleanHost);
        fixture='FusionRealMissingResourceAtomicity';resource_kind=1000;prices=@(100,100);wallet=@(500,400,300);runtime_rng_equality='Not executed';
        limits='Real source0 plus client extra1, valid received snapshot copy; no full reconnect/recovery or unrelated resource-failure matrix.'}
    Write-Utf8NoBom -Path (Join-Path $runRoot 'summary.json') -Text (($summary | ConvertTo-Json -Depth 7)+[Environment]::NewLine)
}
if (-not $pass) { throw ('Fusion resource smoke failed: '+$errorText) }
Write-Output ('Fusion resource smoke Passed; artifacts: '+$runRoot)
