#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateSet("Both", "retry", "close")][string]$Action = "Both",
    [ValidateSet("mouse", "keyboard")][string]$InputMethod = "mouse",
    [ValidateSet("Adventure", "PvP")][string]$Mode = "Adventure",
    [ValidateSet(800, 1066)][int]$WindowWidth = 800,
    [switch]$VerifyPvpPrivacy,
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 120
)

$gameOverExeArg = $GameExe
$gameOverArtifactsArg = $ArtifactsRoot
$gameOverActionArg = $Action
$gameOverInputArg = $InputMethod.ToLowerInvariant()
$gameOverModeArg = if ($Mode -eq "PvP") { "PvP" } else { "Adventure" }
$gameOverWindowWidth = $WindowWidth
$gameOverPrivacy = $VerifyPvpPrivacy.IsPresent
$gameOverTimeout = $TimeoutSeconds
$gameOverScriptRoot = Split-Path -Parent $PSCommandPath
. (Join-Path $gameOverScriptRoot "RunCoopSmoke.ps1") -FunctionsOnly
$Background = $true
if ($gameOverModeArg -ne "PvP" -and ($gameOverPrivacy -or $gameOverWindowWidth -ne 800)) {
    throw "Widescreen and pick-privacy verification require the PvP GameOver workflow."
}
$gameOverRepo = Get-NormalizedPath (Join-Path $gameOverScriptRoot "..\..")
$gameOverExe = if ($gameOverExeArg) { Get-NormalizedPath $gameOverExeArg } else {
    Join-Path $gameOverRepo "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe"
}
if (-not (Test-Path -LiteralPath $gameOverExe -PathType Leaf)) { throw "Game executable not found: $gameOverExe" }
$gameOverArtifacts = if ($gameOverArtifactsArg) { Get-NormalizedPath $gameOverArtifactsArg } else {
    Join-Path $gameOverRepo "work\gameover-smoke"
}
$gameOverId = [Guid]::NewGuid().ToString("N")
$gameOverRoot = Join-Path $gameOverArtifacts ("run_" + $gameOverId)
[void][IO.Directory]::CreateDirectory($gameOverRoot)
$gameOverCases = [Collections.Generic.List[object]]::new()
if ($gameOverModeArg -eq "PvP" -and $gameOverActionArg -eq "close") { throw "PvP GameOver fixture covers Retry; use Adventure for close." }
$gameOverActions = if ($gameOverModeArg -eq "PvP") { @("retry") } elseif ($gameOverActionArg -eq "Both") { @("retry", "close") } else { @($gameOverActionArg) }

function Start-GameOverRole {
    param([string]$Role, [string]$Case, [int]$Port)
    $root = Join-Path (Join-Path $gameOverRoot $Case) $Role
    $state = @{ Role = $Role; Root = $root; Logs = Join-Path $root "logs"; Process = $null; Result = Join-Path $root "result.json" }
    [void][IO.Directory]::CreateDirectory($state.Logs)
    $appData = Join-Path $root "appdata"
    [void][IO.Directory]::CreateDirectory($appData)
    $config = Join-Path $root "coop.ini"
    $manifest = Join-Path $root "smoke.ini"
    Write-CoopConfig -Path $config -Nickname ("GameOver " + $Role) -WindowWidth $gameOverWindowWidth -WindowHeight 600
    $selectedMode = if ($gameOverModeArg -eq "PvP") { "pvp_1v1" } else { "adventure" }
    Write-SmokeManifest -Path $manifest -Role $Role -Nickname ("GameOver " + $Role) `
        -AppDataDirectory $appData -ConfigPath $config -LogDirectory $state.Logs -ResultPath $state.Result `
        -SelectedPort $Port -SelectedMode $selectedMode -SelectedLevel 1 -SelectedSeed 424242 `
        -SelectedTimeoutMs ($gameOverTimeout * 1000) -RequiredSnapshots 3 -SelectedSkipIntro 0
    $oldAction = $env:PVZ_COOP_SMOKE_GAME_OVER
    $oldInput = $env:PVZ_COOP_SMOKE_GAME_OVER_INPUT
    $oldMode = $env:PVZ_COOP_SMOKE_GAME_OVER_MODE
    $oldPrivacy = $env:PVZ_COOP_SMOKE_PVP_PICK_PRIVACY
    try {
        $env:PVZ_COOP_SMOKE_GAME_OVER = $Case
        $env:PVZ_COOP_SMOKE_GAME_OVER_INPUT = $gameOverInputArg
        $env:PVZ_COOP_SMOKE_GAME_OVER_MODE = $gameOverModeArg
        $env:PVZ_COOP_SMOKE_PVP_PICK_PRIVACY = $(if ($gameOverPrivacy) { "1" } else { "0" })
        $state.Process = Start-SmokeProcess -Executable $gameOverExe -WorkingDirectory (Split-Path -Parent $gameOverExe) `
            -ManifestPath $manifest -Role $Role -RunId $gameOverId -EnableUnicodeChat $false
        $gameOverOwned.Add($state)
        Write-SmokeStatus ("GameOver {0}/{1}/{2} PID {3}: hidden, isolated game renderer only." -f $Case, $gameOverInputArg, $Role, $state.Process.Id)
        return $state
    }
    finally {
        $env:PVZ_COOP_SMOKE_GAME_OVER = $oldAction
        $env:PVZ_COOP_SMOKE_GAME_OVER_INPUT = $oldInput
        $env:PVZ_COOP_SMOKE_GAME_OVER_MODE = $oldMode
        $env:PVZ_COOP_SMOKE_PVP_PICK_PRIVACY = $oldPrivacy
    }
}

