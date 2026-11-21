#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Fusion', 'GameModes')][string]$Fixture = 'Fusion',
    [ValidateSet('Manual', 'Mystery', 'Observer')][string]$Variant = 'Manual',
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateRange(30, 3600)][int]$TimeoutSeconds = 180,
    [ValidateRange(0, 65535)][int]$Port = 0,
    [uint32]$Seed = 424242
)

$ErrorActionPreference = 'Stop'
# Reuse only process/config/log/ownership helpers. Generic scenario assertions
# remain unchanged: these fixtures establish their own native completion proof.
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed

$fusionRepoRoot = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe = Join-Path $fusionRepoRoot 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$fusionGameDirectory = Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot = Join-Path $fusionRepoRoot 'work\fusion-native' }
$ArtifactsRoot = Get-NormalizedPath $ArtifactsRoot
$fusionMode = if ($Fixture -eq 'Fusion') { 'fusion_validation' } else { 'adventure' }
$fusionScenario = if ($Fixture -eq 'Fusion') { 'fusion_validation' } else { 'adventure1' }
$fusionCompletion = if ($Fixture -eq 'GameModes') { 'SMOKE_GAMEMODES_COMPLETE' } elseif ($Variant -eq 'Mystery') {
    'SMOKE_FUSION_MYSTERY_COMPLETE'
} elseif ($Variant -eq 'Observer') { 'SMOKE_FUSION_OBSERVER_PAIR_COMPLETE' } else { 'SMOKE_FUSION_COMPLETE' }
$fusionRunId = [Guid]::NewGuid().ToString('N')
$fusionRunRoot = Join-Path $ArtifactsRoot ('run_' + $fusionRunId)
$fusionHostRoot = Join-Path $fusionRunRoot 'host'
$fusionClientRoot = Join-Path $fusionRunRoot 'client'
$fusionObserverRoot = Join-Path $fusionRunRoot 'observer'
$fusionHostLog = Join-Path $fusionHostRoot 'logs'
$fusionClientLog = Join-Path $fusionClientRoot 'logs'
$fusionObserverLog = Join-Path $fusionObserverRoot 'logs'
$fusionHostResult = Join-Path $fusionHostRoot 'result.json'
$fusionClientResult = Join-Path $fusionClientRoot 'result.json'
$fusionObserverResult = Join-Path $fusionObserverRoot 'result.json'
$fusionHostProcess = $null
$fusionClientProcess = $null
$fusionObserverProcess = $null
$fusionPassed = $false
$fusionError = ''
$fusionHashes = @()
$fusionObserverHashes = @()
$fusionProtocols = @()
$fusionStarted = [DateTime]::UtcNow
$fusionOldModes = $env:PVZ_COOP_SMOKE_GAME_MODES
$fusionOldCase = $env:PVZ_COOP_SMOKE_FUSION_CASE
$fusionOldObserver = $env:PVZ_COOP_SMOKE_FUSION_OBSERVER

function Assert-FixtureResult {
    param([object]$Result, [string]$Role)
    if ([string]$Result.status -ne 'pass' -or [string]$Result.role -ne $Role -or
        [string]$Result.mode -ne $fusionMode -or [string]$Result.scenario -ne $fusionScenario -or
        [int]$Result.level -ne 1 -or [uint32]$Result.seed -ne $Seed -or [bool]$Result.skipIntro -or [int]$Result.protocol -le 0) {
        throw ('Invalid ' + $Role + ' fixture result or manifest identity.')
    }
    $count = if ($Role -eq 'host') { [int]$Result.snapshotsSent } else { [int]$Result.snapshotsApplied }
    if ($count -lt 8) { throw ('Fewer than eight real ' + $Role + ' snapshot observations.') }
    return [int]$Result.protocol
}

