#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateSet("chooser", "conveyor")][string]$Workflow = "chooser",
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 150
)

$commsExeArg = $GameExe
$commsArtifactsArg = $ArtifactsRoot
$commsWorkflow = $Workflow
$commsTimeout = $TimeoutSeconds
$commsScriptRoot = Split-Path -Parent $PSCommandPath
. (Join-Path $commsScriptRoot "RunCoopSmoke.ps1") -FunctionsOnly
$Background = $true
$commsRepo = Get-NormalizedPath (Join-Path $commsScriptRoot "..\..")
$commsExe = if ($commsExeArg) { Get-NormalizedPath $commsExeArg } else {
    Join-Path $commsRepo "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe"
}
if (-not (Test-Path -LiteralPath $commsExe -PathType Leaf)) { throw "Game executable not found: $commsExe" }
$commsArtifacts = if ($commsArtifactsArg) { Get-NormalizedPath $commsArtifactsArg } else {
    Join-Path $commsRepo "work\communications-smoke"
}
$commsId = [Guid]::NewGuid().ToString("N")
$commsRoot = Join-Path $commsArtifacts ("run_" + $commsId)
[void][IO.Directory]::CreateDirectory($commsRoot)
$commsOwned = [Collections.Generic.List[object]]::new()
$commsPort = Get-FreeLoopbackPort
$commsError = ""

function Start-CommunicationsRole {
    param([string]$Role)
    $root = Join-Path $commsRoot $Role
    $state = @{ Role = $Role; Root = $root; Logs = Join-Path $root "logs"; Process = $null; Result = Join-Path $root "result.json" }
    $appData = Join-Path $root "appdata"
    [void][IO.Directory]::CreateDirectory($appData)
    [void][IO.Directory]::CreateDirectory($state.Logs)
    $config = Join-Path $root "coop.ini"
    $manifest = Join-Path $root "smoke.ini"
    Write-CoopConfig -Path $config -Nickname ("Comms " + $Role) -WindowWidth 800 -WindowHeight 600
    $mode = if ($commsWorkflow -eq "chooser") { "adventure_soak" } else { "adventure_bowling_client" }
    $level = if ($commsWorkflow -eq "chooser") { 8 } else { 5 }
    Write-SmokeManifest -Path $manifest -Role $Role -Nickname ("Comms " + $Role) `
        -AppDataDirectory $appData -ConfigPath $config -LogDirectory $state.Logs -ResultPath $state.Result `
        -SelectedPort $commsPort -SelectedMode $mode -SelectedLevel $level -SelectedSeed 424242 `
        -SelectedTimeoutMs ($commsTimeout * 1000) -RequiredSnapshots 3 -SelectedSkipIntro 0
    $previous = $env:PVZ_COOP_SMOKE_COMMUNICATIONS
    try {
        $env:PVZ_COOP_SMOKE_COMMUNICATIONS = $commsWorkflow
        $state.Process = Start-SmokeProcess -Executable $commsExe -WorkingDirectory (Split-Path -Parent $commsExe) `
            -ManifestPath $manifest -Role $Role -RunId $commsId -EnableUnicodeChat $false
        $commsOwned.Add($state)
        Write-SmokeStatus ("Communications {0}/{1} PID {2}: hidden, owned game-renderer captures only." -f $commsWorkflow, $Role, $state.Process.Id)
        return $state
    }
    finally { $env:PVZ_COOP_SMOKE_COMMUNICATIONS = $previous }
}

function Wait-CommunicationsRole {
    param([hashtable]$State)
    $deadline = [DateTime]::UtcNow.AddSeconds($commsTimeout)
    while ([DateTime]::UtcNow -lt $deadline) {
        $log = Get-CombinedLogText $State.Logs
        if ($log -match 'SMOKE_FAIL|Access Violation|unhandled exception|assertion failed|snapshot.+rejected') {
            throw "Communications runtime failed: $($State.Logs)"
        }
        $result = Read-JsonIfComplete $State.Result
        if ($null -ne $result) {
            if ($result.status -ne "pass" -or $result.role -ne $State.Role -or $result.protocol -ne 34) {
                throw "Invalid communications result: $($State.Result)"
            }
            return $result
        }
        if ($State.Process.HasExited) { throw "Communications process exited before result: $($State.Role)" }
        Start-Sleep -Milliseconds 200
    }
    throw "Communications runtime timed out: $($State.Logs)"
}

try {
    Write-SmokeStatus ("Communications workflow={0}; artifacts: {1}" -f $commsWorkflow, $commsRoot)
    $commsHost = Start-CommunicationsRole "host"
    Wait-ForLogPattern -LogDirectory $commsHost.Logs -Pattern '(?im)host listening port=' `
        -Process $commsHost.Process -Role "host" -TimeoutSeconds $commsTimeout
    $commsClient = Start-CommunicationsRole "client"
    $hostResult = Wait-CommunicationsRole $commsHost
    $clientResult = Wait-CommunicationsRole $commsClient
    foreach ($state in @($commsHost, $commsClient)) {
        $log = Get-CombinedLogText $state.Logs
        $steps = if ($commsWorkflow -eq "chooser") {
            @("profile", "chooser_ping", "different_first_picks", "chooser_relative_hover")
        } else { @("conveyor_hover") }
        foreach ($step in $steps) {
            if ($log -notmatch ('SMOKE_COMMUNICATIONS_FRAME[^\r\n]*step=' + $step + '\b[^\r\n]*capture=1') -or
                -not (Test-Path -LiteralPath (Join-Path $state.Root ("communications_" + $step + ".bmp")) -PathType Leaf)) {
                throw "Missing communications rendered proof: $($state.Role)/$step"
            }
        }
        if ($commsWorkflow -eq "chooser" -and
            ($log -notmatch 'SMOKE_COMMUNICATIONS_CHOOSER_PING[^\r\n]*altRight=1 selected=0 received=1' -or
             $log -notmatch 'SMOKE_COMMUNICATIONS_CHOOSER_HOVER[^\r\n]*ownFirst=1 renderInside=1 sdl=1')) {
            throw "Missing actual chooser input/relative hover evidence: $($state.Role)"
        }
        if ($commsWorkflow -eq "conveyor" -and
            $log -notmatch 'SMOKE_COMMUNICATIONS_CONVEYOR[^\r\n]*packetId=\d+ semantic=1 renderInside=1 sdl=1') {
            throw "Missing actual conveyor semantic hover evidence: $($state.Role)"
        }
    }
}
catch { $commsError = $_.Exception.Message; Write-Host ("[Communications] FAIL: " + $commsError) -ForegroundColor Red }
finally {
    foreach ($state in @($commsOwned.ToArray()) | Sort-Object { $_.Role -eq "host" }) {
        if (-not (Stop-OwnedProcess -Process $state.Process -Role $state.Role)) { $commsError = "Owned communications process could not be stopped." }
    }
    $summary = @{ status = $(if ($commsError) { "fail" } else { "pass" }); run_id = $commsId;
        workflow = $commsWorkflow; error = $commsError; artifacts = $commsRoot }
    Write-Utf8NoBom -Path (Join-Path $commsRoot "summary.json") -Text ($summary | ConvertTo-Json -Depth 5)
}
if ($commsError) { throw $commsError }
Write-SmokeStatus ("PASS communications {0}; artifacts: {1}" -f $commsWorkflow, $commsRoot)
