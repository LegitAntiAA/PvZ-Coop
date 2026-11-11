#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 180,
    [switch]$SkipBrowser
)

$nativeRequestedExe = $GameExe
$nativeRequestedArtifacts = $ArtifactsRoot
$nativeTimeout = $TimeoutSeconds
$nativeSkipBrowser = [bool]$SkipBrowser
$nativeScriptRoot = Split-Path -Parent $PSCommandPath
. (Join-Path $nativeScriptRoot "RunCoopSmoke.ps1") -FunctionsOnly
$Background = $true
$repoRoot = Get-NormalizedPath -Path (Join-Path $nativeScriptRoot "..\..")
$nativeExe = if ($nativeRequestedExe) { Get-NormalizedPath $nativeRequestedExe } else {
    Join-Path $repoRoot "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe"
}
if (-not (Test-Path -LiteralPath $nativeExe -PathType Leaf)) { throw "Game executable not found: $nativeExe" }
$nativeArtifacts = if ($nativeRequestedArtifacts) { Get-NormalizedPath $nativeRequestedArtifacts } else {
    Join-Path $repoRoot "work\native-ui-smoke"
}
$nativeRunId = [Guid]::NewGuid().ToString("N")
$nativeRoot = Join-Path $nativeArtifacts ("run_" + $nativeRunId)
[void][IO.Directory]::CreateDirectory($nativeRoot)
$nativeCases = [Collections.Generic.List[object]]::new()
$nativeOwned = [Collections.Generic.List[object]]::new()
$nativeError = ""

function New-NativeRole {
    param([string]$Folder, [string]$Role, [string]$Nickname)
    $root = Join-Path $nativeRoot $Folder
    $state = @{
        Root = $root; Role = $Role; Nick = $Nickname
        Config = Join-Path $root "coop.ini"
        AppData = Join-Path $root "appdata"
        Logs = Join-Path $root "logs"
        Process = $null; Manifest = ""; Result = ""
    }
    [void][IO.Directory]::CreateDirectory($state.AppData)
    [void][IO.Directory]::CreateDirectory($state.Logs)
    Write-CoopConfig -Path $state.Config -Nickname $Nickname -WindowWidth 800 -WindowHeight 600
    return $state
}

function Start-NativeRole {
    param([hashtable]$State, [string]$Workflow, [int]$Port, [string]$Expected = "")
    $State.Manifest = Join-Path $State.Root ("smoke-" + $Workflow + ".ini")
    $State.Result = Join-Path $State.Root ("result-" + $Workflow + ".json")
    Write-SmokeManifest -Path $State.Manifest -Role $State.Role -Nickname $State.Nick `
        -AppDataDirectory $State.AppData -ConfigPath $State.Config -LogDirectory $State.Logs `
        -ResultPath $State.Result -SelectedPort $Port -SelectedMode "adventure" -SelectedLevel 1 `
        -SelectedSeed 424242 -SelectedTimeoutMs ($nativeTimeout * 1000) -RequiredSnapshots 1 -SelectedSkipIntro 0
    $oldWorkflow = $env:PVZ_COOP_SMOKE_NATIVE_UI
    $oldExpected = $env:PVZ_COOP_SMOKE_APPEARANCE_EXPECTED
    try {
        $env:PVZ_COOP_SMOKE_NATIVE_UI = $Workflow
        $env:PVZ_COOP_SMOKE_APPEARANCE_EXPECTED = $Expected
        $State.Process = Start-SmokeProcess -Executable $nativeExe -WorkingDirectory (Split-Path -Parent $nativeExe) `
            -ManifestPath $State.Manifest -Role $State.Role -RunId $nativeRunId -EnableUnicodeChat $false
        $nativeOwned.Add($State.Process)
        Write-SmokeStatus ("Native {0}/{1} PID {2}: hidden window, game renderer captures only." -f $Workflow, $State.Role, $State.Process.Id)
        $ledger = @($nativeOwned | ForEach-Object { @{ pid = $_.Id; executable = $nativeExe } })
        Write-Utf8NoBom -Path (Join-Path $nativeRoot "pids.json") -Text ($ledger | ConvertTo-Json -Depth 4)
    }
    finally {
        $env:PVZ_COOP_SMOKE_NATIVE_UI = $oldWorkflow
        $env:PVZ_COOP_SMOKE_APPEARANCE_EXPECTED = $oldExpected
    }
}

function Wait-NativeRole {
    param([hashtable]$State)
    $deadline = [DateTime]::UtcNow.AddSeconds($nativeTimeout)
    while ([DateTime]::UtcNow -lt $deadline) {
        $result = Read-JsonIfComplete -Path $State.Result
        $log = Get-CombinedLogText -LogDirectory $State.Logs
        if ($log -match 'SMOKE_FAIL|Access Violation|unhandled exception|assertion failed') {
            throw ("Native {0} runtime failure; inspect {1}" -f $State.Role, $State.Logs)
        }
        if ($null -ne $result) {
            if ($result.status -ne "pass" -or $result.role -ne $State.Role -or $result.protocol -le 0) {
                throw ("Native result rejected: {0}" -f ($result | ConvertTo-Json -Compress))
            }
            return $result
        }
        if ($State.Process.HasExited) { throw ("Native {0} exited before its result: {1}" -f $State.Role, $State.Process.ExitCode) }
        Start-Sleep -Milliseconds 200
    }
    throw ("Native {0} timed out; inspect {1}" -f $State.Role, $State.Logs)
}

function Stop-NativeRole {
    param([hashtable]$State)
    if (-not (Stop-OwnedProcess -Process $State.Process -Role $State.Role)) { throw "Owned native process did not stop." }
}