function Assert-FixtureFrames {
    param([string]$Directory, [string]$Role = 'pair')
    Add-Type -AssemblyName System.Drawing
    $names = if ($Role -eq 'observer') {
        @('fusion_observer_join', 'fusion_observer_readonly', 'fusion_observer_after_reject')
    } elseif ($Fixture -eq 'Fusion' -and $Variant -eq 'Mystery') {
        @('fusion_chooser', 'fusion_picks', 'fusion_intro', 'fusion_playing', 'fusion_peashooter', 'fusion_fused',
            'fusion_mystery_chooser', 'fusion_mystery_pending', 'fusion_mystery_intro', 'fusion_mystery_revealed', 'fusion_mystery_purchase')
    } elseif ($Fixture -eq 'Fusion' -and $Variant -eq 'Observer') {
        @('fusion_chooser', 'fusion_picks', 'fusion_intro', 'fusion_playing', 'fusion_peashooter', 'fusion_fused', 'fusion_observer_pair')
    } elseif ($Fixture -eq 'Fusion') {
        @('fusion_chooser', 'fusion_picks', 'fusion_intro', 'fusion_playing', 'fusion_peashooter', 'fusion_fused', 'fusion_headless', 'fusion_complete')
    } else {
        @('gamemodes_menu', 'gamemodes_open', 'gamemodes_fusion_guard', 'gamemodes_keyboard_guard', 'gamemodes_closed', 'gamemodes_classic_started')
    }
    if ($Fixture -eq 'Fusion' -and $Variant -eq 'Manual' -and $Directory -eq $fusionHostRoot) {
        $names += @('fusion_shovel_held', 'fusion_shovel_reselected')
    }
    foreach ($name in $names) {
        $path = Join-Path $Directory ($name + '.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual renderer frame: ' + $path) }
        $bitmap = [Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -lt 800 -or $bitmap.Height -lt 450) { throw ('Renderer frame is unexpectedly small: ' + $path) }
            $colors = New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x = 16; $x -lt $bitmap.Width; $x += 37) {
                for ($y = 16; $y -lt $bitmap.Height; $y += 31) { [void]$colors.Add($bitmap.GetPixel($x, $y).ToArgb()) }
            }
            if ($colors.Count -lt 8) { throw ('Renderer frame is blank or flat: ' + $path) }
        }
        finally { $bitmap.Dispose() }
    }
}

function Wait-ObserverFixtureResults {
    $participants = @(
        @{ Name = 'host'; Process = $fusionHostProcess; Result = $fusionHostResult; Logs = $fusionHostLog },
        @{ Name = 'client'; Process = $fusionClientProcess; Result = $fusionClientResult; Logs = $fusionClientLog },
        @{ Name = 'observer'; Process = $fusionObserverProcess; Result = $fusionObserverResult; Logs = $fusionObserverLog }
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $complete = @{}
        foreach ($entry in $participants) {
            $result = Read-JsonIfComplete $entry.Result
            if ($null -ne $result) {
                if ([string]$result.status -ne 'pass') { throw ($entry.Name + ' fixture failed: ' + [string]$result.error) }
                $complete[$entry.Name] = $result
            } elseif ($entry.Process.HasExited) { throw ($entry.Name + ' process exited without its fixture result.') }
            $text = Get-CombinedLogText $entry.Logs
            if ($text -match '(?im)\bSMOKE_FAIL\b|Access Violation|unhandled exception|assertion failed') { throw ($entry.Name + ' log contains a fixture failure.') }
        }
        if ($complete.Count -eq 3) { return $complete }
        Start-Sleep -Milliseconds 200
    }
    throw 'Timed out waiting for the three owned Fusion participants.'
}

