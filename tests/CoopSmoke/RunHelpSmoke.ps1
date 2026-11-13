#requires -Version 5.1
[CmdletBinding()]
param([string]$GameExe = "", [string]$ArtifactsRoot = "", [ValidateRange(30,300)][int]$TimeoutSeconds = 120)

$ErrorActionPreference = "Stop"
$helpExeArg = $GameExe
$helpArtifactsArg = $ArtifactsRoot
$helpTimeout = $TimeoutSeconds
$helpScripts = $PSScriptRoot
. (Join-Path $helpScripts "RunCoopSmoke.ps1") -FunctionsOnly
$Background = $true
$helpRepo = Get-NormalizedPath (Join-Path $helpScripts "..\..")
$helpExe = if ($helpExeArg) { Get-NormalizedPath $helpExeArg } else { Join-Path $helpRepo "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe" }
$helpRoot = if ($helpArtifactsArg) { Get-NormalizedPath $helpArtifactsArg } else { Join-Path $helpRepo "work\help-smoke" }
$helpRunId = [Guid]::NewGuid().ToString("N")
$helpRun = Join-Path $helpRoot ("run_" + $helpRunId)
$helpStates = @()
$helpFailure = ""
$helpOld = $env:PVZ_COOP_SMOKE_HELP
try {
    $env:PVZ_COOP_SMOKE_HELP = "1"
    $helpPort = Get-FreeLoopbackPort
    foreach ($helpRole in @("host", "client")) {
        $helpDir = Join-Path $helpRun $helpRole
        $helpData = Join-Path $helpDir "appdata"
        $helpLogs = Join-Path $helpDir "logs"
        [void][IO.Directory]::CreateDirectory($helpData)
        [void][IO.Directory]::CreateDirectory($helpLogs)
        $helpConfig = Join-Path $helpDir "coop.ini"
        $helpWidth = if ($helpRole -eq "host") { 1066 } else { 800 }
        Write-CoopConfig -Path $helpConfig -Nickname ("Help_" + $helpRole) -WindowWidth $helpWidth -WindowHeight 600
        $helpManifest = Join-Path $helpDir "smoke.ini"
        $helpResult = Join-Path $helpDir "result.json"
        Write-SmokeManifest -Path $helpManifest -Role $helpRole -Nickname ("Help_" + $helpRole) `
            -AppDataDirectory $helpData -ConfigPath $helpConfig -LogDirectory $helpLogs -ResultPath $helpResult `
            -SelectedPort $helpPort -SelectedMode "adventure" -SelectedLevel 1 -SelectedSeed 424242 `
            -SelectedTimeoutMs ($helpTimeout * 1000) -RequiredSnapshots 1 -SelectedSkipIntro 0
        $helpProcess = Start-SmokeProcess -Executable $helpExe -WorkingDirectory (Split-Path -Parent $helpExe) `
            -ManifestPath $helpManifest -Role $helpRole -RunId $helpRunId -EnableUnicodeChat $false
        $helpStates += @{ Role = $helpRole; Root = $helpDir; Logs = $helpLogs; Result = $helpResult; Process = $helpProcess }
        if ($helpRole -eq "host") {
            Wait-ForLogPattern -LogDirectory $helpLogs -Pattern '(?im)host listening port=' -Process $helpProcess -Role "host" -TimeoutSeconds $helpTimeout
        }
    }
    $helpDeadline = [DateTime]::UtcNow.AddSeconds($helpTimeout)
    do {
        $helpDone = $true
        foreach ($helpState in $helpStates) {
            $helpLog = Get-CombinedLogText -LogDirectory $helpState.Logs
            if ($helpLog -match 'SMOKE_FAIL|Access Violation|unhandled exception|assertion failed') { throw "Help runtime failure: $($helpState.Role)" }
            $helpProof = Read-JsonIfComplete -Path $helpState.Result
            if ($null -eq $helpProof) {
                $helpDone = $false
                if ($helpState.Process.HasExited) { throw "Help process exited without result: $($helpState.Role)" }
            } elseif ($helpProof.status -ne "pass") { throw "Help result failed: $($helpState.Role)" }
        }
        if ($helpDone) { break }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $helpDeadline)
    if (-not $helpDone) { throw "Help smoke timed out" }
    foreach ($helpState in $helpStates) {
        $helpLog = Get-CombinedLogText -LogDirectory $helpState.Logs
        if ($helpLog -notmatch 'SMOKE_HELP_COMPLETE nativeZombieAnimated=1 steps=7 back=1 escape=1 urlButton=1 urlEscape=1 browserBounds=1') {
            throw "Help completion evidence missing: $($helpState.Role)"
        }
        if ($helpLog -notmatch 'SMOKE_HELP_PREVIEWS middlePing=1 ctrlStroke=1 sunflowerBlink=1 twoFrames=1') { throw "Help animated previews proof missing" }
        if ($helpLog -notmatch ('SMOKE_HELP_TYPING role=' + $helpState.Role + ' phase=early visible=\d+ total=\d+ ticks=\d+ gain=') -or
            $helpLog -notmatch ('SMOKE_HELP_TYPING role=' + $helpState.Role + ' phase=early[^\r\n]*freshRendererFrame=1 renderedInk=1') -or
            $helpLog -notmatch ('SMOKE_HELP_TYPING role=' + $helpState.Role + ' phase=complete space=1 stoppedTicks=1 visible=\d+ total=\d+ fixedZombieBottomRight=1 ovalSpeechTail=1')) {
            throw "Help native typewriter and fixed zombie proof missing: $($helpState.Role)"
        }
        if ($helpLog -notmatch ('SMOKE_HELP_SCENE_START role=' + $helpState.Role + ' realAdventure1=1 playing=1 helpClosed=1 previewsClosed=1')) {
            throw "Help real level cleanup proof missing: $($helpState.Role)"
        }
        foreach ($helpStep in 1..7) {
            if ($helpLog -notmatch ('SMOKE_HELP_LAYOUT role=' + $helpState.Role + ' step=' + $helpStep + ' fullStringMeasurement=1 bodyFits=1')) {
                throw "Help native text layout proof missing: $($helpState.Role)/$helpStep"
            }
            if ($helpLog -notmatch ('SMOKE_HELP_PIXELS role=' + $helpState.Role + ' step=' + $helpStep + ' ovalOffWhite=1 tailOffWhite=1 seamlessJoin=1')) {
                throw "Help native oval and tail pixel proof missing: $($helpState.Role)/$helpStep"
            }
        }
        if ($helpState.Role -eq "client" -and
            $helpLog -notmatch 'SMOKE_HELP_SCENE_PREVIEW role=client ordinaryZombieLive=1 onlinePreview=1[\s\S]*COOP_HELP_CLOSE synchronousZombieDispose=1 step=3[\s\S]*MakeNewBoard begin') {
            throw "Help synchronous disposal before the new client Board was not observed"
        }
        foreach ($helpFrame in @("help_typing_early", "help_1", "help_2", "help_3", "help_4", "help_5", "help_6", "help_6_animated", "help_7", "help_done", "help_escape", "github_error", "github_closed", "help_scene_started")) {
            if (-not (Test-Path -LiteralPath (Join-Path $helpState.Root ($helpFrame + ".bmp")))) { throw "Help frame missing: $($helpState.Role)/$helpFrame" }
        }
    }
}
catch { $helpFailure = $_.Exception.Message }
finally {
    foreach ($helpState in @($helpStates | Sort-Object @{Expression={if ($_.Role -eq 'client') {0} else {1}}})) {
        if (-not (Stop-OwnedProcess -Process $helpState.Process -Role $helpState.Role)) { $helpFailure += " Owned process cleanup failed." }
    }
    $env:PVZ_COOP_SMOKE_HELP = $helpOld
    [void][IO.Directory]::CreateDirectory($helpRun)
    @{ status = $(if ($helpFailure) { "fail" } else { "pass" }); error = $helpFailure; artifacts = $helpRun; roles = @($helpStates | ForEach-Object { $_.Role }) } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $helpRun "summary.json") -Encoding UTF8
}
Write-Host ("Help smoke artifacts: " + $helpRun)
if ($helpFailure) { throw $helpFailure }
Write-Host "Help and URL workflows Passed at800/1066."
