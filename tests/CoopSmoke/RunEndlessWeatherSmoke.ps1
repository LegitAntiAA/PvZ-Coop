#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateSet("T", "ST", "Both")][string]$WeatherMode = "Both",
    [ValidateRange(60, 600)][int]$TimeoutSeconds = 180
)
$weatherExeArg = $GameExe
$weatherArtifactsArg = $ArtifactsRoot
$weatherModeArg = $WeatherMode
$weatherTimeout = $TimeoutSeconds
$weatherScriptRoot = Split-Path -Parent $PSCommandPath
. (Join-Path $weatherScriptRoot "RunCoopSmoke.ps1") -FunctionsOnly
$Background = $true
$weatherRepo = Get-NormalizedPath (Join-Path $weatherScriptRoot "..\..")
$weatherExe = if ($weatherExeArg) { Get-NormalizedPath $weatherExeArg } else {
    Join-Path $weatherRepo "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe"
}
if (-not (Test-Path -LiteralPath $weatherExe -PathType Leaf)) { throw "Game executable not found: $weatherExe" }
$weatherArtifacts = if ($weatherArtifactsArg) { Get-NormalizedPath $weatherArtifactsArg } else {
    Join-Path $weatherRepo "work\endless-weather-smoke"
}
$weatherId = [Guid]::NewGuid().ToString("N")
$weatherRoot = Join-Path $weatherArtifacts ("run_" + $weatherId)
[void][IO.Directory]::CreateDirectory($weatherRoot)
$weatherOwned = [Collections.Generic.List[object]]::new()
$weatherCases = [Collections.Generic.List[object]]::new()
$weatherError = ""

function Start-WeatherRole {
    param([string]$Mode, [string]$Role, [int]$Port)
    $root = Join-Path (Join-Path $weatherRoot $Mode) $Role
    $state = @{ Mode=$Mode; Role=$Role; Root=$root; Logs=(Join-Path $root "logs"); Result=(Join-Path $root "result.json"); Process=$null }
    $appData = Join-Path $root "appdata"
    [void][IO.Directory]::CreateDirectory($appData)
    [void][IO.Directory]::CreateDirectory($state.Logs)
    $config = Join-Path $root "coop.ini"
    $manifest = Join-Path $root "smoke.ini"
    $nick = if ($Role -eq "host") { "[Weather H]" } else { "[Weather C]" }
    $width = if ($Role -eq "client") { 1066 } else { 800 }
    Write-CoopConfig -Path $config -Nickname $nick -WindowWidth $width -WindowHeight 600
    # Match the actual requested native mode so the normal deterministic
    # START_GAME guard stays strict for each isolated T/ST workflow.
    $manifestMode = if ($Mode -eq "T") { "endless_t" } else { "endless_st" }
    Write-SmokeManifest -Path $manifest -Role $Role -Nickname $nick -AppDataDirectory $appData `
        -ConfigPath $config -LogDirectory $state.Logs -ResultPath $state.Result -SelectedPort $Port `
        -SelectedMode $manifestMode -SelectedLevel 1 -SelectedSeed 424242 `
        -SelectedTimeoutMs ($weatherTimeout * 1000) -RequiredSnapshots 3 -SelectedSkipIntro 0
    $oldWeather = $env:PVZ_COOP_SMOKE_WEATHER
    $oldMystery = $env:PVZ_COOP_SMOKE_MYSTERY
    $oldDifficulty = $env:PVZ_COOP_SMOKE_DIFFICULTY
    $oldUi = $env:PVZ_COOP_SMOKE_NATIVE_UI
    try {
        $env:PVZ_COOP_SMOKE_WEATHER = $Mode
        $env:PVZ_COOP_SMOKE_MYSTERY = ""
        $env:PVZ_COOP_SMOKE_DIFFICULTY = ""
        $env:PVZ_COOP_SMOKE_NATIVE_UI = ""
        $state.Process = Start-SmokeProcess -Executable $weatherExe -WorkingDirectory (Split-Path -Parent $weatherExe) `
            -ManifestPath $manifest -Role $Role -RunId $weatherId -EnableUnicodeChat $false
        $weatherOwned.Add($state)
        Write-SmokeStatus ("Weather {0}/{1} owned PID {2}: hidden, isolated, renderer only." -f $Mode,$Role,$state.Process.Id)
        return $state
    }
    finally {
        $env:PVZ_COOP_SMOKE_WEATHER = $oldWeather
        $env:PVZ_COOP_SMOKE_MYSTERY = $oldMystery
        $env:PVZ_COOP_SMOKE_DIFFICULTY = $oldDifficulty
        $env:PVZ_COOP_SMOKE_NATIVE_UI = $oldUi
    }
}

