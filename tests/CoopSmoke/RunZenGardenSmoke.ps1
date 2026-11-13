#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 240
)

$zenExeArgument = $GameExe
$zenArtifactsArgument = $ArtifactsRoot
$zenTimeout = $TimeoutSeconds
$zenScriptRoot = Split-Path -Parent $PSCommandPath
. (Join-Path $zenScriptRoot "RunCoopSmoke.ps1") -FunctionsOnly
$Background = $true
$zenRepo = Get-NormalizedPath (Join-Path $zenScriptRoot "..\..")
$zenExe = if ($zenExeArgument) { Get-NormalizedPath $zenExeArgument } else {
    Join-Path $zenRepo "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe"
}
if (-not (Test-Path -LiteralPath $zenExe -PathType Leaf)) { throw "Game executable not found: $zenExe" }
$zenArtifacts = if ($zenArtifactsArgument) { Get-NormalizedPath $zenArtifactsArgument } else {
    Join-Path $zenRepo "work\zen-garden-smoke"
}
$zenRunId = [Guid]::NewGuid().ToString("N")
$zenRoot = Join-Path $zenArtifacts ("run_" + $zenRunId)
[void][IO.Directory]::CreateDirectory($zenRoot)
$zenPort = Get-FreeLoopbackPort
$zenRoles = [Collections.Generic.List[object]]::new()
$zenError = ""
$zenResults = @{}
$zenSteps = @("profile", "needs", "water", "fertilizer", "spray", "music", "glove", "wheelbarrow_full",
    "wheelbarrow_empty", "snail_awake", "snail_chocolate", "plant_chocolate", "coin", "mushroom", "aquarium", "main", "complete")

function Start-ZenRole {
    param([string]$Role)
    $root = Join-Path $zenRoot $Role
    $state = @{ Role = $Role; Root = $root; Logs = Join-Path $root "logs"; Process = $null;
        Result = Join-Path $root "result.json" }
    $appData = Join-Path $root "appdata"
    [void][IO.Directory]::CreateDirectory($appData)
    [void][IO.Directory]::CreateDirectory($state.Logs)
    $config = Join-Path $root "coop.ini"
    $manifest = Join-Path $root "smoke.ini"
    Write-CoopConfig -Path $config -Nickname ("Zen " + $Role) -WindowWidth 800 -WindowHeight 600
    Write-SmokeManifest -Path $manifest -Role $Role -Nickname ("Zen " + $Role) `
        -AppDataDirectory $appData -ConfigPath $config -LogDirectory $state.Logs -ResultPath $state.Result `
        -SelectedPort $zenPort -SelectedMode "adventure" -SelectedLevel 1 -SelectedSeed 424242 `
        -SelectedTimeoutMs ($zenTimeout * 1000) -RequiredSnapshots 3 -SelectedSkipIntro 0
    $previousZen = $env:PVZ_COOP_SMOKE_ZEN_GARDEN
    try {
        $env:PVZ_COOP_SMOKE_ZEN_GARDEN = "1"
        $state.Process = Start-SmokeProcess -Executable $zenExe -WorkingDirectory (Split-Path -Parent $zenExe) `
            -ManifestPath $manifest -Role $Role -RunId $zenRunId -EnableUnicodeChat $false
        $zenRoles.Add($state)
        Write-SmokeStatus ("Zen {0} PID {1}: hidden game, isolated profile, renderer capture only." -f $Role, $state.Process.Id)
        return $state
    }
    finally { $env:PVZ_COOP_SMOKE_ZEN_GARDEN = $previousZen }
}

