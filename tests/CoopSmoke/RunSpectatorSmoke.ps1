#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateSet('TowerDefenseClient', 'Adventure1')][string]$Scenario = 'TowerDefenseClient',
    [ValidateRange(60, 600)][int]$TimeoutSeconds = 240
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (-not $GameExe) { $GameExe = Join-Path $repo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = (Resolve-Path -LiteralPath $GameExe).Path
if (-not $ArtifactsRoot) { $ArtifactsRoot = Join-Path $env:TEMP 'PvZ-Coop-Spectator-Smoke' }
$root = Join-Path ([IO.Path]::GetFullPath($ArtifactsRoot)) ('spectator_' + [Guid]::NewGuid().ToString('N'))
$pairRoot = Join-Path $root 'pair'
New-Item -ItemType Directory -Path $pairRoot -Force | Out-Null
$utf8 = New-Object Text.UTF8Encoding($false)

function Quote-Argument([string]$Value) {
    if ($Value.Contains('"')) { throw 'A native argument contains an unsupported quote.' }
    return '"' + $Value + '"'
}
function Start-Child([string]$Executable, [string]$Arguments, [hashtable]$Environment, [string]$WorkingDirectory) {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $Executable; $start.Arguments = $Arguments
    $start.WorkingDirectory = $WorkingDirectory
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $childEnv = $start.Environment
    if ($null -eq $childEnv) { $childEnv = $start.EnvironmentVariables }
    foreach ($key in $Environment.Keys) { $childEnv[$key] = [string]$Environment[$key] }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    if (-not $process.Start()) { throw ('Could not start ' + $Executable) }
    return [pscustomobject]@{ Process = $process; Output = $process.StandardOutput.ReadToEndAsync(); Error = $process.StandardError.ReadToEndAsync() }
}
function Logs([string]$Directory) {
    if (-not (Test-Path -LiteralPath $Directory)) { return '' }
    $text = ''
    foreach ($file in @(Get-ChildItem -LiteralPath $Directory -File -Filter '*.log')) {
        $stream = [IO.File]::Open($file.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        $reader = New-Object IO.StreamReader($stream)
        try { $text += $reader.ReadToEnd() } finally { $reader.Dispose() }
    }
    return $text
}
function Stop-Exact([Diagnostics.Process]$Process) {
    if ($null -ne $Process -and -not $Process.HasExited) {
        [void]$Process.CloseMainWindow()
        if (-not $Process.WaitForExit(2000)) { $Process.Kill(); $Process.WaitForExit() }
    }
}

$pair = $null; $spectator = $null; $run = $null; $passed = $false
try {
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File ' + (Quote-Argument (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1')) +
        ' -GameExe ' + (Quote-Argument $GameExe) + ' -ArtifactsRoot ' + (Quote-Argument $pairRoot) +
        ' -Scenario ' + $Scenario + ' -SkipIntro ' + $(if ($Scenario -eq 'TowerDefenseClient') { '0' } else { '1' }) +
        ' -Background -TimeoutSeconds ' + $TimeoutSeconds
    $pair = Start-Child 'powershell.exe' $arguments @{ PVZ_COOP_SMOKE_SPECTATOR = '1' } $repo
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds + 30)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($pair.Process.HasExited) { throw ('Pair runner ended before spectator launch: ' + $pair.Output.Result + $pair.Error.Result) }
        $runs = @(Get-ChildItem -LiteralPath $pairRoot -Directory -Filter 'run_*')
        if ($runs.Count -gt 1) { throw 'Spectator wrapper found an ambiguous pair run.' }
        if ($runs.Count -eq 1) {
            $run = $runs[0].FullName
            if ((Logs (Join-Path $run 'host\logs')) -match 'SMOKE_SPECTATOR_READY role=host') { break }
        }
        Start-Sleep -Milliseconds 250
    }
    if (-not $run -or -not ((Logs (Join-Path $run 'host\logs')) -match 'SMOKE_SPECTATOR_READY role=host')) {
        throw 'The original two peers did not reach their post-action checkpoint.'
    }
    Write-Host '[SpectatorSmoke] Original pair converged; launching a third hidden game process.'
    $roleRoot = Join-Path $root 'spectator'
    $logRoot = Join-Path $roleRoot 'logs'; $appdata = Join-Path $roleRoot 'appdata'
    New-Item -ItemType Directory -Path $logRoot, $appdata -Force | Out-Null
    $config = Join-Path $roleRoot 'coop.ini'; $manifest = Join-Path $roleRoot 'smoke.ini'; $result = Join-Path $roleRoot 'result.json'
    $configText = [IO.File]::ReadAllText((Join-Path $run 'client\coop.ini'))
    $configLines = @($configText -split '\r?\n' | ForEach-Object { if ($_ -match '^nickname=') { 'nickname=Spectator' } else { $_ } })
    [IO.File]::WriteAllText($config, ($configLines -join "`n"), $utf8)
    $replacements = @{ nickname = 'Spectator'; appdata_dir = $appdata; config_path = $config; log_dir = $logRoot; result_path = $result }
    $manifestLines = @([IO.File]::ReadAllText((Join-Path $run 'client\smoke.ini')) -split '\r?\n' | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$' -and $replacements.ContainsKey($Matches[1])) {
            $Matches[1] + '=' + $replacements[$Matches[1]]
        } else { $_ }
    })
    [IO.File]::WriteAllText($manifest, ($manifestLines -join "`n"), $utf8)
    $runId = (Split-Path -Leaf $run).Substring(4)
    $environment = @{
        PVZ_COOP_SMOKE_RUN_ID = $runId; PVZ_COOP_SMOKE_ROLE = 'client'
        PVZ_COOP_SMOKE_SPECTATOR = '1'; PVZ_COOP_SMOKE_SPECTATOR_ROLE = 'spectator'
        PVZ_COOP_SMOKE_BACKGROUND = '1'; PVZ_COOP_SMOKE_CAPTURE_DIR = $roleRoot
    }
    $spectator = Start-Child $GameExe ('-coop-smoke-manifest=' + (Quote-Argument $manifest)) $environment (Split-Path -Parent $GameExe)
    [IO.File]::WriteAllText((Join-Path $root 'spectator-pid.json'),
        (@{ pid = $spectator.Process.Id; started_utc = $spectator.Process.StartTime.ToUniversalTime().ToString('o'); executable = $GameExe } | ConvertTo-Json), $utf8)
    while ([DateTime]::UtcNow -lt $deadline -and -not $pair.Process.HasExited) {
        if ($spectator.Process.HasExited -and -not (Test-Path -LiteralPath $result)) { throw 'Third game process exited without spectator proof.' }
        if (Test-Path -LiteralPath $result) {
            $liveProof = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
            if ($liveProof.status -eq 'fail') { throw ('Third game process failed: ' + $liveProof.error) }
        }
        $thirdLog = Logs $logRoot
        if ($thirdLog -match 'SMOKE_FAIL|Access Violation|snapshot .*rejected') { throw 'Third game process reported a smoke/runtime failure.' }
        Start-Sleep -Milliseconds 250
    }
    if (-not $pair.Process.HasExited) { throw 'Pair runner timed out after the late join.' }
    [IO.File]::WriteAllText((Join-Path $root 'pair.stdout.log'), $pair.Output.Result, $utf8)
    [IO.File]::WriteAllText((Join-Path $root 'pair.stderr.log'), $pair.Error.Result, $utf8)
    if ($pair.Process.ExitCode -ne 0) { throw ('Base pair verification failed: ' + $pair.Output.Result) }
    if (-not (Test-Path -LiteralPath $result)) { throw 'Expected host-kick proof is missing.' }
    $proof = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
    if ($proof.status -ne 'pass' -or $proof.test -ne 'late-spectator' -or -not $proof.readonly -or -not $proof.ping -or -not $proof.hostKick) {
        throw 'Third-process proof does not satisfy late-join/kick criteria.'
    }
    $hostText = Logs (Join-Path $run 'host\logs'); $clientText = Logs (Join-Path $run 'client\logs'); $thirdText = Logs $logRoot
    $hostShape = [regex]::Match($hostText, 'SMOKE_SPECTATOR_BOOTSTRAP role=host shape=([0-9A-F]{8})')
    $thirdShape = [regex]::Match($thirdText, 'SMOKE_SPECTATOR_BOOTSTRAP role=spectator shape=([0-9A-F]{8})')
    if (-not $hostShape.Success -or -not $thirdShape.Success -or $hostShape.Groups[1].Value -ne $thirdShape.Groups[1].Value) {
        throw 'Late spectator plant/packet identities differ from the existing host.'
    }
    if ($Scenario -eq 'TowerDefenseClient' -and
        ($hostText -notmatch 'SMOKE_SPECTATOR_MIXED_BANK role=host packets=10 host=5 client=5 spectator=0' -or
         $thirdText -notmatch 'SMOKE_SPECTATOR_MIXED_BANK role=spectator packets=10 host=5 client=5 spectator=0')) {
        throw 'Late spectator did not reconstruct the native mixed-owner 10-card bank.'
    }
    foreach ($text in @($hostText, $thirdText)) {
        if ($text -notmatch 'SMOKE_SPECTATOR_READONLY [^\r\n]*noPlantMutation=1 ping=1 input=SDL_queue') { throw 'Missing read-only SDL interaction evidence.' }
    }
    if ($hostText -notmatch 'SMOKE_SPECTATOR_KICK role=host inactive=1 systemChat=1 pauseButton=1' -or
        $clientText -notmatch 'SMOKE_SPECTATOR_KICK role=client inactive=1 systemChat=1 existingSession=1' -or
        $thirdText -notmatch 'SMOKE_SPECTATOR_KICKED role=spectator reason=host_kick readonly=1 ping=1') {
        throw 'Missing pause-button kick, peer system-chat, or actual disconnect-reason evidence.'
    }
    $passed = $true
    Write-Host ('[SpectatorSmoke] PASS: 3 game processes; late join, read-only input, ping, pause kick and system chat. Artifacts: ' + $root)
}
finally {
    if ($null -ne $spectator) { Stop-Exact $spectator.Process }
    if ($null -ne $pair -and -not $pair.Process.HasExited) {
        # Only exact, start-time-verified game PIDs recorded by our owned runner.
        if ($run -and (Test-Path -LiteralPath (Join-Path $run 'pids.json'))) {
            foreach ($entry in @(Get-Content -LiteralPath (Join-Path $run 'pids.json') -Raw | ConvertFrom-Json)) {
                $owned = Get-Process -Id $entry.pid -ErrorAction SilentlyContinue
                if ($owned -and $owned.MainModule.FileName -eq $GameExe -and
                    $owned.StartTime.ToUniversalTime().ToString('o') -eq $entry.started_utc) { Stop-Exact $owned }
            }
        }
        Stop-Exact $pair.Process
    }
    [IO.File]::WriteAllText((Join-Path $root 'summary.json'), (@{ passed = $passed; scope = 'late-spectator-ping-kick'; artifacts = $root } | ConvertTo-Json), $utf8)
}