function Require-NativeProof {
    param([hashtable]$State, [string[]]$Steps)
    $log = Get-CombinedLogText -LogDirectory $State.Logs
    foreach ($step in $Steps) {
        if ($log -notmatch ('SMOKE_NATIVE_UI[^\r\n]*step=' + [regex]::Escape($step) + '\b[^\r\n]*capture=1')) {
            throw ("Missing native proof: {0}/{1}" -f $State.Role, $step)
        }
        if (-not (Test-Path -LiteralPath (Join-Path $State.Root ("native_" + $step + ".bmp")) -PathType Leaf)) {
            throw ("Missing native frame: {0}/{1}" -f $State.Role, $step)
        }
    }
}

try {
    Write-SmokeStatus ("Native UI artifacts: {0}" -f $nativeRoot)
    $nativeHost = New-NativeRole "appearance\host" "host" "[UI Host]"
    $nativeClient = New-NativeRole "appearance\client" "client" "[UI Client]"
    $nativePort = Get-FreeLoopbackPort
    Start-NativeRole $nativeHost "appearance" $nativePort
    Wait-ForLogPattern -LogDirectory $nativeHost.Logs -Pattern '(?im)host listening port=' `
        -Process $nativeHost.Process -Role "host" -TimeoutSeconds $nativeTimeout
    Start-NativeRole $nativeClient "appearance" $nativePort
    $hostProof = Wait-NativeRole $nativeHost
    $clientProof = Wait-NativeRole $nativeClient
    if ($hostProof.protocol -ne $clientProof.protocol) { throw "Native peers reported different protocols." }
    Stop-NativeRole $nativeClient
    Stop-NativeRole $nativeHost
    $steps = @("override_menu", "override_minigames_open", "override_minigames_closed", "options",
        "appearance_before", "appearance_changed", "appearance_closed", "appearance_reopened", "appearance_complete")
    foreach ($role in @($nativeHost, $nativeClient)) { Require-NativeProof $role $steps }
    $expected = @{}
    foreach ($role in @($nativeHost, $nativeClient)) {
        $log = Get-CombinedLogText $role.Logs
        $match = [regex]::Match($log, 'SMOKE_NATIVE_SAVED[^\r\n]*held=(\d+) hotbar=(\d+) outline=(\d+) sdl=1 saved=1 animated=1')
        if (-not $match.Success) { throw "Native slider changes were not saved through the UI." }
        $expected[$role.Role] = "{0},{1},{2}" -f $match.Groups[1].Value, $match.Groups[2].Value, $match.Groups[3].Value
    }
    $nativeCases.Add(@{ workflow = "appearance"; status = "pass"; expected = $expected })

    $nativePort = Get-FreeLoopbackPort
    Start-NativeRole $nativeHost "appearance-restart" $nativePort $expected.host
    Wait-ForLogPattern -LogDirectory $nativeHost.Logs -Pattern '(?im)host listening port=' `
        -Process $nativeHost.Process -Role "host" -TimeoutSeconds $nativeTimeout
    Start-NativeRole $nativeClient "appearance-restart" $nativePort $expected.client
    [void](Wait-NativeRole $nativeHost)
    [void](Wait-NativeRole $nativeClient)
    Stop-NativeRole $nativeClient
    Stop-NativeRole $nativeHost
    foreach ($role in @($nativeHost, $nativeClient)) { Require-NativeProof $role @("appearance_restart") }
    $nativeCases.Add(@{ workflow = "appearance-restart"; status = "pass" })

    if (-not $nativeSkipBrowser) {
        $browserHost = New-NativeRole "browser\host" "host" "[UI RoomHost]"
        $browserClient = New-NativeRole "browser\client" "client" "[UI RoomGuest]"
        Start-NativeRole $browserHost "browser" (Get-FreeLoopbackPort)
        Start-NativeRole $browserClient "browser" (Get-FreeLoopbackPort)
        [void](Wait-NativeRole $browserHost)
        [void](Wait-NativeRole $browserClient)
        $probe = New-NativeRole "browser\probe" "client" "[UI FullProbe]"
        Start-NativeRole $probe "browser-probe" (Get-FreeLoopbackPort)
        [void](Wait-NativeRole $probe)
        Stop-NativeRole $probe
        Stop-NativeRole $browserClient
        Stop-NativeRole $browserHost
        Require-NativeProof $browserHost @("browser_ready", "browser_host_room")
        Require-NativeProof $browserClient @("browser_populated", "browser_wrong_password", "browser_joined")
        Require-NativeProof $probe @("browser_full_selected", "browser_room_full", "browser_full_closed")
        $nativeCases.Add(@{ workflow = "browser"; status = "pass"; backend = "compiled shared service" })
    }
}
catch { $nativeError = $_.Exception.Message; Write-Host ("[NativeUI] FAIL: " + $nativeError) -ForegroundColor Red }
finally {
    foreach ($process in $nativeOwned) {
        if (-not (Stop-OwnedProcess -Process $process -Role "native-owned")) { $nativeError = "An owned process could not be stopped." }
    }
    $status = if ($nativeError) { "fail" } else { "pass" }
    $summary = @{ status = $status; run_id = $nativeRunId; workflows = @($nativeCases.ToArray()); error = $nativeError; artifacts = $nativeRoot }
    Write-Utf8NoBom -Path (Join-Path $nativeRoot "summary.json") -Text ($summary | ConvertTo-Json -Depth 7)
}
if ($nativeError) { exit 1 }
Write-SmokeStatus ("PASS: native UI change/reopen/restart and requested browser cases. Artifacts: {0}" -f $nativeRoot)