try {
    if ($Fixture -ne 'Fusion' -and $Variant -ne 'Manual') { throw 'Mystery/Observer variants require the Fusion fixture.' }
    if (-not (Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw ('Game executable missing: ' + $GameExe) }
    foreach ($resource in @('main.pak', 'dependency.pak', 'SDL3.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $fusionGameDirectory $resource) -PathType Leaf)) { throw ('Missing runtime resource: ' + $resource) }
    }
    if ($Fixture -eq 'Fusion' -and -not (Test-Path -LiteralPath (Join-Path $fusionGameDirectory 'fusion\pilot\background1\animation.json') -PathType Leaf)) {
        throw 'Executable-relative Fusion pilot resources are missing.'
    }
    foreach ($directory in @($fusionHostLog, $fusionClientLog, (Join-Path $fusionHostRoot 'appdata'), (Join-Path $fusionClientRoot 'appdata'))) { Ensure-Directory $directory }
    if ($Port -eq 0) { $Port = Get-FreeLoopbackPort }
    if ($Fixture -eq 'GameModes') { $env:PVZ_COOP_SMOKE_GAME_MODES = '1' } else { $env:PVZ_COOP_SMOKE_GAME_MODES = $null }
    $env:PVZ_COOP_SMOKE_FUSION_CASE = if ($Variant -eq 'Manual') { $null } else { $Variant.ToLowerInvariant() }
    $env:PVZ_COOP_SMOKE_FUSION_OBSERVER = $null
    foreach ($role in @('host', 'client')) {
        $root = if ($role -eq 'host') { $fusionHostRoot } else { $fusionClientRoot }
        $width = if ($role -eq 'host') { 1066 } else { 800 }
        $nickname = if ($role -eq 'host') { 'FusionHost' } else { 'FusionClient' }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') `
            -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode $fusionMode -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds * 1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    if ($Variant -eq 'Observer') {
        Ensure-Directory $fusionObserverLog
        Ensure-Directory (Join-Path $fusionObserverRoot 'appdata')
        Write-CoopConfig -Path (Join-Path $fusionObserverRoot 'coop.ini') -Nickname FusionObserver -WindowWidth 800 -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $fusionObserverRoot 'smoke.ini') -Role client -Nickname FusionObserver `
            -AppDataDirectory (Join-Path $fusionObserverRoot 'appdata') -ConfigPath (Join-Path $fusionObserverRoot 'coop.ini') `
            -LogDirectory $fusionObserverLog -ResultPath $fusionObserverResult -SelectedPort $Port -SelectedMode $fusionMode `
            -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds * 1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Fixture ' + $Fixture + '; owned hidden host1066/client800; artifacts: ' + $fusionRunRoot)
    $fusionHostProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $fusionGameDirectory `
        -ManifestPath (Join-Path $fusionHostRoot 'smoke.ini') -Role host -RunId $fusionRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $fusionRunRoot 'pids.json') -Executable $GameExe -HostProcess $fusionHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $fusionHostLog -Pattern ('(?im)host listening port=' + $Port + '\b') `
        -Process $fusionHostProcess -Role host -TimeoutSeconds 120
    $fusionClientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $fusionGameDirectory `
        -ManifestPath (Join-Path $fusionClientRoot 'smoke.ini') -Role client -RunId $fusionRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $fusionRunRoot 'pids.json') -Executable $GameExe -HostProcess $fusionHostProcess -ClientProcess $fusionClientProcess
    if ($Variant -eq 'Observer') {
        Wait-ForLogPattern -LogDirectory $fusionHostLog -Pattern '(?im)SMOKE_FUSION_OBSERVER_WAIT role=host\b' `
            -Process $fusionHostProcess -Role host -TimeoutSeconds $TimeoutSeconds
        $env:PVZ_COOP_SMOKE_FUSION_OBSERVER = '1'
        $fusionObserverProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $fusionGameDirectory `
            -ManifestPath (Join-Path $fusionObserverRoot 'smoke.ini') -Role client -RunId $fusionRunId -EnableUnicodeChat $false
        $env:PVZ_COOP_SMOKE_FUSION_OBSERVER = $null
        $ledger = @()
        foreach ($entry in @(@{ Role = 'host'; Process = $fusionHostProcess }, @{ Role = 'client'; Process = $fusionClientProcess }, @{ Role = 'observer'; Process = $fusionObserverProcess })) {
            $ledger += [ordered]@{ role = $entry.Role; pid = $entry.Process.Id; executable = $GameExe; started_utc = $entry.Process.StartTime.ToUniversalTime().ToString('o') }
        }
        Write-Utf8NoBom -Path (Join-Path $fusionRunRoot 'pids.json') -Text ((ConvertTo-Json -InputObject @($ledger) -Depth 4) + [Environment]::NewLine)
        $three = Wait-ObserverFixtureResults
        $results = @($three.host, $three.client)
        $observerResult = $three.observer
    } else {
        $results = @(Wait-ForResults -HostResultPath $fusionHostResult -ClientResultPath $fusionClientResult `
            -HostProcess $fusionHostProcess -ClientProcess $fusionClientProcess -HostLogDirectory $fusionHostLog `
            -ClientLogDirectory $fusionClientLog -TimeoutSeconds $TimeoutSeconds)
    }
    $fusionProtocols = @((Assert-FixtureResult $results[0] host), (Assert-FixtureResult $results[1] client))
    if ($fusionProtocols[0] -ne $fusionProtocols[1]) { throw 'Fixture peer protocols differ.' }
    $hostText = Get-CombinedLogText $fusionHostLog
    $clientText = Get-CombinedLogText $fusionClientLog
    foreach ($text in @($hostText, $clientText)) {
        if ($text -notmatch ('(?im)\b' + $fusionCompletion + '\b')) { throw ('Missing native completion: ' + $fusionCompletion) }
        if ($text -match '(?im)\bSMOKE_FAIL\b|Access Violation|unhandled exception|assertion failed|Fusion room rejected|Fusion.*reason=sequence') {
            throw 'Fixture contains a failure, rejected room or sequence gap.'
        }
        if ($Fixture -eq 'Fusion') {
            $markers = @('SMOKE_FUSION_CHOOSER', 'SMOKE_FUSION_INTRO', 'SMOKE_FUSION_PLAYING', 'SMOKE_FUSION_PLANT', 'SMOKE_FUSION_FUSION')
            if ($Variant -eq 'Manual') { $markers += @('SMOKE_FUSION_STALE', 'SMOKE_FUSION_COLLECT_SINGLE', 'SMOKE_FUSION_SHOVEL') }
            if ($Variant -eq 'Mystery') { $markers += @('SMOKE_FUSION_MYSTERY_PENDING', 'SMOKE_FUSION_MYSTERY_INTRO', 'SMOKE_FUSION_MYSTERY_REVEALED', 'SMOKE_FUSION_MYSTERY_PURCHASE') }
            if ($Variant -eq 'Observer') { $markers += @('SMOKE_FUSION_OBSERVER_PAIR') }
            foreach ($marker in $markers) {
                if ($text -notmatch ('(?im)\b' + $marker + '\b')) { throw ('Missing observed Fusion checkpoint: ' + $marker) }
            }
            if ($Variant -eq 'Manual') {
                foreach ($eventName in @('bullet8', 'sun5', 'headless', 'death')) {
                    if ($text -notmatch ('(?im)SMOKE_FUSION_COMBAT[^\r\n]*event=' + $eventName + '\b')) { throw ('Missing observed source combat event: ' + $eventName) }
                }
            }
        }
    }
    if ($Fixture -eq 'Fusion') {
        $rules = @('(?im)Fusion command slot=1 action=5 sequence=1 applied=1\b')
        if ($Variant -eq 'Manual') { $rules += @('(?im)Fusion command slot=1 action=5 sequence=2 applied=0\b',
            '(?im)Fusion command slot=1 action=7 sequence=3 applied=1\b', '(?im)Fusion command slot=1 action=7 sequence=4 applied=0\b',
            '(?im)SMOKE_FUSION_SHOVEL_CANCEL role=host escape=1 right=1 worldUntouched=1 nativeSDL=1\b') }
        foreach ($rule in $rules) {
            if ($hostText -notmatch $rule) { throw ('Missing host-authoritative command outcome: ' + $rule) }
        }
        $identityPatterns = @(
            '(?im)SMOKE_FUSION_PLANT[^\r\n]*id=(\d+) kind=0 sun=400',
            '(?im)SMOKE_FUSION_FUSION[^\r\n]*old=(\d+) id=(\d+) kind=1000 parents=0,1 hp=300 sun=300'
        )
        if ($Variant -eq 'Manual') { $identityPatterns += @('(?im)SMOKE_FUSION_ZOMBIE[^\r\n]*id=(\d+) row=2',
            '(?im)SMOKE_FUSION_COLLECT_SINGLE[^\r\n]*id=(\d+) amount=25 before=250 after=275') }
        if ($Variant -eq 'Mystery') { $identityPatterns += @(
            '(?im)SMOKE_FUSION_MYSTERY_PENDING[^\r\n]*epoch=(\d+) packets=(\d+),(\d+) kind=NONE extra=0 price=0 recharge=0 origin=1',
            '(?im)SMOKE_FUSION_MYSTERY_REVEALED[^\r\n]*epoch=(\d+) first=(\d+) kind=(\d+) extra=(\d+) price=(\d+) second=(\d+) kind2=(\d+) extra2=(\d+) origin=1 unique=1',
            '(?im)SMOKE_FUSION_MYSTERY_PURCHASE[^\r\n]*epoch=(\d+) packet=(\d+) id=(\d+) kind=(\d+) extra=(\d+) origin=1 price=(\d+) before=500 after=(\d+) recharge=1 independent=1 nativeSDL=1'
        ) }
        foreach ($pattern in $identityPatterns) {
            $a = [regex]::Match($hostText, $pattern); $b = [regex]::Match($clientText, $pattern)
            if (-not $a.Success -or -not $b.Success) { throw ('Missing shared identity proof: ' + $pattern) }
            for ($i = 1; $i -lt $a.Groups.Count; ++$i) { if ($a.Groups[$i].Value -ne $b.Groups[$i].Value) { throw 'Host/client canonical identities differ.' } }
        }
    }
    $hashHostText = $hostText; $hashClientText = $clientText
    if ($Variant -eq 'Mystery') {
        $restartHost = [regex]::Match($hostText, '(?im)Fusion start host epoch=(\d+)[^\r\n]*activity=validation', [Text.RegularExpressions.RegexOptions]::RightToLeft)
        $restartClient = [regex]::Match($clientText, '(?im)Fusion start client epoch=(\d+)[^\r\n]*activity=validation', [Text.RegularExpressions.RegexOptions]::RightToLeft)
        if (-not $restartHost.Success -or -not $restartClient.Success -or [int]$restartHost.Groups[1].Value -le 1 -or
            $restartHost.Groups[1].Value -ne $restartClient.Groups[1].Value) { throw 'Mystery did not restart the real activity with a shared newer epoch.' }
        $hashHostText = $hostText.Substring($restartHost.Index); $hashClientText = $clientText.Substring($restartClient.Index)
        if ($hashHostText -notmatch '(?im)Fusion command slot=1 action=5 sequence=1 applied=1\b') { throw 'Mystery second-room real client purchase sequence1 was not applied.' }
        foreach ($text in @($hostText, $clientText)) {
            $complete = [regex]::Match($text, '(?im)SMOKE_FUSION_MYSTERY_COMPLETE[^\r\n]*snapshotsAfterRestart=(\d+)')
            if (-not $complete.Success -or [int]$complete.Groups[1].Value -lt 8) { throw 'Mystery second activity lacks eight real snapshot observations.' }
            if ($text -notmatch '(?im)SMOKE_FUSION_MYSTERY_INTRO[^\r\n]*pending=2 kind=NONE extra=0 price=0 recharge=0\b') { throw 'Mystery Intro privacy was not observed.' }
        }
        $purchase = [regex]::Match($hostText, '(?im)SMOKE_FUSION_MYSTERY_PURCHASE[^\r\n]*kind=(\d+) extra=(\d+) origin=1 price=(\d+) before=500 after=(\d+)')
        $kind = [int]$purchase.Groups[1].Value; $extra = [int]$purchase.Groups[2].Value
        if ($kind -notin @(0, 1)) { throw 'Mystery selected an ineligible source card.' }
        $baseCost = if ($kind -eq 0) { 100 } else { 50 }
        if ($extra) { $baseCost *= 2 }
        $expectedPrice = [int][Math]::Floor(($baseCost * 93 + 99) / 100)
        if ([int]$purchase.Groups[3].Value -ne $expectedPrice -or [int]$purchase.Groups[4].Value -ne 500 - $expectedPrice) { throw 'Mystery D1 purchase did not use the canonical seven-percent discount.' }
    }
    if ($Variant -eq 'Observer') {
        $observerProtocol = Assert-FixtureResult $observerResult client
        if ($observerProtocol -ne $fusionProtocols[0]) { throw 'Observer protocol differs from the original pair.' }
        $observerText = Get-CombinedLogText $fusionObserverLog
        if ($hostText -notmatch '(?im)Fusion late join sent slot=2 readonly=1\b' -or $observerText -notmatch '(?im)SMOKE_FUSION_OBSERVER_COMPLETE role=observer slot=2\b' -or
            $observerText -match '(?im)RX client type=START_GAME\b|client START_GAME decoded|\bSMOKE_FAIL\b|Fusion room rejected') { throw 'Observer did not use the separate Fusion latejoin path cleanly.' }
        $hostIdentity = [regex]::Match($hostText, '(?im)SMOKE_FUSION_OBSERVER_PAIR role=host epoch=(\d+) id=(\d+) parents=0,1 hp=300 sun=300 packets=4 active=3 readonly=1')
        $clientIdentity = [regex]::Match($clientText, '(?im)SMOKE_FUSION_OBSERVER_PAIR role=client epoch=(\d+) id=(\d+) parents=0,1 hp=300 sun=300 packets=4 active=3 readonly=1')
        $observerIdentity = [regex]::Match($observerText, '(?im)SMOKE_FUSION_OBSERVER_STATE role=observer slot=2 epoch=(\d+) id=(\d+) parents=0,1 hp=300 sun=300 packets=4 active=3 readonly=1')
        if (-not $hostIdentity.Success -or -not $clientIdentity.Success -or -not $observerIdentity.Success) { throw 'Missing complete latejoin world/bank observation.' }
        foreach ($group in @(1, 2)) {
            if ($hostIdentity.Groups[$group].Value -ne $clientIdentity.Groups[$group].Value -or $hostIdentity.Groups[$group].Value -ne $observerIdentity.Groups[$group].Value) { throw 'Late observer received different epoch or fused entity identity.' }
        }
        $reject = '(?im)SMOKE_FUSION_SPECTATOR_REJECT slot=2 action=6 epoch=' + $hostIdentity.Groups[1].Value + ' target=' + $hostIdentity.Groups[2].Value + '\b'
        if ($hostText -notmatch $reject -or $observerText -notmatch '(?im)SMOKE_FUSION_OBSERVER_READONLY[^\r\n]*nativeSDL=1 localSubmitRejected=1\b' -or
            $observerText -notmatch '(?im)SMOKE_FUSION_OBSERVER_FORGED[^\r\n]*action=6\b') { throw 'Observer mutation denial lacks actual framed host-authority evidence.' }
        $observerApplied = @(Get-SnapshotHashes -Text $observerText -Marker SMOKE_SNAPSHOT_APPLIED)
        $hostTransmitted = @(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX)
        $fusionObserverHashes = @($observerApplied | Where-Object { $hostTransmitted -contains $_ } | Select-Object -Unique)
        if (-not $fusionObserverHashes.Count) { throw 'Observer has no real applied room hash matching host transmission.' }
        Assert-FixtureFrames $fusionObserverRoot observer
    }
    $tx = @(Get-SnapshotHashes -Text $hashHostText -Marker SMOKE_SNAPSHOT_TX)
    $applied = @(Get-SnapshotHashes -Text $hashClientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $fusionHashes = @($applied | Where-Object { $tx -contains $_ } | Select-Object -Unique)
    if (-not $fusionHashes.Count) { throw 'No real client-applied room hash matches host transmission.' }
    Assert-FixtureFrames $fusionHostRoot
    Assert-FixtureFrames $fusionClientRoot
    $fusionPassed = $true
    Write-SmokeStatus ('PASS ' + $Fixture + ': protocol ' + $fusionProtocols[0] + ', common room hash ' + $fusionHashes[0])
}
catch {
    $fusionError = $_.Exception.Message
    if ($_.ScriptStackTrace) { $fusionError += ' | ' + $_.ScriptStackTrace }
    Write-SmokeStatus ('FAIL ' + $Fixture + ': ' + $fusionError)
}
finally {
    $env:PVZ_COOP_SMOKE_GAME_MODES = $fusionOldModes
    $env:PVZ_COOP_SMOKE_FUSION_CASE = $fusionOldCase
    $env:PVZ_COOP_SMOKE_FUSION_OBSERVER = $fusionOldObserver
    $observerStopped = Stop-OwnedProcess -Process $fusionObserverProcess -Role observer
    $clientStopped = Stop-OwnedProcess -Process $fusionClientProcess -Role client
    $hostStopped = Stop-OwnedProcess -Process $fusionHostProcess -Role host
    if (-not $observerStopped -or -not $clientStopped -or -not $hostStopped) { $fusionPassed = $false; $fusionError += ' Owned process cleanup failed.' }
    Ensure-Directory $fusionRunRoot
    $summary = [ordered]@{
        status = $(if ($fusionPassed) { 'pass' } else { 'fail' })
        fixture = $Fixture
        variant = $Variant
        run_id = $fusionRunId
        started_utc = $fusionStarted.ToString('o')
        finished_utc = [DateTime]::UtcNow.ToString('o')
        executable = $GameExe
        mode = $fusionMode
        level = 1
        seed = $Seed
        host_window = '1066x600'
        client_window = '800x600'
        common_snapshot_hashes = $fusionHashes
        observer_common_snapshot_hashes = $fusionObserverHashes
        source_adventure_complete = $false
        owned_cleanup = [bool]($observerStopped -and $clientStopped -and $hostStopped)
        error = $fusionError
        artifacts_root = $fusionRunRoot
    }
    Write-Utf8NoBom -Path (Join-Path $fusionRunRoot 'summary.json') -Text (($summary | ConvertTo-Json -Depth 5) + [Environment]::NewLine)
}
if ($fusionPassed) { exit 0 } else { exit 1 }
