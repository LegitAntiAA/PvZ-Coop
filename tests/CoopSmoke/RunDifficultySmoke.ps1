#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 180,
    [switch]$FamiliesOnly,
    [switch]$PvPOnly
)

$difficultyExeArg = $GameExe
$difficultyArtifactsArg = $ArtifactsRoot
$difficultyTimeout = $TimeoutSeconds
$difficultyFamiliesOnly = [bool]$FamiliesOnly
$difficultyPvPOnly = [bool]$PvPOnly
$difficultyScriptRoot = Split-Path -Parent $PSCommandPath
. (Join-Path $difficultyScriptRoot "RunCoopSmoke.ps1") -FunctionsOnly
$Background = $true
$difficultyRepo = Get-NormalizedPath (Join-Path $difficultyScriptRoot "..\..")
$difficultyExe = if ($difficultyExeArg) { Get-NormalizedPath $difficultyExeArg } else {
    Join-Path $difficultyRepo "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe"
}
if (-not (Test-Path -LiteralPath $difficultyExe -PathType Leaf)) { throw "Game executable not found: $difficultyExe" }
if ($difficultyFamiliesOnly -and $difficultyPvPOnly) { throw "Choose either FamiliesOnly or PvPOnly." }
$difficultyArtifacts = if ($difficultyArtifactsArg) { Get-NormalizedPath $difficultyArtifactsArg } else {
    Join-Path $difficultyRepo "work\difficulty-smoke"
}
$difficultyId = [Guid]::NewGuid().ToString("N")
$difficultyRoot = Join-Path $difficultyArtifacts ("run_" + $difficultyId)
[void][IO.Directory]::CreateDirectory($difficultyRoot)
$difficultyOwned = [Collections.Generic.List[object]]::new()
$difficultyCases = [Collections.Generic.List[object]]::new()
$difficultyError = ""

function Start-DifficultyRole {
    param([string]$Workflow, [string]$Role, [int]$Port)
    $root = Join-Path (Join-Path $difficultyRoot $Workflow) $Role
    $state = @{ Workflow = $Workflow; Role = $Role; Root = $root; Logs = Join-Path $root "logs";
        Result = Join-Path $root "result.json"; Process = $null }
    $appData = Join-Path $root "appdata"
    [void][IO.Directory]::CreateDirectory($appData)
    [void][IO.Directory]::CreateDirectory($state.Logs)
    $config = Join-Path $root "coop.ini"
    $manifest = Join-Path $root "smoke.ini"
    $nick = if ($Role -eq "host") { "[Diff H]" } else { "[Diff C]" }
    # The two native PvP cases also exercise the brain HUD and real SDL input
    # on both supported target widths without changing the user's settings.
    $windowWidth = if ($Role -eq "client" -and $Workflow -eq "pvp10") { 1066 } else { 800 }
    Write-CoopConfig -Path $config -Nickname $nick -WindowWidth $windowWidth -WindowHeight 600
    $selectedMode = if ($Workflow -eq "families") { "adventure" } else { "pvp_1v1" }
    Write-SmokeManifest -Path $manifest -Role $Role -Nickname $nick `
        -AppDataDirectory $appData -ConfigPath $config -LogDirectory $state.Logs -ResultPath $state.Result `
        -SelectedPort $Port -SelectedMode $selectedMode -SelectedLevel 1 -SelectedSeed 424242 `
        -SelectedTimeoutMs ($difficultyTimeout * 1000) -RequiredSnapshots 3 -SelectedSkipIntro 1
    $oldWorkflow = $env:PVZ_COOP_SMOKE_DIFFICULTY
    $oldMystery = $env:PVZ_COOP_SMOKE_MYSTERY
    $oldNative = $env:PVZ_COOP_SMOKE_NATIVE_UI
    try {
        $env:PVZ_COOP_SMOKE_DIFFICULTY = $Workflow
        $env:PVZ_COOP_SMOKE_MYSTERY = "0"
        $env:PVZ_COOP_SMOKE_NATIVE_UI = ""
        $state.Process = Start-SmokeProcess -Executable $difficultyExe -WorkingDirectory (Split-Path -Parent $difficultyExe) `
            -ManifestPath $manifest -Role $Role -RunId $difficultyId -EnableUnicodeChat $false
        $difficultyOwned.Add($state)
        Write-SmokeStatus ("Difficulty {0}/{1} PID {2}: hidden, isolated, game renderer only." -f $Workflow, $Role, $state.Process.Id)
        $ledger = @($difficultyOwned | ForEach-Object { @{ pid = $_.Process.Id; executable = $difficultyExe; workflow = $_.Workflow; role = $_.Role } })
        Write-Utf8NoBom -Path (Join-Path $difficultyRoot "pids.json") -Text ($ledger | ConvertTo-Json -Depth 4)
        return $state
    }
    finally {
        $env:PVZ_COOP_SMOKE_DIFFICULTY = $oldWorkflow
        $env:PVZ_COOP_SMOKE_MYSTERY = $oldMystery
        $env:PVZ_COOP_SMOKE_NATIVE_UI = $oldNative
    }
}

