#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateSet("None", "MalformedStart", "RngFailure")][string]$Fault = "None",
    [ValidateRange(1, 10)][int]$Difficulty = 10,
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 180
)

$mysteryExeArg = $GameExe
$mysteryArtifactsArg = $ArtifactsRoot
$mysteryFault = switch ($Fault) { "MalformedStart" { "MalformedStart" } "RngFailure" { "RngFailure" } default { "None" } }
$mysteryDifficulty = $Difficulty
$mysteryTimeout = $TimeoutSeconds
$mysteryScriptRoot = Split-Path -Parent $PSCommandPath
. (Join-Path $mysteryScriptRoot "RunCoopSmoke.ps1") -FunctionsOnly
$Background = $true
if ($mysteryFault -ne "None" -and $env:PVZ_COOP_SMOKE_MYSTERY_OBSERVER -in @("IntroJoin", "PlayingKick")) {
    throw "Mystery fault and observer workflows must run separately."
}
$mysteryRepo = Get-NormalizedPath (Join-Path $mysteryScriptRoot "..\..")
$mysteryExe = if ($mysteryExeArg) { Get-NormalizedPath $mysteryExeArg } else {
    Join-Path $mysteryRepo "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe"
}
if (-not (Test-Path -LiteralPath $mysteryExe -PathType Leaf)) { throw "Game executable not found: $mysteryExe" }
$mysteryArtifacts = if ($mysteryArtifactsArg) { Get-NormalizedPath $mysteryArtifactsArg } else {
    Join-Path $mysteryRepo "work\mystery-smoke"
}
$mysteryId = [Guid]::NewGuid().ToString("N")
$mysteryRoot = Join-Path $mysteryArtifacts ("run_" + $mysteryId)
[void][IO.Directory]::CreateDirectory($mysteryRoot)
$mysteryRoles = [Collections.Generic.List[object]]::new()
$mysteryPort = Get-FreeLoopbackPort
$mysteryError = ""

function Start-MysteryRole {
    param([string]$Role)
    $root = Join-Path $mysteryRoot $Role
    $state = @{ Role = $Role; Root = $root; Logs = Join-Path $root "logs"; Process = $null; Result = Join-Path $root "result.json" }
    $appData = Join-Path $root "appdata"
    [void][IO.Directory]::CreateDirectory($appData)
    [void][IO.Directory]::CreateDirectory($state.Logs)
    $config = Join-Path $root "coop.ini"
    $manifest = Join-Path $root "smoke.ini"
    Write-CoopConfig -Path $config -Nickname ("Mystery " + $Role) -WindowWidth 800 -WindowHeight 600
    Write-SmokeManifest -Path $manifest -Role $Role -Nickname ("Mystery " + $Role) `
        -AppDataDirectory $appData -ConfigPath $config -LogDirectory $state.Logs -ResultPath $state.Result `
        -SelectedPort $mysteryPort -SelectedMode "adventure_soak" -SelectedLevel 8 -SelectedSeed 424242 `
        -SelectedTimeoutMs ($mysteryTimeout * 1000) -RequiredSnapshots 3 -SelectedSkipIntro 0
    $oldMystery = $env:PVZ_COOP_SMOKE_MYSTERY
    $oldDifficulty = $env:PVZ_COOP_SMOKE_MYSTERY_DIFFICULTY
    $oldFault = $env:PVZ_COOP_SMOKE_MYSTERY_FAULT
    try {
        $env:PVZ_COOP_SMOKE_MYSTERY = "1"
        $env:PVZ_COOP_SMOKE_MYSTERY_DIFFICULTY = [string]$mysteryDifficulty
        $env:PVZ_COOP_SMOKE_MYSTERY_FAULT = $(if ($mysteryFault -eq "None") { "" } else { $mysteryFault })
        $state.Process = Start-SmokeProcess -Executable $mysteryExe -WorkingDirectory (Split-Path -Parent $mysteryExe) `
            -ManifestPath $manifest -Role $Role -RunId $mysteryId -EnableUnicodeChat $false
        $mysteryRoles.Add($state)
        Write-SmokeStatus ("Mystery {0} PID {1}: hidden, isolated game renderer only." -f $Role, $state.Process.Id)
        return $state
    }
    finally {
        $env:PVZ_COOP_SMOKE_MYSTERY = $oldMystery
        $env:PVZ_COOP_SMOKE_MYSTERY_DIFFICULTY = $oldDifficulty
        $env:PVZ_COOP_SMOKE_MYSTERY_FAULT = $oldFault
    }
}