function Wait-GameOverRole {
    param([hashtable]$State, [hashtable]$Peer)
    $deadline = [DateTime]::UtcNow.AddSeconds($gameOverTimeout)
    while ([DateTime]::UtcNow -lt $deadline) {
        foreach ($role in @($State, $Peer)) {
            $log = Get-CombinedLogText $role.Logs
            $failure = [regex]::Match($log, 'SMOKE_FAIL reason=([^\r\n]+)')
            if ($failure.Success) { throw ("{0}: {1}" -f $role.Role, $failure.Groups[1].Value) }
            if ($log -match 'Access Violation|unhandled exception|assertion failed') { throw "Native runtime failure: $($role.Logs)" }
        }
        $result = Read-JsonIfComplete $State.Result
        if ($null -ne $result) {
            if ($result.status -ne "pass" -or $result.role -ne $State.Role -or $result.protocol -le 0) { throw "Invalid GameOver result: $($State.Result)" }
            return $result
        }
        if ($State.Process.HasExited) { throw "GameOver process exited before result: $($State.Role)" }
        Start-Sleep -Milliseconds 200
    }
    throw "GameOver runtime timed out: $($State.Logs)"
}

Write-SmokeStatus ("GameOver artifacts: {0}" -f $gameOverRoot)
foreach ($case in $gameOverActions) {
    $gameOverOwned = [Collections.Generic.List[object]]::new()
    $caseError = ""
    try {
        $port = Get-FreeLoopbackPort
        $hostRole = Start-GameOverRole "host" $case $port
        Wait-ForLogPattern -LogDirectory $hostRole.Logs -Pattern '(?im)host listening port=' `
            -Process $hostRole.Process -Role "host" -TimeoutSeconds $gameOverTimeout
        $clientRole = Start-GameOverRole "client" $case $port
        $hostResult = Wait-GameOverRole $hostRole $clientRole
        $clientResult = Wait-GameOverRole $clientRole $hostRole
        if ($hostResult.protocol -ne $clientResult.protocol) { throw "GameOver peers reported different protocols." }
        foreach ($state in @($hostRole, $clientRole)) {
            $log = Get-CombinedLogText $state.Logs
            if ($log -notmatch 'SMOKE_GAMEOVER_SHOWN[^\r\n]*dialog=1' -or
                $log -notmatch 'SMOKE_GAMEOVER_CLEAN[^\r\n]*dialog=0[^\r\n]*capture=1' -or
                -not (Test-Path -LiteralPath (Join-Path $state.Root "gameover_shown.bmp")) -or
                -not (Test-Path -LiteralPath (Join-Path $state.Root "gameover_clean_after_action.bmp"))) {
                throw "Missing GameOver native modal/cleanup evidence: $($state.Role)"
            }
        }
        $hostLog = Get-CombinedLogText $hostRole.Logs
        $key = if ($gameOverInputArg -eq "keyboard") { if ($case -eq "retry") { "Enter" } else { "Escape" } } else { "none" }
        if ($hostLog -notmatch ('SMOKE_GAMEOVER_ACTION role=host action=' + $case + ' sdl=1 input=' + $gameOverInputArg + ' key=' + $key)) {
            throw "Missing real host SDL action."
        }
        if ($gameOverInputArg -eq "keyboard") {
            $clientLog = Get-CombinedLogText $clientRole.Logs
            if ($clientLog -notmatch ('SMOKE_GAMEOVER_CLIENT_KEY key=' + $key + ' ignored=1 dialog=1 capture=1') -or
                -not (Test-Path -LiteralPath (Join-Path $clientRole.Root "gameover_client_key_ignored.bmp"))) {
                throw "Client keyboard input did not preserve host-owned GameOver state."
            }
        }
        if ($case -eq "retry") {
            $clientLog = Get-CombinedLogText $clientRole.Logs
            $modePattern = if ($gameOverModeArg -eq "PvP") { '\d+' } else { '0' }
            if ([regex]::Matches($hostLog, ('start game broadcast mode=' + $modePattern + ' level=1')).Count -lt 2 -or
                [regex]::Matches($clientLog, ('client START_GAME decoded mode=' + $modePattern + ' level=1')).Count -lt 2) {
                throw "Retry did not create an authoritative second board generation on both peers."
            }
        }
        if ($gameOverModeArg -eq "PvP") {
            foreach ($state in @($hostRole, $clientRole)) {
                $log = Get-CombinedLogText $state.Logs
                foreach ($round in @("initial", "retry")) {
                    if ($log -notmatch ('SMOKE_GAMEOVER_PVP_TEAMS[^\r\n]*round=' + $round + ' board=0 gameover=0 scene=menu capture=1') -or
                        -not (Test-Path -LiteralPath (Join-Path $state.Root ("gameover_pvp_teams_" + $round + ".bmp")))) {
                        throw "Missing native clean PvP team-selection round: $($state.Role)/$round"
                    }
                    if ($gameOverPrivacy) {
                        $waiting = if ($state.Role -eq "client") { 1 } else { 0 }
                        $quota = if ($state.Role -eq "client") { 0 } else { 10 }
                        $picture = if ($state.Role -eq "client") { "gameover_pvp_waiting_" } else { "gameover_pvp_plants_" }
                        if ($log -notmatch ('SMOKE_PVP_PICK_PRIVACY role=' + $state.Role + ' round=' + $round +
                            ' manual=10 waiting=' + $waiting + ' tooltip=0 quota=' + $quota + ' readyEnabled=1 size=' +
                            $gameOverWindowWidth + 'x600 sdlHiddenClick=' + $waiting + ' capture=1') -or
                            -not (Test-Path -LiteralPath (Join-Path $state.Root ($picture + $round + ".bmp")))) {
                            throw "Missing actual pre-ready PvP manual/waiting privacy proof: $($state.Role)/$round"
                        }
                        if ($state.Role -eq "client" -and
                            ($log -notmatch ('SMOKE_PVP_PICK_PRIVACY_INTRO role=client round=' + $round +
                             ' waiting=1 mouse=0 tooltip=0 size=' + $gameOverWindowWidth + 'x600 capture=1') -or
                             -not (Test-Path -LiteralPath (Join-Path $state.Root ("gameover_pvp_waiting_intro_" + $round + ".bmp"))))) {
                            throw "Missing Zombie privacy through the closing intro: $round"
                        }
                    }
                }
            }
        }
    }
    catch { $caseError = $_.Exception.Message; Write-Host ("[GameOver] {0} Failed: {1}" -f $case, $caseError) -ForegroundColor Red }
    finally {
        foreach ($state in @($gameOverOwned.ToArray()) | Sort-Object { $_.Role -eq "host" }) {
            if (-not (Stop-OwnedProcess -Process $state.Process -Role $state.Role)) { $caseError = "Owned GameOver process could not be stopped." }
        }
        $gameOverCases.Add(@{ action = $case; mode = $gameOverModeArg; input = $gameOverInputArg; width = $gameOverWindowWidth;
            pick_privacy = $gameOverPrivacy; status = $(if ($caseError) { "fail" } else { "pass" }); error = $caseError })
    }
}
$failed = @($gameOverCases.ToArray() | Where-Object { $_.status -ne "pass" })
$summary = @{ status = $(if ($failed.Count) { "fail" } else { "pass" }); run_id = $gameOverId;
    cases = @($gameOverCases.ToArray()); artifacts = $gameOverRoot }
Write-Utf8NoBom -Path (Join-Path $gameOverRoot "summary.json") -Text ($summary | ConvertTo-Json -Depth 6)
if ($failed.Count) { exit 1 }
Write-SmokeStatus ("PASS: actual GameOver host dismissal and authoritative retry. Artifacts: {0}" -f $gameOverRoot)