function Wait-DifficultyRole {
    param([hashtable]$State)
    $deadline = [DateTime]::UtcNow.AddSeconds($difficultyTimeout)
    while ([DateTime]::UtcNow -lt $deadline) {
        $log = Get-CombinedLogText $State.Logs
        if ($log -match 'SMOKE_FAIL|Access Violation|unhandled exception|assertion failed|snapshot[^\r\n]*rejected') {
            throw "Difficulty runtime failed: $($State.Logs)"
        }
        $result = Read-JsonIfComplete $State.Result
        if ($null -ne $result) {
            if ($result.status -ne "pass" -or $result.role -ne $State.Role -or $result.protocol -le 0) {
                throw "Invalid difficulty result: $($State.Result)"
            }
            return $result
        }
        if ($State.Process.HasExited) { throw "Difficulty process exited before result: $($State.Role)" }
        Start-Sleep -Milliseconds 200
    }
    throw "Difficulty runtime timed out: $($State.Logs)"
}

function Stop-DifficultyRole {
    param([hashtable]$State)
    if (-not (Stop-OwnedProcess -Process $State.Process -Role ("difficulty-" + $State.Role))) {
        throw "Owned difficulty process could not be stopped."
    }
}

function Require-DifficultyFrame {
    param([hashtable]$State, [string]$Name)
    if (-not (Test-Path -LiteralPath (Join-Path $State.Root ($Name + ".bmp")) -PathType Leaf)) {
        throw "Missing game renderer frame: $($State.Workflow)/$($State.Role)/$Name"
    }
}

