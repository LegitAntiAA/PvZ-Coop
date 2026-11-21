#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateSet(1, 10)][int]$Difficulty = 10,
    [ValidateSet('None', 'RngFailure')][string]$Fault = 'None',
    [ValidateRange(60, 600)][int]$TimeoutSeconds = 180,
    [ValidateRange(0, 65535)][int]$Port = 0,
    [uint32]$Seed = 424242
)

# Native shared Board/Plant/SeedPacket workflow. This runner never opts into the
# superseded FusionRuntime/independent world or the Classic mystery fixture.
# Run separately: .\RunFusionNativeMysterySmoke.ps1 -Difficulty 1 (then 10).
# Recovery case: -Difficulty 10 -Fault RngFailure (two complete native intros).
# Root must register NetSmokeUpdateNativeFusionMystery before the manual hook.
$mysteryDifficulty = $Difficulty
$mysteryFault = $Fault
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed
$mysteryRepo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe = Join-Path $mysteryRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$mysteryGameDirectory = Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot = Join-Path $mysteryRepo 'work\fusion-native-mystery' }
$mysteryRunId = [Guid]::NewGuid().ToString('N')
$mysteryRunRoot = Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_' + $mysteryRunId)
$mysteryHostRoot = Join-Path $mysteryRunRoot 'host'
$mysteryClientRoot = Join-Path $mysteryRunRoot 'client'
$mysteryHostLogs = Join-Path $mysteryHostRoot 'logs'
$mysteryClientLogs = Join-Path $mysteryClientRoot 'logs'
$mysteryHostProcess = $null
$mysteryClientProcess = $null
$mysteryPassed = $false
$mysteryError = ''
$mysteryCommon = @()
$mysteryPostPurchase = @()
$mysteryFrames = @()
$mysteryRecovery = $null
$mysteryStarted = [DateTime]::UtcNow
$mysteryEnvironment = @{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE', 'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY', 'PVZ_COOP_SMOKE_GAME_MODES',
    'PVZ_COOP_SMOKE_FUSION_CASE', 'PVZ_COOP_SMOKE_FUSION_OBSERVER', 'PVZ_COOP_SMOKE_MYSTERY',
    'PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_SPECTATOR', 'PVZ_COOP_SMOKE_SPECTATOR_ROLE')) {
    $mysteryEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

function Assert-NativeMysteryResult {
    param([object]$Result, [string]$Role)
    if ([string]$Result.status -ne 'pass' -or [string]$Result.role -ne $Role -or
        [string]$Result.mode -ne 'fusion_validation' -or [string]$Result.scenario -ne 'fusion_validation' -or
        [int]$Result.level -ne 1 -or [uint32]$Result.seed -ne $Seed -or [bool]$Result.skipIntro -or [int]$Result.protocol -le 0) {
        throw ('Invalid native mystery result/manifest identity: ' + $Role)
    }
    $samples = if ($Role -eq 'host') { [int]$Result.snapshotsSent } else { [int]$Result.snapshotsApplied }
    if ($samples -lt 8) { throw ('Native mystery has fewer than eight snapshots: ' + $Role) }
    return [int]$Result.protocol
}

function Assert-NativeMysteryFrames {
    param([string]$Directory, [string]$Role)
    Add-Type -AssemblyName System.Drawing
    $normal = @('chooser', 'pending_first', 'cancelled', 'pending_final', 'intro_secret', 'revealed', 'purchased', 'complete')
    $names = $normal
    if ($mysteryFault -eq 'RngFailure') {
        $names = @('chooser', 'pending_first', 'cancelled', 'pending_final', 'intro_secret', 'recovery_restarted') +
            @($normal | ForEach-Object { 'recovery_'+$_ })
    }
    if ($Role -eq 'client') { $names += $(if ($mysteryFault -eq 'RngFailure') { 'recovery_held' } else { 'held' }) }
    foreach ($name in $names) {
        $path = Join-Path $Directory ('fusion_native_mystery_' + $name + '.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Native mystery renderer BMP missing: ' + $path) }
        $bitmap = [Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -lt 800 -or $bitmap.Height -lt 450) { throw ('Native mystery BMP is unexpectedly small: ' + $path) }
            $colors = [Collections.Generic.HashSet[int]]::new()
            for ($x = 16; $x -lt $bitmap.Width; $x += 37) {
                for ($y = 16; $y -lt $bitmap.Height; $y += 31) { [void]$colors.Add($bitmap.GetPixel($x, $y).ToArgb()) }
            }
            if ($colors.Count -lt 8) { throw ('Native mystery BMP is blank/flat: ' + $path) }
            [ordered]@{ role = $Role; file = $path; width = $bitmap.Width; height = $bitmap.Height;
                sampled_colors = $colors.Count; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
        }
        finally { $bitmap.Dispose() }
    }
}

