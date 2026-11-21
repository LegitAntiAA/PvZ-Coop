#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateRange(60, 600)][int]$TimeoutSeconds = 150,
    [ValidateRange(0, 65535)][int]$Port = 0,
    [uint32]$Seed = 424242
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed `
    -HostWindowWidth 800 -ClientWindowWidth 800

$bookRepo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe = Join-Path $bookRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$bookGameDirectory = Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot = Join-Path $bookRepo 'work\fusion\mode-book' }
$bookRunId = [Guid]::NewGuid().ToString('N')
$bookRunRoot = Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$bookRunId)
$bookHostRoot = Join-Path $bookRunRoot 'host'
$bookClientRoot = Join-Path $bookRunRoot 'client'
$bookHostLogs = Join-Path $bookHostRoot 'logs'
$bookClientLogs = Join-Path $bookClientRoot 'logs'
$bookHostProcess = $null
$bookClientProcess = $null
$bookPassed = $false
$bookError = ''
$bookFrames = @()
$bookModeBroadcasts = 0
$bookStarted = [DateTime]::UtcNow
$bookEnvironment = @{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_ALMANAC_MODE', 'PVZ_COOP_SMOKE_FUSION_NATIVE',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY', 'PVZ_COOP_SMOKE_FUSION_ALMANAC', 'PVZ_COOP_SMOKE_FUSION_ALMANAC_UI',
    'PVZ_COOP_SMOKE_GAME_MODES', 'PVZ_COOP_SMOKE_FUSION_CASE', 'PVZ_COOP_SMOKE_FUSION_OBSERVER',
    'PVZ_COOP_SMOKE_MYSTERY', 'PVZ_COOP_SMOKE_MYSTERY_FAULT', 'PVZ_COOP_SMOKE_NATIVE_UI',
    'PVZ_COOP_SMOKE_AVATAR_EDITOR', 'PVZ_COOP_SMOKE_ONLINE_ENDPOINT')) {
    $bookEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

function Assert-BookResult {
    param([object]$Result, [string]$Role)
    if ([string]$Result.status -ne 'pass' -or [string]$Result.role -ne $Role -or
        [string]$Result.mode -ne 'fusion_validation' -or [string]$Result.scenario -ne 'fusion_validation' -or
        [int]$Result.level -ne 1 -or [uint32]$Result.seed -ne $Seed -or [bool]$Result.skipIntro -or [int]$Result.protocol -le 0 -or
        [int]$Result.snapshotsSent -ne 0 -or [int]$Result.snapshotsReceived -ne 0 -or [int]$Result.snapshotsApplied -ne 0) {
        throw ('Invalid no-Board menu result/manifest identity: '+$Role)
    }
    return [int]$Result.protocol
}

function Assert-BookFrames {
    param([string]$Directory, [string]$Role)
    Add-Type -AssemblyName System.Drawing
    $names = @('fusion_menu', 'classic_first_menu', 'fusion_return_menu', 'classic_final_menu')
    if ($Role -eq 'host') { $names += 'host_peer_departed' }
    else { $names += @('fusion_first_book', 'classic_book', 'fusion_second_book', 'classic_before_restore', 'fusion_restored_menu', 'fusion_restored_book') }
    foreach ($name in $names) {
        $path = Join-Path $Directory ('fusion_mode_book_'+$name+'.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual mode/book renderer frame: '+$path) }
        $bitmap = [Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne 800 -or $bitmap.Height -ne 600) { throw ('Mode/book frame does not match its requested800x600 viewport: '+$path) }
            $colors = [Collections.Generic.HashSet[int]]::new()
            for ($x = 16; $x -lt $bitmap.Width; $x += 37) {
                for ($y = 16; $y -lt $bitmap.Height; $y += 31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) }
            }
            if ($colors.Count -lt 8) { throw ('Mode/book renderer frame is blank/flat: '+$path) }
            [ordered]@{ role=$Role; name=$name; path=$path; width=$bitmap.Width; height=$bitmap.Height;
                sampled_colors=$colors.Count; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
        }
        finally { $bitmap.Dispose() }
    }
}

try {
    foreach ($resource in @('PlantsVsZombies.exe', 'main.pak', 'dependency.pak', 'SDL3.dll',
        'fusion\almanac\portraits\manifest.json', 'fusion\almanac\fonts\source_cjk.ttf')) {
        $path = if ($resource -eq 'PlantsVsZombies.exe') { $GameExe } else { Join-Path $bookGameDirectory $resource }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Mode/book runtime resource missing: '+$path) }
    }
    foreach ($directory in @($bookHostLogs,$bookClientLogs,(Join-Path $bookHostRoot 'appdata'),(Join-Path $bookClientRoot 'appdata'))) { Ensure-Directory $directory }
    if ($Port -eq 0) { $Port = Get-FreeLoopbackPort }
    foreach ($name in $bookEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_ALMANAC_MODE = '1'
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE = '1'
    foreach ($role in @('host','client')) {
        $root = if ($role -eq 'host') { $bookHostRoot } else { $bookClientRoot }
        $nickname = if ($role -eq 'host') { 'BookHost' } else { 'BookClient' }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth 800 -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') `
            -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 1 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Mode/book regression: two owned menu peers, no Board; '+$bookRunRoot)
    $bookHostProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $bookGameDirectory `
        -ManifestPath (Join-Path $bookHostRoot 'smoke.ini') -Role host -RunId $bookRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $bookRunRoot 'pids.json') -Executable $GameExe -HostProcess $bookHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $bookHostLogs -Pattern ('(?im)host listening port='+$Port+'\b') -Process $bookHostProcess -Role host -TimeoutSeconds 120
    $bookClientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $bookGameDirectory `
        -ManifestPath (Join-Path $bookClientRoot 'smoke.ini') -Role client -RunId $bookRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $bookRunRoot 'pids.json') -Executable $GameExe -HostProcess $bookHostProcess -ClientProcess $bookClientProcess
    $results = @(Wait-ForResults -HostResultPath (Join-Path $bookHostRoot 'result.json') -ClientResultPath (Join-Path $bookClientRoot 'result.json') `
        -HostProcess $bookHostProcess -ClientProcess $bookClientProcess -HostLogDirectory $bookHostLogs -ClientLogDirectory $bookClientLogs -TimeoutSeconds $TimeoutSeconds)
    $protocols = @((Assert-BookResult $results[0] host),(Assert-BookResult $results[1] client))
    if ($protocols[0] -ne $protocols[1]) { throw 'Mode/book peer protocols differ.' }
    $hostText = Get-CombinedLogText $bookHostLogs
    $clientText = Get-CombinedLogText $bookClientLogs
    $bookModeBroadcasts = [regex]::Matches($hostText,'content mode broadcast mode=(Classic|Fusion)\b').Count
    foreach ($role in @('host','client')) {
        $text = if ($role -eq 'host') { $hostText } else { $clientText }
        if ($text -match '(?im)\bSMOKE_FAIL\b|Access Violation|unhandled exception|assertion failed|host state rejected|reason=sequence|START_GAME decoded|start game broadcast|SMOKE_SNAPSHOT_(TX|RX|APPLIED)') {
            throw ('Mode/book fixture contains a failure or unexpected gameplay/network state: '+$role)
        }
        if ($text -notmatch ('SMOKE_FUSION_MODE_BOOK_HANDSHAKE role='+$role+' slot='+$(if($role -eq 'host'){0}else{1})+' mode=Fusion players=2 realConnection=1 noBoard=1') -or
            $text -notmatch ('SMOKE_FUSION_MODE_BOOK_COMPLETE role='+$role+' transitions=3 bothProviders=1 remoteClosure=1 disconnectRestore=1 noBoard=1 snapshots=0 gameplayNotTested=1')) {
            throw ('Missing real connection/completion proof: '+$role)
        }
        foreach ($transition in @(1,2,3)) {
            $mode = if ($transition -eq 2) { 'Fusion' } else { 'Classic' }
            if ($text -notmatch ('SMOKE_FUSION_MODE_BOOK_CLOSED role='+$role+' transition='+$transition+' mode='+$mode+' book=0 dialogs=0 freshMs=350 noBoard=1')) {
                throw ('Open Almanac closure/fresh menu frame not proved: '+$role+'/'+$transition)
            }
        }
    }
    if ($clientText -notmatch 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion' -or
        [regex]::Matches($hostText,'content mode broadcast mode=Classic\b').Count -ne 2 -or
        [regex]::Matches($hostText,'content mode broadcast mode=Fusion\b').Count -ne 1) {
        throw 'Missing genuine Fusion handshake or the three normal mode broadcasts.'
    }
    foreach ($round in @(1,2,3,4)) {
        $mode = if ($round -in @(1,3)) { 'Fusion' } else { 'Classic' }
        $id = if ($mode -eq 'Fusion') { 1000 } else { 1 }
        if ($clientText -notmatch ('SMOKE_FUSION_MODE_BOOK_OPEN role=client round='+$round+' mode='+$mode+' selected='+$id+' provider='+$mode+' noBoard=1 publicApi=1')) {
            throw ('Fresh native Almanac provider/selection not established: '+$round)
        }
    }
    if ($clientText -notmatch 'SMOKE_FUSION_MODE_BOOK_RESTORE role=client before=Classic after=Fusion book=0 originalMode=Fusion realDisconnect=1 actualProfileRestore=1 noBoard=1' -or
        $clientText -notmatch 'SMOKE_FUSION_MODE_BOOK_REOPEN role=client mode=Fusion selected=1000 provider=Fusion afterRestore=1 noBoard=1' -or
        $clientText -notmatch 'local content mode restored mode=1\b' -or
        $hostText -notmatch 'SMOKE_FUSION_MODE_BOOK_DEPART role=host players=1 mode=Classic realDisconnect=1 noBoard=1') {
        throw 'Missing real disconnect, provider closure, original-mode restoration or fresh reopened book.'
    }
    $bookFrames = @((Assert-BookFrames $bookHostRoot host),(Assert-BookFrames $bookClientRoot client))
    $bookPassed = $true
}
catch { $bookError=$_.Exception.Message; if($_.ScriptStackTrace){$bookError+=' | '+$_.ScriptStackTrace}; Write-SmokeStatus ('FAIL mode/book: '+$bookError) }
finally {
    foreach($name in $bookEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$bookEnvironment[$name],'Process')}
    $clientStopped = Stop-OwnedProcess -Process $bookClientProcess -Role client
    $hostStopped = Stop-OwnedProcess -Process $bookHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$bookPassed=$false;$bookError+=' Owned cleanup failed.'}
    Ensure-Directory $bookRunRoot
    $summary = [ordered]@{status=$(if($bookPassed){'pass'}else{'fail'});fixture='FusionAlmanacMode';run_id=$bookRunId;
        started_utc=$bookStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;
        requested_transitions=@('Fusion->Classic','Classic->Fusion','Fusion->Classic');requested_disconnect_restore='Classic->originalFusion';
        observed_mode_broadcasts=$bookModeBroadcasts;no_board_verified=$bookPassed;disconnect_restore_verified=$bookPassed;
        gameplay_and_snapshot_tests='Not executed';frames=$bookFrames;
        owned_cleanup=[bool]($clientStopped -and $hostStopped);error=$bookError;artifacts=$bookRunRoot}
    Write-Utf8NoBom -Path (Join-Path $bookRunRoot 'summary.json') -Text (($summary | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if($bookPassed){Write-SmokeStatus ('PASS mode/book; '+$bookRunRoot);exit 0}
exit 1