function Wait-WeatherRole {
    param([hashtable]$State)
    $deadline = [DateTime]::UtcNow.AddSeconds($weatherTimeout)
    while ([DateTime]::UtcNow -lt $deadline) {
        foreach ($peer in $weatherOwned) {
            if ($peer.Mode -ne $State.Mode) { continue }
            $peerLog = Get-CombinedLogText $peer.Logs
            if ($peerLog -match 'SMOKE_FAIL|Access Violation|unhandled exception|assertion failed|snapshot[^\r\n]*rejected') {
                throw "Weather peer runtime failed: $($peer.Logs)"
            }
            $peerResult = Read-JsonIfComplete $peer.Result
            if ($null -ne $peerResult -and $peerResult.status -ne "pass") {
                throw "Weather peer reported failure: $($peer.Result)"
            }
        }
        $log = Get-CombinedLogText $State.Logs
        if ($log -match 'SMOKE_FAIL|Access Violation|unhandled exception|assertion failed|snapshot[^\r\n]*rejected') {
            throw "Weather runtime failed: $($State.Logs)"
        }
        $result = Read-JsonIfComplete $State.Result
        if ($null -ne $result) {
            if ($result.status -ne "pass" -or $result.role -ne $State.Role -or $result.protocol -lt 33) { throw "Invalid weather result: $($State.Result)" }
            return $result
        }
        if ($State.Process.HasExited) { throw "Weather process exited before result: $($State.Role)" }
        Start-Sleep -Milliseconds 200
    }
    throw "Weather runtime timed out: $($State.Logs)"
}