function Get-NativePoolEvidence {
    param([string]$Text,[string]$Role,[string]$Prefix)
    $layouts=@([regex]::Matches($Text,($Prefix+'_POOL_LAYOUT role='+$Role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) mandatory=0,1,2,4 unfiltered=1')))
    if (-not $layouts.Count) { throw ('Missing explicit live source pool: '+$Role) }
    $eligible=[int]$layouts[0].Groups[1].Value; $target=[int]$layouts[0].Groups[2].Value
    $quota0=[int]$layouts[0].Groups[3].Value; $quota1=[int]$layouts[0].Groups[4].Value
    if ($target -ne [Math]::Min(10,$eligible) -or $quota0+$quota1 -ne $target -or $quota0 -lt 4 -or $quota1 -lt 4) { throw 'Observed live source pool has invalid native bank target/mandatory baseline quotas.' }
    foreach ($layout in $layouts) {
        if ([int]$layout.Groups[1].Value -ne $eligible -or [int]$layout.Groups[2].Value -ne $target -or
            [int]$layout.Groups[3].Value -ne $quota0 -or [int]$layout.Groups[4].Value -ne $quota1) { throw 'Fixture live pool changed across the activity/recovery.' }
    }
    $byIndex=@{}; $cards=@{}
    foreach ($row in [regex]::Matches($Text,($Prefix+'_ELIGIBLE role='+$Role+' index=(\d+) kind=(\d+) extra=([01]) baseCost=(\d+) sourceCD=(\d+) maxHP=(\d+) alias=(\d+)'))) {
        $index=[int]$row.Groups[1].Value
        $signature=($row.Groups | Select-Object -Skip 2 | ForEach-Object { $_.Value }) -join ':'
        if ($byIndex.ContainsKey($index) -and $byIndex[$index].signature -ne $signature) { throw 'Source definition changed at an existing pool index.' }
        $key=$row.Groups[2].Value+':'+$row.Groups[3].Value
        $card=[ordered]@{ index=$index; kind=[int]$row.Groups[2].Value; extra=[int]$row.Groups[3].Value;
            cost=[int]$row.Groups[4].Value; cd=[int]$row.Groups[5].Value; hp=[int]$row.Groups[6].Value; alias=[int]$row.Groups[7].Value; signature=$signature }
        if ($index -ge $eligible -or $card.hp -le 0 -or ($cards.ContainsKey($key) -and $cards[$key].index -ne $index)) { throw 'Observed source pool repeats an identity or contains invalid index/stats.' }
        $byIndex[$index]=$card; $cards[$key]=$card
    }
    if ($byIndex.Count -ne $eligible -or $cards.Count -ne $eligible) { throw 'Source pool evidence omits an eligible identity.' }
    $sourceBaseline=@{0=@(100,750);1=@(50,750);2=@(150,5000);4=@(25,3000)}
    foreach ($kind in $sourceBaseline.Keys) {
        foreach ($extra in @(0,1)) {
            $key=([string]$kind)+':'+$extra
            if (-not $cards.ContainsKey($key) -or $cards[$key].cost -ne $sourceBaseline[$kind][0]*($extra+1) -or
                $cards[$key].cd -ne $sourceBaseline[$kind][1]) { throw 'Mandatory0/1/2/4 source price/CD or variant coverage regressed.' }
        }
    }
    $signature=($eligible,$target,$quota0,$quota1 -join ',')+'|'+((0..($eligible-1) | ForEach-Object {
        if (-not $byIndex.ContainsKey($_)) { throw 'Observed source pool indices are not contiguous.' }; $byIndex[$_].signature
    }) -join '|')
    return [ordered]@{eligible=$eligible;target=$target;quotas=@($quota0,$quota1);cards=$cards;signature=$signature}
}

function Assert-NativeMysteryRecovery {
    param([string]$HostText, [string]$ClientText)
    $records = @()
    foreach ($role in @('host', 'client')) {
        $text = if ($role -eq 'host') { $HostText } else { $ClientText }
        $pattern = 'SMOKE_FUSION_NATIVE_MYSTERY_RNG_RESTART role=' + $role +
            ' oldEpoch=(\d+) oldInput=(\d+) newEpoch=(\d+) newInput=(\d+) ready=0 packets=0 sun=500 cooldown=0 hiddenChecks=(\d+) spanMs=(\d+) wireEpochFresh=1 failedPlayingSnapshots=0 nativeChooser=1'
        $restart = @([regex]::Matches($text, $pattern))
        if ($restart.Count -ne 1) { throw ('Missing one fresh empty chooser after the full failed intro: ' + $role) }
        $r = $restart[0]
        if ([uint32]$r.Groups[1].Value -eq 0 -or $r.Groups[1].Value -eq $r.Groups[3].Value -or
            [uint32]$r.Groups[2].Value -eq 0 -or $r.Groups[2].Value -eq $r.Groups[4].Value -or
            [int]$r.Groups[5].Value -lt 10 -or [int]$r.Groups[6].Value -lt 1000) {
            throw ('RNG restart reused an epoch or bypassed the hidden native intro: ' + $role)
        }
        $complete = [regex]::Match($text, 'SMOKE_FUSION_NATIVE_MYSTERY_RNG_COMPLETE role=' + $role +
            ' oldEpoch=' + $r.Groups[1].Value + ' newEpoch=' + $r.Groups[3].Value +
            ' chooserInput=' + $r.Groups[4].Value + ' playingInput=(\d+) restarts=1 fullIntros=2 staleLocalReject=1 secondRevealAtomic=1 nativeWorld=1 transportReplayNotTested=1')
        if (-not $complete.Success -or $complete.Groups[1].Value -eq $r.Groups[4].Value -or
            $text -notmatch ('MYSTERY_COMPLETE[^\r\n]*epoch=' + $r.Groups[3].Value + '\b') -or
            $text -notmatch 'MYSTERY_FRAME[^\r\n]*step=recovery_intro_secret\b' -or
            $text -notmatch 'MYSTERY_FRAME[^\r\n]*step=recovery_pending_final\b') {
            throw ('Recovered bank did not use a second full intro and a fresh ordered Playing barrier: ' + $role)
        }
        $records += [ordered]@{ role = $role; old_epoch = [uint32]$r.Groups[1].Value; old_input = [uint32]$r.Groups[2].Value;
            new_epoch = [uint32]$r.Groups[3].Value; chooser_input = [uint32]$r.Groups[4].Value;
            playing_input = [uint32]$complete.Groups[1].Value; failed_hidden_checks = [int]$r.Groups[5].Value; failed_intro_ms = [int]$r.Groups[6].Value }
    }
    foreach ($key in @('old_epoch', 'old_input', 'new_epoch', 'chooser_input', 'playing_input')) {
        if ($records[0][$key] -ne $records[1][$key]) { throw ('RNG recovery peers disagree on activity/input generation: ' + $key) }
    }
    $record = $records[0]
    if ($HostText -notmatch ('MYSTERY_RNG_STALE role=host oldEpoch=' + $record.old_epoch + ' newEpoch=' + $record.new_epoch +
        ' applied=0 fingerprintUnchanged=1 validFreshChoose=1 localBusinessGuard=1 transportReplayNotTested=1') -or
        [regex]::Matches($HostText, 'Fusion chooser restart requested epoch=').Count -ne 1 -or
        $HostText -notmatch ('Fusion chooser restart consuming epoch=' + $record.old_epoch + ' inputEpoch=' + $record.old_input + ' depth=[01] board=') -or
        $HostText -notmatch ('Fusion chooser restart completed oldEpoch=' + $record.old_epoch + ' newEpoch=' + $record.new_epoch + ' inputEpoch=\d+ board=')) {
        throw 'Missing actual deferred restart lifecycle or local old-activity rollback proof.'
    }
    $failureAt = $HostText.IndexOf('Fusion chooser restart requested epoch=')
    $hostStart = $HostText.IndexOf('SMOKE_FUSION_NATIVE_MYSTERY_CHOOSER role=host')
    $clientStart = $ClientText.IndexOf('SMOKE_FUSION_NATIVE_MYSTERY_CHOOSER role=client')
    $clientRestart = $ClientText.IndexOf('SMOKE_FUSION_NATIVE_MYSTERY_RNG_RESTART')
    if ($hostStart -lt 0 -or $clientStart -lt 0 -or $hostStart -ge $failureAt -or $clientStart -ge $clientRestart -or
        $HostText.Substring($hostStart, $failureAt-$hostStart) -match 'host entering gameplay; broadcasting barrier|SMOKE_SNAPSHOT_TX' -or
        $ClientText.Substring($clientStart, $clientRestart-$clientStart) -match 'gameplay entered client=1|SMOKE_SNAPSHOT_APPLIED') {
        throw 'Failed activity exposed a gameplay barrier/snapshot before the fresh chooser.'
    }
    return [ordered]@{ fault = 'RngFailure'; restarts = 1; full_intros = 2; peers = $records;
        old_activity_rejection = 'local host business guard'; transport_replay = 'Not executed'; partial_reveal = $false }
}

try {
    if (-not (Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw ('Game executable missing: ' + $GameExe) }
    foreach ($resource in @('main.pak', 'dependency.pak', 'SDL3.dll', 'fusion\pilot\peasunflower\animation.json', 'fusion\pilot\portrait-peasunflower\animation.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $mysteryGameDirectory $resource) -PathType Leaf)) { throw ('Native mystery runtime resource missing: ' + $resource) }
    }
    foreach ($directory in @($mysteryHostLogs, $mysteryClientLogs, (Join-Path $mysteryHostRoot 'appdata'), (Join-Path $mysteryClientRoot 'appdata'))) { Ensure-Directory $directory }
    if ($Port -eq 0) { $Port = Get-FreeLoopbackPort }
    foreach ($name in $mysteryEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE = '1'
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY = '1'
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY = [string]$mysteryDifficulty
    if ($mysteryFault -eq 'RngFailure') { $env:PVZ_COOP_SMOKE_MYSTERY_FAULT = 'RngFailure' }
    foreach ($role in @('host', 'client')) {
        $root = if ($role -eq 'host') { $mysteryHostRoot } else { $mysteryClientRoot }
        $nickname = if ($role -eq 'host') { 'FusMyHost' } else { 'FusMyClient' }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth 800 -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') `
            -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds * 1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native mystery D' + $mysteryDifficulty + ' fault=' + $mysteryFault + ': two owned hidden800x600 windows; ' + $mysteryRunRoot)
    $mysteryHostProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $mysteryGameDirectory `
        -ManifestPath (Join-Path $mysteryHostRoot 'smoke.ini') -Role host -RunId $mysteryRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $mysteryRunRoot 'pids.json') -Executable $GameExe -HostProcess $mysteryHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $mysteryHostLogs -Pattern ('(?im)host listening port=' + $Port + '\b') `
        -Process $mysteryHostProcess -Role host -TimeoutSeconds 120
    $mysteryClientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $mysteryGameDirectory `
        -ManifestPath (Join-Path $mysteryClientRoot 'smoke.ini') -Role client -RunId $mysteryRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $mysteryRunRoot 'pids.json') -Executable $GameExe -HostProcess $mysteryHostProcess -ClientProcess $mysteryClientProcess
    $results = @(Wait-ForResults -HostResultPath (Join-Path $mysteryHostRoot 'result.json') -ClientResultPath (Join-Path $mysteryClientRoot 'result.json') `
        -HostProcess $mysteryHostProcess -ClientProcess $mysteryClientProcess -HostLogDirectory $mysteryHostLogs `
        -ClientLogDirectory $mysteryClientLogs -TimeoutSeconds $TimeoutSeconds)
    $protocols = @((Assert-NativeMysteryResult $results[0] host), (Assert-NativeMysteryResult $results[1] client))
    if ($protocols[0] -ne $protocols[1]) { throw 'Native mystery peer protocols differ.' }
    $hostText = Get-CombinedLogText $mysteryHostLogs
    $clientText = Get-CombinedLogText $mysteryClientLogs
    $mysteryHostPool=Get-NativePoolEvidence $hostText host SMOKE_FUSION_NATIVE_MYSTERY
    $mysteryClientPool=Get-NativePoolEvidence $clientText client SMOKE_FUSION_NATIVE_MYSTERY
    if ($mysteryHostPool.signature -ne $mysteryClientPool.signature) { throw 'Native mystery peers disagree on the unfiltered source pool/target/quotas.' }
    $mysteryQuota=$mysteryHostPool.quotas[1]
    if ($mysteryFault -eq 'RngFailure') { $mysteryRecovery = Assert-NativeMysteryRecovery $hostText $clientText }
    elseif ($hostText -match 'MYSTERY_RNG_RESTART|Fusion chooser restart requested epoch=') { throw 'Unexpected mystery recovery in the normal D1/D10 case.' }
    foreach ($text in @($hostText, $clientText)) {
        if ($text -match '(?im)\bSMOKE_FAIL\b|Access Violation|unhandled exception|assertion failed|native Fusion snapshot[^\r\n]*rejected|reason=sequence|Fusion start host epoch=|Fusion room') {
            throw 'Native mystery contains rejection, sequence gap, crash or independent-world evidence.'
        }
        foreach ($marker in @('CHOOSER', 'SECRET', 'CANCEL', 'INTRO', 'REVEAL', 'POOL', 'PURCHASE', 'COMPLETE')) {
            if ($text -notmatch ('(?im)\bSMOKE_FUSION_NATIVE_MYSTERY_' + $marker + '\b')) { throw ('Native mystery checkpoint missing: ' + $marker) }
        }
        if ($text -notmatch 'MYSTERY_SECRET[^\r\n]*pass=1[^\r\n]*hiddenKind=1 hiddenExtra=1 hiddenPrice=1 hiddenRecharge=1 nativePackets=1' -or
            $text -notmatch 'MYSTERY_SECRET[^\r\n]*pass=2[^\r\n]*hiddenKind=1 hiddenExtra=1 hiddenPrice=1 hiddenRecharge=1 nativePackets=1' -or
            $text -notmatch ('MYSTERY_POOL[^\r\n]*eligible='+$mysteryHostPool.eligible+' hostManualKinds='+$mysteryHostPool.quotas[0]+' baseKindOverlap='+$mysteryHostPool.quotas[0]+'\b') -or
            $text -notmatch ('MYSTERY_CHOOSER[^\r\n]*difficulty=' + $mysteryDifficulty + '\b[^\r\n]*nativeChooser=1 secondWorld=0') -or
            $text -notmatch 'MYSTERY_COMPLETE[^\r\n]*cooldownProgress=1 nativeWorld=1 secondWorld=0') {
            throw 'Native mystery secrecy, full pool, difficulty or native ownership evidence is incomplete.'
        }
        $intro = [regex]::Match($text, 'MYSTERY_INTRO[^\r\n]*hiddenChecks=(\d+) spanMs=(\d+) noLeak=1')
        if (-not $intro.Success -or [int]$intro.Groups[1].Value -lt 10 -or [int]$intro.Groups[2].Value -lt 1000) { throw 'Full hidden native intro was not observed.' }
    }
    if ($hostText -notmatch ('MYSTERY_DIFFICULTY role=host difficulty=' + $mysteryDifficulty + ' cleanLobby=1') -or
        [regex]::Matches($clientText, 'MYSTERY_BUTTON role=client[^\r\n]*hoverObserved=1 nativeSDL=1').Count -lt 4 -or
        $clientText -notmatch 'MYSTERY_HELD role=client[^\r\n]*snapshotEcho=1 charge=0 recharge=0 nativeSDL=1' -or
        $hostText -notmatch '(?im)Fusion native command slot=1 action=5 sequence=\d+ applied=1\b') { throw 'Native mystery is missing real hover/toggle/ready/authoritative held/purchase input proof.' }
    foreach ($pattern in @(
        'MYSTERY_PURCHASE[^\r\n]*difficulty=(\d+) sid=(\d+) packet=(\d+) plant=(\d+) kind=(\d+) extra=([01]) origin=(\d+) price=(\d+) sun=(\d+) cooldown=(\d+)',
        'MYSTERY_COMPLETE[^\r\n]*difficulty=(\d+) epoch=(\d+) packet=(\d+) plant=(\d+) price=(\d+) sun=(\d+) cooldown=(\d+)'
    )) {
        $a = [regex]::Match($hostText, $pattern); $b = [regex]::Match($clientText, $pattern)
        if (-not $a.Success -or -not $b.Success) { throw ('Shared native mystery identity proof missing: ' + $pattern) }
        for ($i = 1; $i -lt $a.Groups.Count; ++$i) { if ($a.Groups[$i].Value -ne $b.Groups[$i].Value) { throw 'Native mystery peer identities/prices/provenance differ.' } }
    }
    # Every observed pending/revealed identity is checked, including recovery.
    $attempts=if ($mysteryFault -eq 'RngFailure') {2} else {1}
    $lastSecrets=@{}
    foreach ($pass in @(1,2)) {
        foreach ($role in @('host','client')) {
            $text=if ($role -eq 'host') {$hostText} else {$clientText}
            if ([regex]::Matches($text,('MYSTERY_SECRET role='+$role+' pass='+$pass+' count='+$mysteryQuota+' hiddenKind=1')).Count -ne $attempts -or
                [regex]::Matches($text,('MYSTERY_SECRET_ID role='+$role+' pass='+$pass+' index=')).Count -ne $attempts*$mysteryQuota) { throw 'Pending mystery identity evidence is incomplete or contains extra entries.' }
        }
        $seenByAttempt=@(); for ($attempt=0;$attempt -lt $attempts;++$attempt) { $seenByAttempt+=@{sid=@{};packet=@{}} }
        for ($index=0;$index -lt $mysteryQuota;++$index) {
            $suffix=' pass='+$pass+' index='+$index+' sid=(\d+) packet=(\d+)\b'
            $a=@([regex]::Matches($hostText,('MYSTERY_SECRET_ID role=host'+$suffix)))
            $b=@([regex]::Matches($clientText,('MYSTERY_SECRET_ID role=client'+$suffix)))
            if ($a.Count -ne $attempts -or $b.Count -ne $attempts) { throw 'A full pending quota identity is missing from an activity attempt.' }
            for ($attempt=0;$attempt -lt $attempts;++$attempt) {
                $sid=$a[$attempt].Groups[1].Value; $packet=$a[$attempt].Groups[2].Value
                if ([int]$sid -le 0 -or [int]$packet -le 0 -or $sid -ne $b[$attempt].Groups[1].Value -or $packet -ne $b[$attempt].Groups[2].Value -or
                    $seenByAttempt[$attempt].sid.ContainsKey($sid) -or $seenByAttempt[$attempt].packet.ContainsKey($packet)) { throw 'Pending mystery identities differ between peers or repeat within a quota.' }
                $seenByAttempt[$attempt].sid[$sid]=$true; $seenByAttempt[$attempt].packet[$packet]=$true
            }
            if ($pass -eq 2) { $lastSecrets[$index]=@($a[-1].Groups[1].Value,$a[-1].Groups[2].Value) }
        }
    }
    $revealedIdentities=@{}
    if ([regex]::Matches($hostText,'MYSTERY_REVEAL role=host index=').Count -ne $mysteryQuota -or
        [regex]::Matches($clientText,'MYSTERY_REVEAL role=client index=').Count -ne $mysteryQuota) { throw 'Atomic reveal quota is incomplete or contains extra entries.' }
    for ($index=0;$index -lt $mysteryQuota;++$index) {
        $suffix=' index='+$index+' sid=(\d+) packet=(\d+) kind=(\d+) extra=([01]) origin=(\d+) price=(\d+) cd=([\d.]+) stableIds=1 atomic=1 nativePacket=1'
        $a=[regex]::Match($hostText,('MYSTERY_REVEAL role=host'+$suffix)); $b=[regex]::Match($clientText,('MYSTERY_REVEAL role=client'+$suffix))
        if (-not $a.Success -or -not $b.Success -or $a.Groups[1].Value -ne $lastSecrets[$index][0] -or $a.Groups[2].Value -ne $lastSecrets[$index][1]) { throw 'Atomic reveal did not preserve a final pending native identity.' }
        for ($group=1;$group -lt $a.Groups.Count;++$group) { if ($a.Groups[$group].Value -ne $b.Groups[$group].Value) { throw 'A revealed quota identity/source value differs between peers.' } }
        $identity=$a.Groups[3].Value+':'+$a.Groups[4].Value
        if ($revealedIdentities.ContainsKey($identity)) { throw 'Mystery repeated a semantic kind/extra identity.' }; $revealedIdentities[$identity]=$true
    }
    # Source definition-backed price/CD; original four source baselines remain fixed above.
    foreach ($reveal in [regex]::Matches($hostText, 'MYSTERY_REVEAL[^\r\n]*kind=(\d+) extra=([01]) origin=(\d+) price=(\d+) cd=([\d.]+)')) {
        $kind = [int]$reveal.Groups[1].Value; $extra = [int]$reveal.Groups[2].Value
        $identity=([string]$kind)+':'+$extra
        if (-not $mysteryHostPool.cards.ContainsKey($identity) -or [int]$reveal.Groups[3].Value -ne $mysteryDifficulty) { throw 'Native mystery revealed an ineligible source kind/origin.' }
        $base = $mysteryHostPool.cards[$identity].cost
        $recharge = $mysteryHostPool.cards[$identity].cd/100.0
        $numerator = if ($mysteryDifficulty -eq 1) { 93 } else { 45 }
        if ([int]$reveal.Groups[4].Value -ne [int][Math]::Ceiling($base*$numerator/100.0) -or
            [double]::Parse($reveal.Groups[5].Value, [Globalization.CultureInfo]::InvariantCulture) -ne $recharge) { throw 'Native mystery source price/recharge expectation failed.' }
    }
    $tx = @(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX)
    $applied = @(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $mysteryCommon = @($applied | Where-Object { $tx -contains $_ } | Select-Object -Unique)
    if ($mysteryCommon.Count -lt 8) { throw 'Fewer than eight actual native snapshot hashes match between peers.' }
    $hostAfter = $hostText.Substring($hostText.IndexOf('SMOKE_FUSION_NATIVE_MYSTERY_PURCHASE'))
    $clientAfter = $clientText.Substring($clientText.IndexOf('SMOKE_FUSION_NATIVE_MYSTERY_PURCHASE'))
    $afterTx = @(Get-SnapshotHashes -Text $hostAfter -Marker SMOKE_SNAPSHOT_TX)
    $afterApplied = @(Get-SnapshotHashes -Text $clientAfter -Marker SMOKE_SNAPSHOT_APPLIED)
    $mysteryPostPurchase = @($afterApplied | Where-Object { $afterTx -contains $_ } | Select-Object -Unique)
    if ($mysteryPostPurchase.Count -lt 2) { throw 'Final barrier has fewer than two shared post-purchase snapshot hashes.' }
    $mysteryFrames = @((Assert-NativeMysteryFrames $mysteryHostRoot host), (Assert-NativeMysteryFrames $mysteryClientRoot client))
    $mysteryPassed = $true
}
catch { $mysteryError = $_.Exception.Message; if ($_.ScriptStackTrace) { $mysteryError += ' | ' + $_.ScriptStackTrace }; Write-SmokeStatus ('FAIL native mystery: ' + $mysteryError) }
finally {
    foreach ($name in $mysteryEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $mysteryEnvironment[$name], 'Process') }
    $clientStopped = Stop-OwnedProcess -Process $mysteryClientProcess -Role client
    $hostStopped = Stop-OwnedProcess -Process $mysteryHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) { $mysteryPassed = $false; $mysteryError += ' Owned native mystery cleanup failed.' }
    Ensure-Directory $mysteryRunRoot
    $summary = [ordered]@{ status = $(if ($mysteryPassed) { 'pass' } else { 'fail' }); error = $mysteryError;
        fixture = 'FusionNativeMystery'; difficulty = $mysteryDifficulty; fault = $mysteryFault; recovery = $mysteryRecovery; run_id = $mysteryRunId;
        executable = $GameExe; executable_sha256 = $(if (Test-Path -LiteralPath $GameExe -PathType Leaf) { (Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash } else { '' });
        started_utc = $mysteryStarted.ToString('o'); finished_utc = [DateTime]::UtcNow.ToString('o');
        host_window = '800x600'; client_window = '800x600'; grid = '10x5'; second_world = $false;
        snapshot_hash_algorithm = 'existing native FNV1a32'; common_snapshot_hashes = $mysteryCommon;
        post_purchase_common_hashes = $mysteryPostPurchase; frames = $mysteryFrames;
        owned_cleanup = [bool]($clientStopped -and $hostStopped); source_campaign_complete = $false; artifacts = $mysteryRunRoot }
    Write-Utf8NoBom -Path (Join-Path $mysteryRunRoot 'summary.json') -Text ((ConvertTo-Json $summary -Depth 8) + [Environment]::NewLine)
}
if ($mysteryPassed) { Write-SmokeStatus ('PASS native mystery D' + $mysteryDifficulty + '; ' + $mysteryRunRoot); exit 0 }
exit 1