try {
    Write-SmokeStatus ("Zen shared-area workflow artifacts: " + $zenRoot)
    $zenHost = Start-ZenRole "host"
    Wait-ForLogPattern -LogDirectory $zenHost.Logs -Pattern '(?im)host listening port=' `
        -Process $zenHost.Process -Role "host" -TimeoutSeconds $zenTimeout
    $zenClient = Start-ZenRole "client"
    $deadline = [DateTime]::UtcNow.AddSeconds($zenTimeout)
    while ($zenResults.Count -lt 2 -and [DateTime]::UtcNow -lt $deadline) {
        foreach ($state in @($zenRoles.ToArray())) {
            $log = Get-CombinedLogText $state.Logs
            if ($log -match 'SMOKE_FAIL|Access Violation|unhandled exception|assertion failed|snapshot[^\r\n]*(rejected|truncated|ignored trailing)') {
                throw "Zen runtime failed: $($state.Role), inspect $($state.Logs)"
            }
            if ($zenResults.ContainsKey($state.Role)) { continue }
            $result = Read-JsonIfComplete $state.Result
            if ($null -ne $result) {
                if ($result.status -ne "pass" -or $result.role -ne $state.Role -or $result.protocol -le 0) {
                    throw "Invalid Zen result: $($state.Result)"
                }
                $zenResults[$state.Role] = $result
            }
            elseif ($state.Process.HasExited) { throw "Zen process exited before its result: $($state.Role)" }
        }
        if ($zenResults.Count -lt 2) { Start-Sleep -Milliseconds 200 }
    }
    if ($zenResults.Count -ne 2) { throw "Zen shared-area workflow timed out." }
    if ($zenResults.host.protocol -ne $zenResults.client.protocol) { throw "Zen peers used different protocols." }
    if ($zenResults.host.snapshotsSent -lt 3 -or $zenResults.client.snapshotsApplied -lt 3) {
        throw "Zen did not prove actual host snapshots and client application."
    }
    $zenClientLog = Get-CombinedLogText $zenClient.Logs
    $zenHostLog = Get-CombinedLogText $zenHost.Logs
    $zenCoinClick = [regex]::Match($zenClientLog,
        'SMOKE_ZEN_INPUT role=client step=coin id=(\d+) x=(\d+) y=(\d+) before=(\d+) sdl=1')
    if (-not $zenCoinClick.Success) { throw "Missing actual client SDL click on the identified Zen coin." }
    $zenCoinId = $zenCoinClick.Groups[1].Value
    $zenCoinX = $zenCoinClick.Groups[2].Value
    $zenCoinY = $zenCoinClick.Groups[3].Value
    $zenCoinBefore = [int]$zenCoinClick.Groups[4].Value
    if ($zenHostLog -notmatch ('mouse apply slot=1[^\r\n]*action=1[^\r\n]*screen=\(' + $zenCoinX + ',' + $zenCoinY + '\)')) {
        throw "The host did not apply the client's actual Zen coin click coordinates."
    }
    foreach ($roleLog in @($zenHostLog, $zenClientLog)) {
        $coinResult = [regex]::Match($roleLog, ('SMOKE_ZEN_COIN[^\r\n]*id=' + $zenCoinId + '\b before=(\d+) after=(\d+) collected=1'))
        if (-not $coinResult.Success -or [int]$coinResult.Groups[2].Value -le [int]$coinResult.Groups[1].Value -or
            [int]$coinResult.Groups[2].Value -le $zenCoinBefore -or
            ($roleLog -eq $zenClientLog -and [int]$coinResult.Groups[1].Value -ne $zenCoinBefore)) {
            throw "The identified client-clicked Zen coin lacks a mirrored currency increase."
        }
    }
    foreach ($state in @($zenRoles.ToArray())) {
        $log = Get-CombinedLogText $state.Logs
        foreach ($step in $zenSteps) {
            if ($log -notmatch ('SMOKE_ZEN_FRAME[^\r\n]*step=' + $step + '\b[^\r\n]*capture=1') -or
                -not (Test-Path -LiteralPath (Join-Path $state.Root ("zen_" + $step + ".bmp")) -PathType Leaf)) {
                throw "Missing Zen actual rendered checkpoint: $($state.Role)/$step"
            }
        }
        foreach ($need in 1..4) {
            if ($log -notmatch ('SMOKE_ZEN_OVERLAY[^\r\n]*stage=1[^\r\n]*need=' + $need + '\b[^\r\n]*drawn=1')) {
                throw "Missing native Zen need icon render: $($state.Role), need=$need"
            }
        }
        foreach ($step in @("water", "fertilizer", "spray", "music")) {
            if ($log -notmatch ('SMOKE_ZEN_TOOL[^\r\n]*step=' + $step + '\b[^\r\n]*capture=1') -or
                -not (Test-Path -LiteralPath (Join-Path $state.Root ("zen_tool_" + $step + ".bmp")) -PathType Leaf)) {
                throw "Missing active native Zen tool visual: $($state.Role)/$step"
            }
        }
        if ($state.Role -eq "client") {
            foreach ($step in @("water", "fertilizer", "spray", "music", "glove", "wheelbarrow_full", "wheelbarrow_empty",
                "snail_awake", "snail_chocolate", "plant_chocolate", "coin", "mushroom", "aquarium", "main", "complete")) {
                if ($log -notmatch ('SMOKE_ZEN_INPUT role=client step=' + $step + '\b[^\r\n]*sdl=1')) {
                    throw "Missing client SDL input proof: $step"
                }
            }
        }
        elseif ($log -notmatch 'mouse apply slot=1[^\r\n]*action=1') {
            throw "Missing authoritative host application of client actions."
        }
    }
}
catch { $zenError = $_.Exception.Message; Write-Host ("[Zen] FAIL: " + $zenError) -ForegroundColor Red }
finally {
    foreach ($state in @($zenRoles.ToArray()) | Sort-Object { $_.Role -eq "host" }) {
        if (-not (Stop-OwnedProcess -Process $state.Process -Role $state.Role)) { $zenError = "Owned Zen process could not be stopped." }
    }
    $zenCompleted = @{}
    $zenToolVisuals = @{}
    foreach ($state in @($zenRoles.ToArray())) {
        $roleLog = Get-CombinedLogText $state.Logs
        $zenCompleted[$state.Role] = [regex]::Matches($roleLog, 'SMOKE_ZEN_FRAME[^\r\n]*capture=1').Count
        $zenToolVisuals[$state.Role] = [regex]::Matches($roleLog, 'SMOKE_ZEN_TOOL[^\r\n]*capture=1').Count
    }
    $summary = @{ status = $(if ($zenError) { "fail" } else { "pass" }); run_id = $zenRunId;
        error = $zenError; artifacts = $zenRoot; expected_checkpoints_per_peer = $zenSteps.Count;
        completed_checkpoints = $zenCompleted; active_tool_visuals = $zenToolVisuals }
    Write-Utf8NoBom -Path (Join-Path $zenRoot "summary.json") -Text ($summary | ConvertTo-Json -Depth 5)
}
if ($zenError) { exit 1 }
Write-SmokeStatus ("PASS: actual client Zen tools, movement, wheelbarrow, snail, chocolate, coins and garden changes. Artifacts: " + $zenRoot)