try {
    Write-SmokeStatus ("Difficulty artifacts: {0}" -f $difficultyRoot)
    if (-not $difficultyPvPOnly) {
        $family = Start-DifficultyRole "families" "host" (Get-FreeLoopbackPort)
        [void](Wait-DifficultyRole $family)
        Stop-DifficultyRole $family
        $log = Get-CombinedLogText $family.Logs
        foreach ($name in @("waves", "whack", "boss", "match", "aquarium", "vases", "izombie", "squirrel", "finite")) {
            foreach ($level in @(1, 10)) {
                if ($log -notmatch ("SMOKE_DIFF_FAMILY family=" + $name + " difficulty=" + $level + "\b[^\r\n]*native=1")) {
                    throw "Missing native family metric: $name/$level"
                }
                Require-DifficultyFrame $family ("difficulty_" + $name + "_" + $level)
            }
        }
        if ($log -notmatch 'SMOKE_DIFF_FAMILIES_COMPLETE cases=18 native=1 fullCampaign=0') { throw "Family fixture did not finish all native cases." }
        $difficultyCases.Add(@{ workflow = "families"; native_cases = 18; status = "pass"; scope = "initialization and native hooks" })
    }
    if (-not $difficultyFamiliesOnly) {
        foreach ($level in @(1, 10)) {
            $workflow = "pvp" + $level
            $port = Get-FreeLoopbackPort
            $hostRole = Start-DifficultyRole $workflow "host" $port
            Wait-ForLogPattern -LogDirectory $hostRole.Logs -Pattern '(?im)host listening port=' `
                -Process $hostRole.Process -Role "host" -TimeoutSeconds $difficultyTimeout
            $clientRole = Start-DifficultyRole $workflow "client" $port
            $hostResult = Wait-DifficultyRole $hostRole
            $clientResult = Wait-DifficultyRole $clientRole
            if ($hostResult.protocol -ne $clientResult.protocol) { throw "Difficulty peers reported different protocols." }
            Stop-DifficultyRole $clientRole
            Stop-DifficultyRole $hostRole
            foreach ($state in @($hostRole, $clientRole)) {
                $log = Get-CombinedLogText $state.Logs
                foreach ($step in @("ZOMBIES", "SPEND", "PLANTS", "TERMINAL")) {
                    if ($log -notmatch ("SMOKE_DIFF_PVP_" + $step + " role=" + $state.Role + " difficulty=" + $level + "\b")) {
                        throw "Missing actual PvP effect: $workflow/$($state.Role)/$step"
                    }
                }
                if ($log -notmatch ("SMOKE_DIFF_PVP_OPENING role=" + $state.Role + " brains=25 sun=50 normalCost=25 basicAffordable=1 twoBasicsAffordable=0 native=1") -or
                    $log -notmatch ("SMOKE_DIFF_PVP_PASSIVE role=" + $state.Role + "[^\r\n]*delta=25")) {
                    throw "Missing native PvP opening/passive economy: $workflow/$($state.Role)"
                }
                foreach ($bungeeCase in @(0, 1)) {
                    if ($log -notmatch ("SMOKE_DIFF_PVP_BUNGEE role=" + $state.Role + " case=" + $bungeeCase + " step=placed[^\r\n]*sdl=1") -or
                        $log -notmatch ("SMOKE_DIFF_PVP_BUNGEE role=" + $state.Role + " case=" + $bungeeCase + " step=complete[^\r\n]*decoys=2")) {
                        throw "Missing actual Bungee target/empty SDL lifecycle: $workflow/$($state.Role)/$bungeeCase"
                    }
                    $bungeeFrame = if ($bungeeCase -eq 0) { "difficulty_bungee_target" } else { "difficulty_bungee_empty" }
                    if ($state.Role -eq "client") {
                        Require-DifficultyFrame $state ($bungeeFrame + "_mirror")
                        Require-DifficultyFrame $state ($bungeeFrame + "_complete_mirror")
                    }
                    else { Require-DifficultyFrame $state $bungeeFrame }
                }
                if ($state.Role -eq "host") {
                    if ($log -notmatch 'SMOKE_DIFF_PVP_DIGGER step=complete brainsDelta=0 killsDelta=0 normalHouse=1 goalOrders=2 native=1' -or
                        $log -notmatch 'SMOKE_DIFF_PVP_GOAL_ORDER first=kill kills=100 brains=4' -or
                        $log -notmatch 'SMOKE_DIFF_PVP_GOAL_ORDER first=house kills=99 brains=5') {
                        throw "Missing native Digger/house-entry/first-goal regression evidence: $workflow"
                    }
                    Require-DifficultyFrame $state "difficulty_digger_walking"
                }
                else {
                    $expectedWidth = if ($level -eq 10) { 1066 } else { 800 }
                    if ($log -notmatch ("SMOKE_DIFF_PVP_DIGGER role=client step=walking[^\r\n]*brains=0 kills=0 width=" + $expectedWidth + " height=600 mirror=1")) {
                        throw "Missing actual Digger walk-back/HUD viewport proof: $workflow"
                    }
                    Require-DifficultyFrame $state "difficulty_digger_walking_mirror"
                }
            }
            foreach ($frame in @("difficulty_zombie_support", "difficulty_plant_support", "difficulty_terminal")) { Require-DifficultyFrame $hostRole $frame }
            foreach ($frame in @("difficulty_purchase_mirror", "difficulty_plant_support_mirror", "difficulty_terminal_mirror")) { Require-DifficultyFrame $clientRole $frame }
            $hostLog = Get-CombinedLogText $hostRole.Logs
            $clientLog = Get-CombinedLogText $clientRole.Logs
            $tx = @(Get-SnapshotHashes -Text $hostLog -Marker "SMOKE_SNAPSHOT_TX")
            $rx = @(Get-SnapshotHashes -Text $clientLog -Marker "SMOKE_SNAPSHOT_APPLIED")
            $common = @($rx | Where-Object { $tx -contains $_ })
            if ($common.Count -lt 1) { throw "Difficulty pair has no common authoritative/applied wire hash." }
            $difficultyCases.Add(@{ workflow = $workflow; status = "pass"; common_hashes = $common.Count;
                scope = "native opening, timer-compressed passive award, Digger, SDL Bungee target/empty, controlled history and adaptive effects" })
        }
    }
}
catch { $difficultyError = $_.Exception.Message; Write-Host ("[Difficulty] FAIL: " + $difficultyError) -ForegroundColor Red }
finally {
    foreach ($state in $difficultyOwned) {
        if (-not (Stop-OwnedProcess -Process $state.Process -Role "difficulty-owned")) { $difficultyError = "An owned difficulty process could not be stopped." }
    }
    $summary = @{ status = $(if ($difficultyError) { "fail" } else { "pass" }); run_id = $difficultyId;
        cases = @($difficultyCases.ToArray()); error = $difficultyError; artifacts = $difficultyRoot }
    Write-Utf8NoBom -Path (Join-Path $difficultyRoot "summary.json") -Text ($summary | ConvertTo-Json -Depth 7)
}
if ($difficultyError) { exit 1 }
Write-SmokeStatus ("PASS: requested native difficulty fixtures. Artifacts: {0}" -f $difficultyRoot)
