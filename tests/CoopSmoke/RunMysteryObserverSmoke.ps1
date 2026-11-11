#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateSet('All', 'IntroJoin', 'PlayingKick')][string]$Variant = 'All',
    [ValidateRange(1, 10)][int]$Difficulty = 10,
    [ValidateRange(90, 600)][int]$TimeoutSeconds = 240
)

$moExeArgument = $GameExe
$moArtifactsArgument = $ArtifactsRoot
$moVariantArgument = $Variant
$moDifficulty = $Difficulty
$moTimeout = $TimeoutSeconds
$moScriptRoot = Split-Path -Parent $PSCommandPath
. (Join-Path $moScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly
$Background = $true
$moRepo = Get-NormalizedPath (Join-Path $moScriptRoot '..\..')
$moExe = if ($moExeArgument) { Get-NormalizedPath $moExeArgument } else { Join-Path $moRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
if (-not (Test-Path -LiteralPath $moExe -PathType Leaf)) { throw "Game executable not found: $moExe" }
$moArtifacts = if ($moArtifactsArgument) { Get-NormalizedPath $moArtifactsArgument } else { Join-Path $moRepo 'work\mystery-observer' }
$moVariants = if ($moVariantArgument -eq 'All') { @('IntroJoin', 'PlayingKick') } else { @($moVariantArgument) }

function Start-MysteryObserverRole([string]$Actor) {
    $root = Join-Path $moRunRoot $Actor
    $logs = Join-Path $root 'logs'; $appdata = Join-Path $root 'appdata'
    [void][IO.Directory]::CreateDirectory($logs)
    [void][IO.Directory]::CreateDirectory($appdata)
    $manifest = Join-Path $root 'smoke.ini'; $config = Join-Path $root 'coop.ini'
    $result = Join-Path $root 'result.json'
    $manifestRole = if ($Actor -eq 'host') { 'host' } else { 'client' }
    $nickname = 'Mystery ' + $Actor
    Write-CoopConfig -Path $config -Nickname $nickname -WindowWidth 800 -WindowHeight 600
    Write-SmokeManifest -Path $manifest -Role $manifestRole -Nickname $nickname `
        -AppDataDirectory $appdata -ConfigPath $config -LogDirectory $logs -ResultPath $result `
        -SelectedPort $moPort -SelectedMode 'adventure_soak' -SelectedLevel 8 -SelectedSeed 424242 `
        -SelectedTimeoutMs ($moTimeout * 1000) -RequiredSnapshots 3 -SelectedSkipIntro 0
    $values = @{
        PVZ_COOP_SMOKE_MYSTERY = '1'
        PVZ_COOP_SMOKE_MYSTERY_DIFFICULTY = [string]$moDifficulty
        PVZ_COOP_SMOKE_MYSTERY_FAULT = 'None'
        PVZ_COOP_SMOKE_MYSTERY_OBSERVER = $moCurrentVariant
        PVZ_COOP_SMOKE_MYSTERY_OBSERVER_ROLE = $Actor
        PVZ_COOP_SMOKE_SPECTATOR = '1'
        PVZ_COOP_SMOKE_SPECTATOR_PROMOTE = '0'
        PVZ_COOP_SMOKE_NATIVE_UI = ''
        PVZ_COOP_SMOKE_DIFFICULTY = ''
        PVZ_COOP_SMOKE_GAME_OVER = ''
        PVZ_COOP_SMOKE_GAME_OVER_INPUT = ''
        PVZ_COOP_SMOKE_GAME_OVER_MODE = ''
        PVZ_COOP_SMOKE_ONLINE_ENDPOINT = ''
    }
    $previous = @{}
    try {
        foreach ($key in $values.Keys) {
            $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
            [Environment]::SetEnvironmentVariable($key, [string]$values[$key], 'Process')
        }
        $spectatorRole = if ($Actor -eq 'observer') { 'spectator' } else { $Actor }
        $process = Start-SmokeProcess -Executable $moExe -WorkingDirectory (Split-Path -Parent $moExe) `
            -ManifestPath $manifest -Role $manifestRole -RunId $moRunId -EnableUnicodeChat $false -SpectatorRole $spectatorRole
        $state = @{ Role = $Actor; ManifestRole = $manifestRole; Root = $root; Logs = $logs; Result = $result; Process = $process }
        $moProcesses.Add($state)
    }
    finally {
        foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
    }
    Write-Utf8NoBom -Path (Join-Path $moRunRoot 'pids.json') -Text ((@($moProcesses.ToArray() | ForEach-Object {
        @{ role = $_.Role; pid = $_.Process.Id; started_utc = $_.Process.StartTime.ToUniversalTime().ToString('o'); executable = $moExe }
    }) | ConvertTo-Json -Depth 4) + [Environment]::NewLine)
    Write-SmokeStatus ("Mystery observer {0}/{1} PID {2}: hidden game renderer only." -f $moCurrentVariant, $Actor, $process.Id)
    return $state
}

function Assert-MysteryObserverLive {
    foreach ($state in $moProcesses.ToArray()) {
        $log = Get-CombinedLogText $state.Logs
        if ($log -match 'SMOKE_FAIL|Access Violation|assertion failed|START_PLAYING rejected|snapshot [^\r\n]*rejected') {
            throw "Mystery observer runtime failure: $($state.Logs)"
        }
        if ($state.Process.HasExited -and -not (Test-Path -LiteralPath $state.Result)) {
            throw "Mystery actor exited before its proof: $($state.Role)"
        }
    }
}

function Assert-MysteryObserverResult([hashtable]$State) {
    $value = Read-JsonIfComplete $State.Result
    if ($null -eq $value -or $value.status -ne 'pass' -or $value.role -ne $State.ManifestRole -or $value.protocol -le 0) {
        throw "Invalid Mystery observer actor result: $($State.Result)"
    }
    return $value
}

foreach ($moCurrentVariant in $moVariants) {
    $moRunId = [Guid]::NewGuid().ToString('N')
    $moRunRoot = Join-Path $moArtifacts ($moCurrentVariant + '_' + $moRunId)
    [void][IO.Directory]::CreateDirectory($moRunRoot)
    $moProcesses = [Collections.Generic.List[object]]::new()
    $moPort = Get-FreeLoopbackPort
    $moError = ''
    try {
        $moHost = Start-MysteryObserverRole 'host'
        Wait-ForLogPattern -LogDirectory $moHost.Logs -Pattern '(?im)host listening port=' `
            -Process $moHost.Process -Role 'host' -TimeoutSeconds $moTimeout
        $moClient = Start-MysteryObserverRole 'client'
        $deadline = [DateTime]::UtcNow.AddSeconds($moTimeout)
        $phase = if ($moCurrentVariant -eq 'IntroJoin') { 'intro_pending' } else { 'playing' }
        while ([DateTime]::UtcNow -lt $deadline) {
            Assert-MysteryObserverLive
            if ((Get-CombinedLogText $moHost.Logs) -match ('SMOKE_MO_WAIT role=host phase=' + $phase + ' pause=1')) { break }
            Start-Sleep -Milliseconds 200
        }
        if ((Get-CombinedLogText $moHost.Logs) -notmatch ('SMOKE_MO_WAIT role=host phase=' + $phase + ' pause=1')) {
            throw 'Original pair never reached the explicit native pause/join checkpoint.'
        }
        $moObserver = Start-MysteryObserverRole 'observer'
        while ([DateTime]::UtcNow -lt $deadline) {
            Assert-MysteryObserverLive
            $complete = $true
            foreach ($state in $moProcesses.ToArray()) { if ($null -eq (Read-JsonIfComplete $state.Result)) { $complete = $false } }
            if ($complete) { break }
            Start-Sleep -Milliseconds 200
        }
        $results = @($moProcesses.ToArray() | ForEach-Object { Assert-MysteryObserverResult $_ })
        if (@($results.protocol | Select-Object -Unique).Count -ne 1) { throw 'Mystery observer peers used different protocol versions.' }
        $texts = @{}
        $seen = @()
        foreach ($state in $moProcesses.ToArray()) {
            $texts[$state.Role] = Get-CombinedLogText $state.Logs
            if ($state.Role -ne 'observer' -and
                ($texts[$state.Role] -notmatch 'SMOKE_MYSTERY_PURCHASE[^\r\n]*displayed=[1-9]\d* spent=[1-9]\d* cooldown=[1-9]\d*' -or
                 -not (Test-Path -LiteralPath (Join-Path $state.Root 'mystery_spent.bmp')) -or
                 -not (Test-Path -LiteralPath (Join-Path $state.Root 'mystery_intro_secret.bmp')))) {
                throw "Original Mystery pair did not finish its native secret/reveal/discounted-spend precondition: $($state.Role)"
            }
            $match = [regex]::Match($texts[$state.Role], ('SMOKE_MO_SEEN role=' + $state.Role + ' shape=([0-9A-F]{8}) cooldown=([0-9A-F]{8}) originCards=5 owner=1 phase=playing capture=1'))
            if (-not $match.Success -or -not (Test-Path -LiteralPath (Join-Path $state.Root 'mystery_observer_reveal.bmp'))) {
                throw "Missing actual mirrored Mystery metadata/cooldown capture: $($state.Role)"
            }
            $seen += $match
        }
        foreach ($group in 1, 2) {
            if ($seen[0].Groups[$group].Value -ne $seen[1].Groups[$group].Value -or $seen[0].Groups[$group].Value -ne $seen[2].Groups[$group].Value) {
                throw 'Mystery observer identity/origin/price/cooldown hashes differ across the three actors.'
            }
        }
        if ([regex]::Matches($texts.host, 'spectator mutation rejected slot=2 type=INPUT_MOUSE').Count -lt 2 -or
            $texts.host -match 'mouse apply slot=2' -or $texts.observer -notmatch 'SMOKE_MO_READONLY role=observer slot=2 owned=0 rawFrames=2') {
            throw 'Missing authenticated observer mutation rejection evidence.'
        }
        $proof = Read-JsonIfComplete (Join-Path $moObserver.Root 'mystery_observer.json')
        if ($null -eq $proof -or $proof.status -ne 'pass' -or -not $proof.readonly -or $proof.rawFrames -ne 2) {
            throw 'Observer extension proof is missing or incomplete.'
        }
        if ($moCurrentVariant -eq 'IntroJoin') {
            if (-not $proof.introJoin -or $texts.observer -notmatch 'SMOKE_MO_INTRO role=observer scene=intro pending=5 realTypes=0' -or
                -not (Test-Path -LiteralPath (Join-Path $moObserver.Root 'mystery_observer_intro.bmp'))) {
                throw 'Late observer did not see a secret pending bank before its stable-ID reveal.'
            }
        }
        else {
            $kicked = Read-JsonIfComplete (Join-Path $moClient.Root 'mystery_observer_owner_kick.json')
            if ($null -eq $kicked -or -not $kicked.hostKick -or $kicked.slot -ne 1 -or $kicked.origin -ne $moDifficulty -or
                $texts.client -notmatch 'SMOKE_MO_KICKED role=client slot=1 reason=host_kick') { throw 'Active mystery owner did not observe an actual host kick.' }
            $purchases = @()
            foreach ($actor in @('host', 'observer')) {
                $state = @($moProcesses.ToArray() | Where-Object Role -eq $actor)[0]
                if ($texts[$actor] -notmatch ('SMOKE_MO_TRANSFER role=' + $actor + ' [^\r\n]*beforeOwner=1 owner=0 [^\r\n]*preserved=1') -or
                    -not (Test-Path -LiteralPath (Join-Path $state.Root 'mystery_observer_transfer.bmp')) -or
                    -not (Test-Path -LiteralPath (Join-Path $state.Root 'mystery_observer_purchase.bmp'))) {
                    throw "Missing stable origin/cooldown transfer renderer evidence: $actor"
                }
                $purchase = [regex]::Match($texts[$actor], ('SMOKE_MO_PURCHASE role=' + $actor + ' packet=(\d+) plant=(\d+) cell=(\d+),(\d+) displayed=(\d+) spent=(\d+) origin=(\d+) sdl=1'))
                if (-not $purchase.Success -or $purchase.Groups[5].Value -ne $purchase.Groups[6].Value -or
                    [int]$purchase.Groups[5].Value -le 0 -or [int]$purchase.Groups[7].Value -ne $moDifficulty) { throw "Transferred card did not use its real discounted price: $actor" }
                $purchases += $purchase
            }
            foreach ($group in 1..7) {
                if ($purchases[0].Groups[$group].Value -ne $purchases[1].Groups[$group].Value) { throw 'Recipient and observer disagree about the transferred native purchase.' }
            }
        }
    }
    catch { $moError = $_.Exception.Message; Write-Host ("[MysteryObserver] FAIL: " + $moError) -ForegroundColor Red }
    finally {
        foreach ($state in @($moProcesses.ToArray()) | Sort-Object { $_.Role -eq 'host' }) {
            if (-not (Stop-OwnedProcess -Process $state.Process -Role $state.Role)) { $moError = 'Owned Mystery observer process could not be stopped.' }
        }
        Write-Utf8NoBom -Path (Join-Path $moRunRoot 'summary.json') -Text ((@{
            status = $(if ($moError) { 'fail' } else { 'pass' }); variant = $moCurrentVariant;
            difficulty = $moDifficulty; error = $moError; artifacts = $moRunRoot
        } | ConvertTo-Json -Depth 4) + [Environment]::NewLine)
    }
    if ($moError) { exit 1 }
    Write-SmokeStatus ("PASS Mystery observer {0}: real late join, stable origin/cooldown and bounded read-only gameplay. Artifacts: {1}" -f $moCurrentVariant, $moRunRoot)
}