try {
    Write-SmokeStatus ("Weather artifacts: {0}" -f $weatherRoot)
    $modes = if ($weatherModeArg -eq "Both") { @("T","ST") } else { @($weatherModeArg) }
    foreach ($mode in $modes) {
        $port = Get-FreeLoopbackPort
        $hostRole = Start-WeatherRole $mode "host" $port
        Wait-ForLogPattern -LogDirectory $hostRole.Logs -Pattern '(?im)host listening port=' `
            -Process $hostRole.Process -Role "host" -TimeoutSeconds $weatherTimeout
        $clientRole = Start-WeatherRole $mode "client" $port
        $hr = Wait-WeatherRole $hostRole
        $cr = Wait-WeatherRole $clientRole
        if ($hr.protocol -ne $cr.protocol) { throw "Weather peer protocol mismatch." }
        foreach ($state in @($hostRole,$clientRole)) {
            $log = Get-CombinedLogText $state.Logs
            if ($log -notmatch ("SMOKE_WEATHER_MODE role="+$state.Role+" actual="+$mode+" sdlChooser=1 quota=5 native=1") -or
                $log -notmatch ("SMOKE_WEATHER_COMPLETE role="+$state.Role+" actual="+$mode+" checkpoints=11 menuGain=1")) {
                throw "Missing actual native weather mode or menu-gain completion: $mode/$($state.Role)"
            }
            foreach ($step in @("day","mid","night","reverse_mid","day_return","fog","plantern","blover","fog_return","storm_dark","storm_flash")) {
                if ($log -notmatch ("SMOKE_WEATHER_CHECK role="+$state.Role+" step="+$step+"\b[^\r\n]*capture=1 native=1") -or
                    -not (Test-Path -LiteralPath (Join-Path $state.Root ("weather_"+$step+".bmp")) -PathType Leaf)) {
                    throw "Missing authoritative weather renderer checkpoint: $mode/$($state.Role)/$step"
                }
            }
            if ($log -notmatch 'SMOKE_WEATHER_AUDIO[^\r\n]*backend=1 userVolumeUnchanged=1' -or
                $log -notmatch 'SMOKE_WEATHER_RAIN_RENDER[^\r\n]*native=1') { throw "Missing actual music/rain native evidence: $mode/$($state.Role)" }
            foreach ($paused in @(0,1)) {
                if ($log -notmatch ('SMOKE_WEATHER_AUDIO[^\r\n]*backend=1 userVolumeUnchanged=1 paused='+$paused+'\b')) {
                    throw "Missing native playing/paused audio-channel evidence: $mode/$($state.Role)/$paused"
                }
            }
            if ($log -notmatch 'SMOKE_WEATHER_CHECK[^\r\n]*step=storm_flash\b[^\r\n]*probe=median9') {
                throw "Missing native flash-window/multipoint field evidence: $mode/$($state.Role)"
            }
            foreach ($maskStep in @("plantern","fog_return")) {
                if ($log -notmatch ('SMOKE_WEATHER_CHECK[^\r\n]*step='+$maskStep+'\b[^\r\n]*lawnArea=3x3 clearFogSamples=12 outsideFog=10\b')) {
                    throw "Missing exact native 3x3 Plantern/exterior fog proof: $mode/$($state.Role)/$maskStep"
                }
            }
            $thunder = @([regex]::Matches($log,'WEATHER_THUNDER[^\r\n]*local=1'))
            if ($thunder.Count -ne 1) { throw "Missing or duplicated native weather thunder: $mode/$($state.Role)" }
        }
        $hostLog = Get-CombinedLogText $hostRole.Logs
        $clientLog = Get-CombinedLogText $clientRole.Logs
        foreach ($direction in @("day","night")) {
            $samples = @([regex]::Matches($hostLog,("SMOKE_WEATHER_BLEND direction="+$direction+" blend=(\d+)")))
            if ($samples.Count -lt 5) { throw "Too few actual blend/gain samples: $mode/$direction" }
            for ($i=1;$i -lt $samples.Count;$i++) {
                $previous = [int]$samples[$i-1].Groups[1].Value; $current = [int]$samples[$i].Groups[1].Value
                if (($direction -eq "night" -and $current -lt $previous) -or ($direction -eq "day" -and $current -gt $previous)) {
                    throw "Weather blend was not monotonic: $mode/$direction"
                }
            }
        }
        $tx = @(Get-SnapshotHashes -Text $hostLog -Marker "SMOKE_SNAPSHOT_TX")
        $rx = @(Get-SnapshotHashes -Text $clientLog -Marker "SMOKE_SNAPSHOT_APPLIED")
        $common = @($rx | Where-Object { $tx -contains $_ })
        if ($common.Count -lt 10) { throw "Weather peers lack sufficient matching full authoritative snapshots." }
        foreach ($state in @($clientRole,$hostRole)) {
            if (-not (Stop-OwnedProcess -Process $state.Process -Role ("weather-"+$state.Role))) { throw "Owned weather process could not be stopped." }
        }
        $weatherCases.Add(@{ mode=$mode; status="pass"; checkpoints=11; common_hashes=$common.Count; scope="controlled boundaries/native SDL, rendering, fog grid, channel gain; no natural60s/audio claim" })
    }
}
catch { $weatherError = $_.Exception.Message; Write-Host ("[Weather] FAIL: "+$weatherError) -ForegroundColor Red }
finally {
    foreach ($state in $weatherOwned) {
        if (-not (Stop-OwnedProcess -Process $state.Process -Role "weather-owned")) { $weatherError="An owned weather process could not be stopped." }
    }
    Write-Utf8NoBom -Path (Join-Path $weatherRoot "summary.json") -Text (@{ status=$(if($weatherError){"fail"}else{"pass"}); cases=@($weatherCases.ToArray()); error=$weatherError; artifacts=$weatherRoot } | ConvertTo-Json -Depth 6)
}
if ($weatherError) { exit 1 }
Write-SmokeStatus ("PASS requested native weather fixtures: {0}" -f $weatherRoot)