function Wait-MysteryRole {
    param([hashtable]$State)
    $deadline = [DateTime]::UtcNow.AddSeconds($mysteryTimeout)
    while ([DateTime]::UtcNow -lt $deadline) {
        $log = Get-CombinedLogText $State.Logs
        if ($log -match 'SMOKE_FAIL|Access Violation|unhandled exception|assertion failed') { throw "Mystery runtime failed: $($State.Logs)" }
        $result = Read-JsonIfComplete $State.Result
        if ($null -ne $result) {
            if ($result.status -ne "pass" -or $result.role -ne $State.Role -or $result.protocol -le 0) { throw "Invalid Mystery result: $($State.Result)" }
            return $result
        }
        if ($State.Process.HasExited) { throw "Mystery process exited before result: $($State.Role)" }
        Start-Sleep -Milliseconds 200
    }
    throw "Mystery runtime timed out: $($State.Logs)"
}

try {
    Write-SmokeStatus ("Mystery fault={0}, artifacts: {1}" -f $mysteryFault, $mysteryRoot)
    $mysteryHost = Start-MysteryRole "host"
    Wait-ForLogPattern -LogDirectory $mysteryHost.Logs -Pattern '(?im)host listening port=' `
        -Process $mysteryHost.Process -Role "host" -TimeoutSeconds $mysteryTimeout
    $mysteryClient = Start-MysteryRole "client"
    $hostResult = Wait-MysteryRole $mysteryHost
    $clientResult = Wait-MysteryRole $mysteryClient
    if ($hostResult.protocol -ne $clientResult.protocol) { throw "Mystery peers reported different protocols." }
    foreach ($state in @($mysteryHost, $mysteryClient)) {
        $log = Get-CombinedLogText $state.Logs
        $steps = @("profile", "both_secret", "cancelled", "mixed_ready", "revealed", "discount", "spent")
        if ($mysteryFault -eq "RngFailure") { $steps += @("rng_reopened", "mixed_ready_after_rng") }
        foreach ($step in $steps) {
            if ($log -notmatch ('SMOKE_MYSTERY_FRAME[^\r\n]*step=' + $step + '\b[^\r\n]*capture=1') -or
                -not (Test-Path -LiteralPath (Join-Path $state.Root ("mystery_" + $step + ".bmp")) -PathType Leaf)) {
                throw "Missing Mystery rendered proof: $($state.Role)/$step"
            }
        }
        if (-not (Test-Path -LiteralPath (Join-Path $state.Root "mystery_intro_secret.bmp")) -or
            $log -notmatch 'SMOKE_MYSTERY_WARNING_FIXTURE[^\r\n]*fixedSun=0 manualSun=0' -or
            $log -notmatch 'SMOKE_MYSTERY_SECRET[^\r\n]*pending=5 realTypes=0 fixed=3 ids=1 capture=1' -or
            $log -notmatch 'SMOKE_MYSTERY_REVEAL[^\r\n]*stableIDs=5 overlap=1 imitator=1' -or
            $log -notmatch 'SMOKE_MYSTERY_PURCHASE[^\r\n]*displayed=\d+ spent=\d+ cooldown=\d+ origin=\d+ sdl=1') {
            throw "Missing Mystery secrecy/reveal/purchase evidence: $($state.Role)"
        }
        if ($log -match 'SMOKE_MYSTERY_WARNING[^\r\n]*sun=0 sdlYes=1 capture=1' -and
            -not (Test-Path -LiteralPath (Join-Path $state.Root "mystery_stock_warning.bmp"))) {
            throw "Missing rendered native warning confirmation: $($state.Role)"
        }
        $cooldown = [regex]::Match($log, 'SMOKE_MYSTERY_COOLDOWN[^\r\n]*time=(\d+) expected=(\d+) first=(\d+) later=(\d+) progress=1')
        if (-not $cooldown.Success -or [int]$cooldown.Groups[1].Value -le 0 -or
            [int]$cooldown.Groups[1].Value -ne [int]$cooldown.Groups[2].Value -or
            [int]$cooldown.Groups[4].Value -le [int]$cooldown.Groups[3].Value) {
            throw "Missing positive, correct and advancing native cooldown: $($state.Role)"
        }
        if ($mysteryFault -eq "MalformedStart") {
            if ($log -notmatch 'SMOKE_MYSTERY_FAULT_RECOVER[^\r\n]*kind=MalformedStart toggleOffOn=1 chooserOpen=1') {
                throw "Mystery chooser did not accept real off/on input after the bad START: $($state.Role)"
            }
            if ($state.Role -eq "client" -and
                ($log -notmatch 'CHOOSER_START rejected before generation commit: malformed final bank' -or
                 $log -notmatch 'SMOKE_MYSTERY_FAULT_REJECT kind=MalformedStart unchanged=1 chooserOpen=1 epoch=\d+' -or
                 -not (Test-Path -LiteralPath (Join-Path $state.Root "mystery_malformed_rejected.bmp") -PathType Leaf))) {
                throw "Missing unchanged client chooser/epoch/bank/readiness after malformed START."
            }
            if ($state.Role -eq "host" -and
                [regex]::Matches($log, 'SMOKE_MYSTERY_FAULT_INJECT kind=MalformedStart bytes=3').Count -ne 1) {
                throw "Malformed START was not injected exactly once."
            }
        }
        elseif ($mysteryFault -eq "RngFailure") {
            $reopen = [regex]::Match($log, 'SMOKE_MYSTERY_FAULT_REOPEN[^\r\n]*kind=RngFailure oldEpoch=(\d+) epoch=(\d+) mask=0 ready=0 mutable=0 noPlaying=1')
            if (-not $reopen.Success -or [uint32]$reopen.Groups[2].Value -le [uint32]$reopen.Groups[1].Value -or
                $log -notmatch 'SMOKE_MYSTERY_FAULT_RECOVER[^\r\n]*kind=RngFailure freshPick=1') {
                throw "Missing fresh, empty chooser generation and real reselection after RNG failure: $($state.Role)"
            }
            if ($state.Role -eq "host" -and
                ([regex]::Matches($log, 'mystery private RNG unavailable; no packets committed').Count -ne 1 -or
                 [regex]::Matches($log, 'CHOOSER_COMMIT[^\r\n]*total=10 bank=10 flight=0').Count -ne 2 -or
                 [regex]::Matches($log, 'host entering gameplay; broadcasting barrier').Count -ne 1 -or
                 $log.IndexOf('host entering gameplay; broadcasting barrier') -lt $log.IndexOf('kind=RngFailure freshPick=1'))) {
                throw "RNG recovery did not produce exactly one failure, two settled commits and one later PLAYING barrier."
            }
        }
    }
}
catch { $mysteryError = $_.Exception.Message; Write-Host ("[Mystery] FAIL: " + $mysteryError) -ForegroundColor Red }
finally {
    foreach ($state in @($mysteryRoles.ToArray()) | Sort-Object { $_.Role -eq "host" }) {
        if (-not (Stop-OwnedProcess -Process $state.Process -Role $state.Role)) { $mysteryError = "Owned Mystery process could not be stopped." }
    }
    $summary = @{ status = $(if ($mysteryError) { "fail" } else { "pass" }); run_id = $mysteryId;
        difficulty = $mysteryDifficulty; fault = $mysteryFault; error = $mysteryError; artifacts = $mysteryRoot }
    Write-Utf8NoBom -Path (Join-Path $mysteryRoot "summary.json") -Text ($summary | ConvertTo-Json -Depth 5)
}
if ($mysteryError) { exit 1 }
Write-SmokeStatus ("PASS: actual Mystery toggle/cancel/ready/secret/reveal/discounted purchase; fault={0}. Artifacts: {1}" -f $mysteryFault, $mysteryRoot)
