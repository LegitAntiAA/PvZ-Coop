#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateRange(120, 600)][int]$TimeoutSeconds = 240
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (-not $GameExe) { $GameExe = Join-Path $repo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = (Resolve-Path -LiteralPath $GameExe).Path
if (-not $ArtifactsRoot) { $ArtifactsRoot = Join-Path $env:TEMP 'PvZ-Coop-Zombatar-Smoke' }
$root = Join-Path ([IO.Path]::GetFullPath($ArtifactsRoot)) ('avatar_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
$utf8 = New-Object Text.UTF8Encoding($false)

function Quote-Argument([string]$Value) {
    if ($Value.Contains('"')) { throw 'A native argument contains an unsupported quote.' }
    return '"' + $Value + '"'
}
function Start-Owned([string]$Executable, [string]$Arguments, [hashtable]$Environment, [string]$Directory) {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $Executable; $start.Arguments = $Arguments; $start.WorkingDirectory = $Directory
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true; $start.WindowStyle = 'Hidden'
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $variables = $start.Environment
    if ($null -eq $variables) { $variables = $start.EnvironmentVariables }
    foreach ($key in $Environment.Keys) { $variables[$key] = [string]$Environment[$key] }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    if (-not $process.Start()) { throw ('Could not launch ' + $Executable) }
    return [pscustomobject]@{ Process = $process; Output = $process.StandardOutput.ReadToEndAsync(); Error = $process.StandardError.ReadToEndAsync() }
}
function Stop-Owned([Diagnostics.Process]$Process) {
    if ($null -eq $Process) { return }
    try {
        if ($Process.HasExited) { return }
        [void]$Process.CloseMainWindow()
        if (-not $Process.WaitForExit(2000)) { if (-not $Process.HasExited) { $Process.Kill() }; $Process.WaitForExit() }
    }
    catch { if (-not $Process.HasExited) { throw } }
}
function Logs([string]$Directory) {
    $text = ''
    foreach ($file in @(Get-ChildItem -LiteralPath $Directory -File -Filter '*.log')) {
        $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite')
        $reader = New-Object IO.StreamReader($stream)
        try { $text += $reader.ReadToEnd() } finally { $reader.Dispose() }
    }
    return $text
}

$editor = $null; $pair = $null; $pairRun = $null; $passed = $false
$saved = @{}
try {
    # Each editor process owns a fresh personal INI. The restarted pair copies
    # these complete files; the wrapper never constructs or injects an avatar.
    foreach ($role in @('host', 'client')) {
        $directory = Join-Path $root ('editor_' + $role)
        $appdata = Join-Path $directory 'appdata'; $logs = Join-Path $directory 'logs'
        New-Item -ItemType Directory -Path $appdata, $logs -Force | Out-Null
        $config = Join-Path $directory 'coop.ini'; $manifest = Join-Path $directory 'smoke.ini'; $result = Join-Path $directory 'result.json'
        $nickname = if ($role -eq 'host') { 'SmokeHost' } else { 'SmokeClient' }
        [IO.File]::WriteAllText($config, (@(
            ('nickname=' + $nickname), 'enable_coop=1', 'unlock_all_minigames=1', 'base_snapshot_rate=15',
            'window_width=800', 'window_height=600', 'borderless_fullscreen=0'
        ) -join "`n"), $utf8)
        [IO.File]::WriteAllText($manifest, (@(
            'schema=1', ('role=' + $role), 'endpoint=127.0.0.1', 'port=49152', ('nickname=' + $nickname),
            'mode=adventure', 'level=1', 'seed=424242', ('appdata_dir=' + $appdata), ('config_path=' + $config),
            ('log_dir=' + $logs), ('result_path=' + $result), ('timeout_ms=' + ($TimeoutSeconds * 1000)),
            'minimum_snapshots=3', 'skip_intro=0', 'close_on_result=0'
        ) -join "`n"), $utf8)
        $environment = @{
            PVZ_COOP_SMOKE_RUN_ID = [Guid]::NewGuid().ToString('N'); PVZ_COOP_SMOKE_ROLE = $role
            PVZ_COOP_SMOKE_AVATAR_EDITOR = '1'; PVZ_COOP_SMOKE_SAVED_AVATARS = '0'
            PVZ_COOP_SMOKE_BACKGROUND = '1'; PVZ_COOP_SMOKE_CAPTURE_DIR = $directory
            PVZ_COOP_SMOKE_NATIVE_UI = ''; PVZ_COOP_SMOKE_ONLINE_ENDPOINT = ''
            PVZ_COOP_SMOKE_SPECTATOR = '0'; PVZ_COOP_SMOKE_PRESENTATION = '0'
        }
        Write-Host ('[ZombatarSmoke] Editing and saving the personal ' + $role + ' appearance in a hidden game process.')
        $editor = Start-Owned $GameExe ('-coop-smoke-manifest=' + (Quote-Argument $manifest)) $environment (Split-Path -Parent $GameExe)
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (-not (Test-Path -LiteralPath $result) -and [DateTime]::UtcNow -lt $deadline) {
            if ($editor.Process.HasExited) { throw ('Editor exited without proof for ' + $role) }
            Start-Sleep -Milliseconds 200
        }
        if (-not (Test-Path -LiteralPath $result)) { throw ('Editor workflow timed out for ' + $role) }
        $proof = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
        if ($proof.status -ne 'pass' -or $proof.test -ne 'avatar-editor' -or $proof.input -ne 'SDL_queue' -or
            $proof.bytes -ne 13 -or $proof.appearance -notmatch '^[A-F0-9]{26}$') { throw ('Editor proof failed for ' + $role) }
        if (-not $editor.Process.WaitForExit(10000)) { throw ('Editor did not close after saving ' + $role) }
        $ini = [IO.File]::ReadAllText($config)
        if ($ini -notmatch ('(?m)^zombatar=' + [regex]::Escape($proof.appearance) + '\r?$')) { throw 'Editor proof differs from its actual personal INI.' }
        foreach ($name in @('avatar_editor_fields.bmp', 'avatar_editor_reopened.bmp')) {
            if (-not (Test-Path -LiteralPath (Join-Path $directory $name) -PathType Leaf)) { throw ('Missing editor renderer capture: ' + $name) }
        }
        if ((Logs $logs) -notmatch 'SMOKE_AVATAR_EDITOR[^\r\n]*saved=1 reset=1 bytes=13') { throw 'Native editor save/Reset evidence is missing.' }
        $saved[$role] = [pscustomobject]@{ Config = $config; Appearance = [string]$proof.appearance; SHA256 = (Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash }
        $editor = $null
    }
    if ($saved.host.Appearance -eq $saved.client.Appearance) { throw 'The two personal editor appearances are not distinct.' }
    $pairRoot = Join-Path $root 'pair'
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File ' + (Quote-Argument (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1')) +
        ' -GameExe ' + (Quote-Argument $GameExe) + ' -ArtifactsRoot ' + (Quote-Argument $pairRoot) +
        ' -Background -Scenario Adventure1 -SkipIntro 0 -VerifySavedAvatars -TimeoutSeconds ' + $TimeoutSeconds +
        ' -HostConfigSource ' + (Quote-Argument $saved.host.Config) + ' -ClientConfigSource ' + (Quote-Argument $saved.client.Config)
    Write-Host '[ZombatarSmoke] Restarting both saved personal configurations, then joining and observing the natural appearance union.'
    $pair = Start-Owned 'powershell.exe' $arguments @{ PVZ_COOP_SMOKE_AVATAR_EDITOR = '0'; PVZ_COOP_SMOKE_SAVED_AVATARS = '1' } $repo
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds + 60)
    while (-not $pair.Process.HasExited -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 250 }
    if (-not $pair.Process.HasExited) { throw 'Restarted avatar pair timed out.' }
    [IO.File]::WriteAllText((Join-Path $root 'pair.stdout.log'), $pair.Output.Result, $utf8)
    [IO.File]::WriteAllText((Join-Path $root 'pair.stderr.log'), $pair.Error.Result, $utf8)
    if ($pair.Process.ExitCode -ne 0) { throw ('Restarted avatar pair failed: ' + $pair.Output.Result) }
    $runs = @(Get-ChildItem -LiteralPath $pairRoot -Directory -Filter 'run_*')
    if ($runs.Count -ne 1) { throw 'The saved-avatar pair artifact directory is ambiguous.' }
    $pairRun = $runs[0].FullName
    foreach ($role in @('host', 'client')) {
        $roleRoot = Join-Path $pairRun $role
        $before = Get-Content -LiteralPath (Join-Path $roleRoot 'avatar_before_join.json') -Raw | ConvertFrom-Json
        if ($before.status -ne 'pass' -or $before.active -ne $false -or $before.bytes -ne 13 -or
            $before.appearance -ne $saved[$role].Appearance) { throw ('Restart did not load the personal ' + $role + ' avatar before joining.') }
        if ((Get-FileHash -LiteralPath (Join-Path $roleRoot 'coop.ini') -Algorithm SHA256).Hash -ne $saved[$role].SHA256) {
            throw 'The restarted pair modified a copied personal INI.'
        }
        if (-not (Test-Path -LiteralPath (Join-Path $roleRoot 'avatar_saved_union.bmp'))) { throw 'Missing natural saved-avatar union render capture.' }
    }
    $passed = $true
    Write-Host ('[ZombatarSmoke] PASS: SDL editor save/Reset, fresh-process load before join, and both natural personal appearances. Artifacts: ' + $root)
}
finally {
    if ($null -ne $editor) { Stop-Owned $editor.Process }
    if ($null -ne $pair -and -not $pair.Process.HasExited) {
        $runs = @(Get-ChildItem -LiteralPath (Join-Path $root 'pair') -Directory -Filter 'run_*' -ErrorAction SilentlyContinue)
        if ($runs.Count -eq 1) {
            $ledger = Join-Path $runs[0].FullName 'pids.json'
            if (Test-Path -LiteralPath $ledger) {
                $records = Get-Content -LiteralPath $ledger -Raw | ConvertFrom-Json
                foreach ($entry in $records) {
                    $owned = Get-Process -Id $entry.pid -ErrorAction SilentlyContinue
                    if (-not $owned -or $owned.HasExited) { continue }
                    try {
                        $module = $owned.MainModule; $when = [DateTimeOffset]$entry.started_utc
                        if ($module -and $module.FileName -eq $GameExe -and $owned.StartTime.ToUniversalTime().Ticks -eq $when.UtcDateTime.Ticks) { Stop-Owned $owned }
                    }
                    catch { if (-not $owned.HasExited) { throw } }
                }
            }
        }
        Stop-Owned $pair.Process
    }
    [IO.File]::WriteAllText((Join-Path $root 'summary.json'), (@{ passed = $passed; scope = 'avatar-editor-save-restart-join-natural-union'; artifacts = $root } | ConvertTo-Json), $utf8)
}
