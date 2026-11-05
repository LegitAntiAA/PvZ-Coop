#requires -Version 5.1

[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateRange(10, 3600)]
    [int]$TimeoutSeconds = 120,
    [ValidateRange(5, 300)]
    # A clean isolated appdata directory forces the original game to extract
    # its sound cache before LoadingCompleted/NetInit can open the listener.
    [int]$HostReadyTimeoutSeconds = 120,
    [ValidateRange(1, 60)]
    [int]$GracefulExitSeconds = 10,
    [ValidateRange(0, 65535)]
    [int]$Port = 0,
    [ValidateSet("Adventure1", "Adventure5Bowling", "Adventure5BowlingClient", "Adventure8Soak", "Adventure8Victory", "Adventure15WhackClient", "SlotMachineClient", "Vasebreaker", "Beghouled")]
    [string]$Scenario = "Adventure1",
    [ValidateSet(0, 1)]
    [int]$SkipIntro = 1,
    [ValidateRange(800, 3840)]
    [int]$HostWindowWidth = 800,
    [ValidateRange(600, 2160)]
    [int]$HostWindowHeight = 600,
    [ValidateRange(800, 3840)]
    [int]$ClientWindowWidth = 800,
    [ValidateRange(600, 2160)]
    [int]$ClientWindowHeight = 600,
    [uint32]$Seed = 424242,
    [ValidateRange(1, 1000)]
    [int]$MinimumSnapshots = 3,
    [switch]$CleanupOnSuccess
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-SmokeStatus {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host ("[CoopSmoke] {0}" -f $Message)
}

if (-not ("CoopSmokeNativeWindow" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class CoopSmokeNativeWindow
{
    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern IntPtr GetWindow(IntPtr hwnd, uint command);
    [DllImport("user32.dll")]
    public static extern bool ShowWindowAsync(IntPtr hwnd, int command);
    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hwnd, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);

    public static IntPtr FindVisibleTopLevelWindow(int processId)
    {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr ignored)
        {
            uint ownerProcessId;
            GetWindowThreadProcessId(hwnd, out ownerProcessId);
            if (ownerProcessId == (uint)processId && IsWindowVisible(hwnd) &&
                GetWindow(hwnd, 4) == IntPtr.Zero)
            {
                result = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
"@
}

function Set-SmokeWindowLayout {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$Role,
        [Parameter(Mandatory = $true)][int]$X,
        [Parameter(Mandatory = $true)][int]$Y,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height
    )

    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 200; $attempt++) {
        if ($Process.HasExited) {
            throw ("The {0} process exited before its visible window appeared." -f $Role)
        }
        $window = [CoopSmokeNativeWindow]::FindVisibleTopLevelWindow($Process.Id)
        if ($window -ne [IntPtr]::Zero) {
            break
        }
        Start-Sleep -Milliseconds 25
    }
    if ($window -eq [IntPtr]::Zero) {
        throw ("The {0} process did not create a visible top-level window." -f $Role)
    }

    [void][CoopSmokeNativeWindow]::ShowWindowAsync($window, 9) # SW_RESTORE
    $shown = [CoopSmokeNativeWindow]::SetWindowPos(
        $window, [IntPtr]::Zero, $X, $Y, $Width, $Height, 0x0040) # SWP_SHOWWINDOW
    if (-not $shown) {
        throw ("Could not place the {0} window side-by-side." -f $Role)
    }
    Write-SmokeStatus ("Visible {0} window placed at ({1},{2}) size={3}x{4} HWND=0x{5}." -f
        $Role, $X, $Y, $Width, $Height, $window.ToInt64().ToString("X"))
}

function Get-NormalizedPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return [System.IO.Path]::GetFullPath($Path)
}

function Ensure-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)
    [void][System.IO.Directory]::CreateDirectory($Path)
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text
    )

    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Assert-IniValue {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value
    )

    if ($Value.IndexOfAny([char[]]@("`r", "`n")) -ge 0) {
        throw ("Manifest value '{0}' contains a newline." -f $Name)
    }
}

function Get-FreeLoopbackPort {
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
    try {
        $listener.Start()
        return ([System.Net.IPEndPoint]$listener.LocalEndpoint).Port
    }
    finally {
        $listener.Stop()
    }
}

function Write-CoopConfig {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Nickname,
        [Parameter(Mandatory = $true)][int]$WindowWidth,
        [Parameter(Mandatory = $true)][int]$WindowHeight
    )

    Assert-IniValue -Name "nickname" -Value $Nickname
    $content = @"
# Generated by tests/CoopSmoke/RunCoopSmoke.ps1
nickname=$Nickname
unlock_all_minigames=1
enable_coop=1
base_snapshot_rate=15
window_width=$WindowWidth
window_height=$WindowHeight
borderless_fullscreen=0
"@
    Write-Utf8NoBom -Path $Path -Text ($content + [Environment]::NewLine)
}

function Write-SmokeManifest {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][ValidateSet("host", "client")][string]$Role,
        [Parameter(Mandatory = $true)][string]$Nickname,
        [Parameter(Mandatory = $true)][string]$AppDataDirectory,
        [Parameter(Mandatory = $true)][string]$ConfigPath,
        [Parameter(Mandatory = $true)][string]$LogDirectory,
        [Parameter(Mandatory = $true)][string]$ResultPath,
        [Parameter(Mandatory = $true)][int]$SelectedPort,
        [Parameter(Mandatory = $true)][string]$SelectedMode,
        [Parameter(Mandatory = $true)][int]$SelectedLevel,
        [Parameter(Mandatory = $true)][uint32]$SelectedSeed,
        [Parameter(Mandatory = $true)][int]$SelectedTimeoutMs,
        [Parameter(Mandatory = $true)][int]$RequiredSnapshots,
        [Parameter(Mandatory = $true)][ValidateSet(0, 1)][int]$SelectedSkipIntro
    )

    $values = [ordered]@{
        role = $Role
        nickname = $Nickname
        appdata_dir = $AppDataDirectory
        config_path = $ConfigPath
        log_dir = $LogDirectory
        result_path = $ResultPath
        mode = $SelectedMode
    }
    foreach ($entry in $values.GetEnumerator()) {
        Assert-IniValue -Name ([string]$entry.Key) -Value ([string]$entry.Value)
    }

    $content = @"
# Generated by tests/CoopSmoke/RunCoopSmoke.ps1
schema=1
role=$Role
endpoint=127.0.0.1
port=$SelectedPort
nickname=$Nickname
mode=$SelectedMode
level=$SelectedLevel
seed=$SelectedSeed
appdata_dir=$AppDataDirectory
config_path=$ConfigPath
log_dir=$LogDirectory
result_path=$ResultPath
timeout_ms=$SelectedTimeoutMs
minimum_snapshots=$RequiredSnapshots
skip_intro=$SelectedSkipIntro
close_on_result=0
"@
    Write-Utf8NoBom -Path $Path -Text ($content + [Environment]::NewLine)
}

function Start-SmokeProcess {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$Role,
        [Parameter(Mandatory = $true)][string]$RunId
    )

    if ($ManifestPath.Contains('"')) {
        throw "Manifest paths containing a quote are not supported."
    }

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.Arguments = '-coop-smoke-manifest="' + $ManifestPath + '"'
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $false
    # Newer Windows PowerShell/.NET combinations can expose the legacy
    # EnvironmentVariables property as null while the IDictionary Environment
    # view remains valid.  Use the latter so the role guard is always inherited.
    $childEnvironment = $startInfo.Environment
    if ($null -eq $childEnvironment) {
        $childEnvironment = $startInfo.EnvironmentVariables
    }
    if ($null -eq $childEnvironment) {
        throw "ProcessStartInfo did not expose a writable child environment."
    }
    $childEnvironment["PVZ_COOP_SMOKE_RUN_ID"] = $RunId
    $childEnvironment["PVZ_COOP_SMOKE_ROLE"] = $Role

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw ("Failed to start the {0} process." -f $Role)
    }

    return $process
}

function Save-PidLedger {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Executable,
        [AllowNull()][System.Diagnostics.Process]$HostProcess,
        [AllowNull()][System.Diagnostics.Process]$ClientProcess
    )

    $entries = @()
    if ($null -ne $HostProcess) {
        $entries += [ordered]@{
            role = "host"
            pid = $HostProcess.Id
            started_utc = $HostProcess.StartTime.ToUniversalTime().ToString("o")
            executable = $Executable
        }
    }
    if ($null -ne $ClientProcess) {
        $entries += [ordered]@{
            role = "client"
            pid = $ClientProcess.Id
            started_utc = $ClientProcess.StartTime.ToUniversalTime().ToString("o")
            executable = $Executable
        }
    }

    Write-Utf8NoBom -Path $Path -Text ((ConvertTo-Json -InputObject @($entries) -Depth 4) + [Environment]::NewLine)
}

function Read-SharedTextFile {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = $null
    $reader = $null
    try {
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite
        )
        $reader = [System.IO.StreamReader]::new($stream)
        return $reader.ReadToEnd()
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        elseif ($null -ne $stream) {
            $stream.Dispose()
        }
    }
}

function Get-CombinedLogText {
    param([Parameter(Mandatory = $true)][string]$LogDirectory)

    if (-not (Test-Path -LiteralPath $LogDirectory -PathType Container)) {
        return ""
    }

    $builder = New-Object System.Text.StringBuilder
    $files = @(Get-ChildItem -LiteralPath $LogDirectory -Filter "*.log" -File -ErrorAction SilentlyContinue |
        Sort-Object -Property LastWriteTimeUtc, Name)
    foreach ($file in $files) {
        try {
            [void]$builder.AppendLine(("--- {0} ---" -f $file.Name))
            [void]$builder.AppendLine((Read-SharedTextFile -Path $file.FullName))
        }
        catch [System.IO.IOException] {
            # The writer can be between a truncate and reopen. The next poll retries it.
        }
    }

    return $builder.ToString()
}

function Wait-ForLogPattern {
    param(
        [Parameter(Mandatory = $true)][string]$LogDirectory,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$Role,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $text = Get-CombinedLogText -LogDirectory $LogDirectory
        if ([regex]::IsMatch($text, $Pattern)) {
            return
        }
        if ($Process.HasExited) {
            throw ("The {0} process exited with code {1} before log readiness." -f $Role, $Process.ExitCode)
        }
        Start-Sleep -Milliseconds 200
    }

    throw ("Timed out waiting for the {0} log pattern: {1}" -f $Role, $Pattern)
}

function Read-JsonIfComplete {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    try {
        $text = Read-SharedTextFile -Path $Path
        if ([string]::IsNullOrWhiteSpace($text)) {
            return $null
        }
        return ($text | ConvertFrom-Json -ErrorAction Stop)
    }
    catch {
        # Result files are written atomically by the hook, but tolerate a partial
        # file so a failed rename cannot make the runner crash during polling.
        return $null
    }
}

function Assert-NoLiveFailure {
    param(
        [Parameter(Mandatory = $true)][string]$HostLogDirectory,
        [Parameter(Mandatory = $true)][string]$ClientLogDirectory
    )

    $fatalPattern = '(?im)Access Violation|\bSMOKE_FAIL\b|\bfatal:|version mismatch|' +
        'protocol violation|queue overflow|unhandled exception|assertion failed'
    foreach ($entry in @(
        @{ Role = "host"; Directory = $HostLogDirectory },
        @{ Role = "client"; Directory = $ClientLogDirectory }
    )) {
        $text = Get-CombinedLogText -LogDirectory ([string]$entry.Directory)
        $match = [regex]::Match($text, $fatalPattern)
        if ($match.Success) {
            throw ("Fail-fast: {0} log contains '{1}'." -f $entry.Role, $match.Value)
        }
    }
}

function Wait-ForResults {
    param(
        [Parameter(Mandatory = $true)][string]$HostResultPath,
        [Parameter(Mandatory = $true)][string]$ClientResultPath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$HostProcess,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$ClientProcess,
        [Parameter(Mandatory = $true)][string]$HostLogDirectory,
        [Parameter(Mandatory = $true)][string]$ClientLogDirectory,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $hostResult = Read-JsonIfComplete -Path $HostResultPath
        $clientResult = Read-JsonIfComplete -Path $ClientResultPath
        Assert-NoLiveFailure -HostLogDirectory $HostLogDirectory `
            -ClientLogDirectory $ClientLogDirectory
        if (($null -ne $hostResult) -and ([string]$hostResult.status -eq "fail")) {
            throw ("Host smoke hook failed: {0}" -f [string]$hostResult.error)
        }
        if (($null -ne $clientResult) -and ([string]$clientResult.status -eq "fail")) {
            throw ("Client smoke hook failed: {0}" -f [string]$clientResult.error)
        }
        if (($null -ne $hostResult) -and ($null -ne $clientResult)) {
            return @($hostResult, $clientResult)
        }

        if ($HostProcess.HasExited -and ($null -eq $hostResult)) {
            throw ("Host exited with code {0} without a complete result file." -f $HostProcess.ExitCode)
        }
        if ($ClientProcess.HasExited -and ($null -eq $clientResult)) {
            throw ("Client exited with code {0} without a complete result file." -f $ClientProcess.ExitCode)
        }

        Start-Sleep -Milliseconds 200
    }

    throw ("Timed out after {0}s waiting for host and client result files." -f $TimeoutSeconds)
}

function Get-RequiredProperty {
    param(
        [Parameter(Mandatory = $true)][object]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Role
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw ("The {0} result is missing property '{1}'." -f $Role, $Name)
    }
    return $property.Value
}

function Assert-SmokeResult {
    param(
        [Parameter(Mandatory = $true)][object]$Result,
        [Parameter(Mandatory = $true)][ValidateSet("host", "client")][string]$Role,
        [Parameter(Mandatory = $true)][string]$ExpectedMode,
        [Parameter(Mandatory = $true)][string]$ExpectedScenario,
        [Parameter(Mandatory = $true)][int]$ExpectedLevel,
        [Parameter(Mandatory = $true)][uint32]$ExpectedSeed,
        [Parameter(Mandatory = $true)][int]$RequiredSnapshots,
        [Parameter(Mandatory = $true)][ValidateSet(0, 1)][int]$ExpectedSkipIntro
    )

    $status = [string](Get-RequiredProperty -Object $Result -Name "status" -Role $Role)
    $actualRole = [string](Get-RequiredProperty -Object $Result -Name "role" -Role $Role)
    $actualMode = [string](Get-RequiredProperty -Object $Result -Name "mode" -Role $Role)
    $actualScenario = [string](Get-RequiredProperty -Object $Result -Name "scenario" -Role $Role)
    $actualLevel = [int](Get-RequiredProperty -Object $Result -Name "level" -Role $Role)
    $actualSeed = [uint32](Get-RequiredProperty -Object $Result -Name "seed" -Role $Role)
    $protocol = [int](Get-RequiredProperty -Object $Result -Name "protocol" -Role $Role)

    if ($status -ne "pass") {
        throw ("The {0} result status is '{1}', expected 'pass'." -f $Role, $status)
    }
    if ($actualRole -ne $Role) {
        throw ("The {0} result reports role '{1}'." -f $Role, $actualRole)
    }
    if (-not $actualMode.Equals($ExpectedMode, [StringComparison]::OrdinalIgnoreCase)) {
        throw ("The {0} result mode is '{1}', expected '{2}'." -f $Role, $actualMode, $ExpectedMode)
    }
    if ($actualScenario -ne $ExpectedScenario) {
        throw ("The {0} result scenario is '{1}', expected '{2}'." -f
            $Role, $actualScenario, $ExpectedScenario)
    }
    if (($actualLevel -ne $ExpectedLevel) -or ($actualSeed -ne $ExpectedSeed)) {
        throw ("The {0} result has level/seed {1}/{2}, expected {3}/{4}." -f
            $Role, $actualLevel, $actualSeed, $ExpectedLevel, $ExpectedSeed)
    }
    if ($protocol -le 0) {
        throw ("The {0} result contains invalid protocol {1}." -f $Role, $protocol)
    }
    $actualSkipIntro = [bool](Get-RequiredProperty -Object $Result -Name "skipIntro" -Role $Role)
    if ($actualSkipIntro -ne ([bool]$ExpectedSkipIntro)) {
        throw ("The {0} result reports skipIntro={1}, expected {2}." -f
            $Role, $actualSkipIntro, [bool]$ExpectedSkipIntro)
    }
    $actionApplied = [bool](Get-RequiredProperty -Object $Result -Name "actionApplied" -Role $Role)
    if (-not $actionApplied) {
        throw ("The {0} result did not observe the scripted action." -f $Role)
    }
    $postActionSnapshots = [int](Get-RequiredProperty -Object $Result -Name "postActionSnapshots" -Role $Role)
    if ($postActionSnapshots -lt $RequiredSnapshots) {
        throw ("The {0} result reports only {1} post-action snapshots; {2} are required." -f
            $Role, $postActionSnapshots, $RequiredSnapshots)
    }

    $snapshotProperty = if ($Role -eq "host") { "snapshotsSent" } else { "snapshotsApplied" }
    $snapshotCount = [int](Get-RequiredProperty -Object $Result -Name $snapshotProperty -Role $Role)
    if ($snapshotCount -lt $RequiredSnapshots) {
        throw ("The {0} result reports {1}={2}; at least {3} are required." -f
            $Role, $snapshotProperty, $snapshotCount, $RequiredSnapshots)
    }

    return $protocol
}

function Assert-PositivePatterns {
    param(
        [Parameter(Mandatory = $true)][string]$Role,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory = $true)][object[]]$Patterns
    )

    $missing = [System.Collections.Generic.List[string]]::new()
    foreach ($specification in $Patterns) {
        if (-not [regex]::IsMatch($Text, [string]$specification.Pattern)) {
            [void]$missing.Add([string]$specification.Label)
        }
    }
    if ($missing.Count -gt 0) {
        throw ("The {0} log is missing required signatures: {1}" -f $Role, ($missing -join ", "))
    }
}

function Assert-NoNegativePatterns {
    param(
        [Parameter(Mandatory = $true)][string]$Role,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory = $true)][object[]]$Patterns
    )

    foreach ($specification in $Patterns) {
        $match = [regex]::Match($Text, [string]$specification.Pattern)
        if ($match.Success) {
            throw ("The {0} log contains forbidden signature '{1}': {2}" -f
                $Role, $specification.Label, $match.Value)
        }
    }
}

function Get-SnapshotHashes {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory = $true)][string]$Marker
    )

    $pattern = '(?im)\b' + [regex]::Escape($Marker) + '\b[^\r\n]*?\bhash=(?:0x)?([0-9a-f]{8,16})\b'
    $hashes = @()
    foreach ($match in [regex]::Matches($Text, $pattern)) {
        $hashes += $match.Groups[1].Value.ToUpperInvariant()
    }
    return @($hashes | Select-Object -Unique)
}

function Assert-CommonPostActionHash {
    param(
        [Parameter(Mandatory = $true)][string]$HostLog,
        [Parameter(Mandatory = $true)][string]$ClientLog
    )

    $transmitted = @(Get-SnapshotHashes -Text $HostLog -Marker "SMOKE_POST_ACTION_TX")
    $applied = @(Get-SnapshotHashes -Text $ClientLog -Marker "SMOKE_POST_ACTION_APPLIED")
    if (($transmitted.Count -eq 0) -or ($applied.Count -eq 0)) {
        throw ("Post-action snapshot hashes are unavailable (host TX={0}, client applied={1})." -f
            $transmitted.Count, $applied.Count)
    }

    $transmittedLookup = @{}
    foreach ($hash in $transmitted) {
        $transmittedLookup[$hash] = $true
    }
    $common = @()
    foreach ($hash in $applied) {
        if ($transmittedLookup.ContainsKey($hash) -and ($common -notcontains $hash)) {
            $common += $hash
        }
    }
    if ($common.Count -eq 0) {
        throw "No client-applied post-action snapshot hash appears in the host TX log."
    }

    return $common
}

function Wait-ForCleanProcessExit {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$Role,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
        throw ("The {0} process did not exit gracefully within {1}s." -f $Role, $TimeoutSeconds)
    }
    if ($Process.ExitCode -ne 0) {
        throw ("The {0} process exited with code {1}." -f $Role, $Process.ExitCode)
    }
}

function Request-OwnedProcessClose {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$Role
    )

    if ($Process.HasExited) {
        return
    }
    Write-SmokeStatus ("Closing owned {0} PID {1} after both results arrived." -f $Role, $Process.Id)
    # Process.MainWindowHandle is cached.  PvZ can recreate its top-level HWND
    # while switching from loading/menu presentation into a differently sized
    # gameplay window, so refresh before asking .NET to post WM_CLOSE.
    $Process.Refresh()
    if (-not $Process.CloseMainWindow()) {
        # A smoke host self-closes after observing the client's orderly departure.
        # Its HWND can disappear just before the process object reports HasExited;
        # let Wait-ForCleanProcessExit distinguish that normal race from a hang.
        Write-SmokeStatus ("The {0} window is already closing; waiting for PID {1}." -f $Role, $Process.Id)
    }
}

function Stop-OwnedProcess {
    param(
        [AllowNull()][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$Role
    )

    if ($null -eq $Process) {
        return $true
    }

    try {
        if ($Process.HasExited) {
            return $true
        }

        Write-SmokeStatus ("Requesting graceful close of {0} PID {1}." -f $Role, $Process.Id)
        $Process.Refresh()
        [void]$Process.CloseMainWindow()
        if (-not $Process.WaitForExit(5000)) {
            Write-SmokeStatus ("Force-closing owned {0} PID {1}." -f $Role, $Process.Id)
            $Process.Kill()
            [void]$Process.WaitForExit(5000)
        }
        return $Process.HasExited
    }
    catch {
        Write-Warning ("Could not stop owned {0} process safely: {1}" -f $Role, $_.Exception.Message)
        return $false
    }
}

function Test-SafeRunRoot {
    param(
        [Parameter(Mandatory = $true)][string]$RunRoot,
        [Parameter(Mandatory = $true)][string]$ArtifactsRoot,
        [Parameter(Mandatory = $true)][string]$RunId
    )

    $resolvedRunRoot = Get-NormalizedPath -Path $RunRoot
    $resolvedArtifactsRoot = (Get-NormalizedPath -Path $ArtifactsRoot).TrimEnd('\', '/')
    $requiredPrefix = $resolvedArtifactsRoot + [System.IO.Path]::DirectorySeparatorChar
    $expectedLeaf = "run_" + $RunId
    return $resolvedRunRoot.StartsWith($requiredPrefix, [StringComparison]::OrdinalIgnoreCase) -and
        ([System.IO.Path]::GetFileName($resolvedRunRoot) -eq $expectedLeaf)
}

$scriptRoot = Split-Path -Parent $PSCommandPath
$repoRoot = Get-NormalizedPath -Path (Join-Path $scriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($GameExe)) {
    $GameExe = Join-Path $repoRoot "build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe"
}
$GameExe = Get-NormalizedPath -Path $GameExe
$gameDirectory = Split-Path -Parent $GameExe

if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) {
    $ArtifactsRoot = Join-Path ([System.IO.Path]::GetTempPath()) "PvZ-Coop-Smoke"
}
$ArtifactsRoot = Get-NormalizedPath -Path $ArtifactsRoot

$scenarioSpec = switch ($Scenario) {
    "Adventure1" { [ordered]@{ Mode = "adventure"; Level = 1; Marker = "adventure1" } }
    "Adventure5Bowling" { [ordered]@{ Mode = "adventure"; Level = 5; Marker = "adventure5_bowling" } }
    "Adventure5BowlingClient" { [ordered]@{ Mode = "adventure_bowling_client"; Level = 5; Marker = "adventure5_bowling_client" } }
    "Adventure8Soak" { [ordered]@{ Mode = "adventure_soak"; Level = 8; Marker = "adventure8_soak" } }
    "Adventure8Victory" { [ordered]@{ Mode = "adventure_victory"; Level = 8; Marker = "adventure8_victory" } }
    "Adventure15WhackClient" { [ordered]@{ Mode = "adventure_whack_client"; Level = 15; Marker = "adventure15_whack_client" } }
    "SlotMachineClient" { [ordered]@{ Mode = "slot_machine_client"; Level = 1; Marker = "slot_machine_client" } }
    "Vasebreaker" { [ordered]@{ Mode = "vasebreaker"; Level = 1; Marker = "vasebreaker" } }
    "Beghouled" { [ordered]@{ Mode = "beghouled"; Level = 1; Marker = "beghouled" } }
    default { throw ("Unsupported smoke scenario: {0}" -f $Scenario) }
}
$Mode = [string]$scenarioSpec.Mode
$Level = [int]$scenarioSpec.Level
$scenarioMarker = [string]$scenarioSpec.Marker
$isBowlingScenario = $Scenario -in @("Adventure5Bowling", "Adventure5BowlingClient")
if ($Scenario -in @("Adventure8Soak", "Adventure8Victory") -and $SkipIntro -ne 0) {
    throw ("{0} requires -SkipIntro 0 so the two-player seed chooser is exercised." -f $Scenario)
}
$runId = [Guid]::NewGuid().ToString("N")
$runRoot = Join-Path $ArtifactsRoot ("run_" + $runId)
$hostRoot = Join-Path $runRoot "host"
$clientRoot = Join-Path $runRoot "client"
$hostLogDirectory = Join-Path $hostRoot "logs"
$clientLogDirectory = Join-Path $clientRoot "logs"
$hostAppDataDirectory = Join-Path $hostRoot "appdata"
$clientAppDataDirectory = Join-Path $clientRoot "appdata"
$hostConfigPath = Join-Path $hostRoot "coop.ini"
$clientConfigPath = Join-Path $clientRoot "coop.ini"
$hostManifestPath = Join-Path $hostRoot "smoke.ini"
$clientManifestPath = Join-Path $clientRoot "smoke.ini"
$hostResultPath = Join-Path $hostRoot "result.json"
$clientResultPath = Join-Path $clientRoot "result.json"
$pidLedgerPath = Join-Path $runRoot "pids.json"
$summaryPath = Join-Path $runRoot "summary.json"

Ensure-Directory -Path $hostLogDirectory
Ensure-Directory -Path $clientLogDirectory
Ensure-Directory -Path $hostAppDataDirectory
Ensure-Directory -Path $clientAppDataDirectory

$hostProcess = $null
$clientProcess = $null
$success = $false
$failureMessage = ""
$commonSnapshotHashes = @()
$startedUtc = [DateTime]::UtcNow

try {
    if (-not (Test-Path -LiteralPath $GameExe -PathType Leaf)) {
        throw ("Game executable not found: {0}" -f $GameExe)
    }
    # The GOTY Release consumes its base resources from main.pak/dependency.pak.
    # `properties\resources.xml` is an extension source asset and is not copied
    # loose beside the executable, so requiring it rejected valid Release builds.
    foreach ($requiredRelativePath in @("main.pak", "dependency.pak", "SDL3.dll")) {
        $requiredPath = Join-Path $gameDirectory $requiredRelativePath
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw ("Required runtime resource not found: {0}" -f $requiredPath)
        }
    }

    if ($Port -eq 0) {
        $Port = Get-FreeLoopbackPort
    }
    $timeoutMs = $TimeoutSeconds * 1000

    Write-CoopConfig -Path $hostConfigPath -Nickname "SmokeHost" `
        -WindowWidth $HostWindowWidth -WindowHeight $HostWindowHeight
    Write-CoopConfig -Path $clientConfigPath -Nickname "SmokeClient" `
        -WindowWidth $ClientWindowWidth -WindowHeight $ClientWindowHeight
    Write-SmokeManifest -Path $hostManifestPath -Role "host" -Nickname "SmokeHost" `
        -AppDataDirectory $hostAppDataDirectory -ConfigPath $hostConfigPath `
        -LogDirectory $hostLogDirectory -ResultPath $hostResultPath -SelectedPort $Port `
        -SelectedMode $Mode -SelectedLevel $Level -SelectedSeed $Seed `
        -SelectedTimeoutMs $timeoutMs -RequiredSnapshots $MinimumSnapshots `
        -SelectedSkipIntro $SkipIntro
    Write-SmokeManifest -Path $clientManifestPath -Role "client" -Nickname "SmokeClient" `
        -AppDataDirectory $clientAppDataDirectory -ConfigPath $clientConfigPath `
        -LogDirectory $clientLogDirectory -ResultPath $clientResultPath -SelectedPort $Port `
        -SelectedMode $Mode -SelectedLevel $Level -SelectedSeed $Seed `
        -SelectedTimeoutMs $timeoutMs -RequiredSnapshots $MinimumSnapshots `
        -SelectedSkipIntro $SkipIntro

    Write-SmokeStatus ("Artifacts: {0}" -f $runRoot)
    Write-SmokeStatus ("Scenario {0}: mode={1}, level={2}, skip_intro={3}, host={4}x{5}, client={6}x{7}." -f
        $Scenario, $Mode, $Level, $SkipIntro, $HostWindowWidth, $HostWindowHeight,
        $ClientWindowWidth, $ClientWindowHeight)
    Write-SmokeStatus ("Starting host on 127.0.0.1:{0}." -f $Port)
    $hostProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDirectory `
        -ManifestPath $hostManifestPath -Role "host" -RunId $runId
    Set-SmokeWindowLayout -Process $hostProcess -Role "host" -X 0 -Y 0 `
        -Width $HostWindowWidth -Height $HostWindowHeight
    Save-PidLedger -Path $pidLedgerPath -Executable $GameExe `
        -HostProcess $hostProcess -ClientProcess $null

    $hostReadyPattern = '(?im)host listening port=' + [regex]::Escape([string]$Port) + '\b'
    Wait-ForLogPattern -LogDirectory $hostLogDirectory -Pattern $hostReadyPattern `
        -Process $hostProcess -Role "host" -TimeoutSeconds $HostReadyTimeoutSeconds

    Write-SmokeStatus "Host is listening; starting client."
    $clientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDirectory `
        -ManifestPath $clientManifestPath -Role "client" -RunId $runId
    Set-SmokeWindowLayout -Process $clientProcess -Role "client" -X $HostWindowWidth -Y 0 `
        -Width $ClientWindowWidth -Height $ClientWindowHeight
    Save-PidLedger -Path $pidLedgerPath -Executable $GameExe `
        -HostProcess $hostProcess -ClientProcess $clientProcess

    $results = @(Wait-ForResults -HostResultPath $hostResultPath -ClientResultPath $clientResultPath `
        -HostProcess $hostProcess -ClientProcess $clientProcess `
        -HostLogDirectory $hostLogDirectory -ClientLogDirectory $clientLogDirectory `
        -TimeoutSeconds $TimeoutSeconds)
    $hostResult = $results[0]
    $clientResult = $results[1]

    # Close the client first so normal host shutdown cannot be misreported by the
    # client as a lost-host failure. Both exact Process objects remain PID-scoped.
    Request-OwnedProcessClose -Process $clientProcess -Role "client"
    Wait-ForCleanProcessExit -Process $clientProcess -Role "client" -TimeoutSeconds $GracefulExitSeconds
    Request-OwnedProcessClose -Process $hostProcess -Role "host"
    Wait-ForCleanProcessExit -Process $hostProcess -Role "host" -TimeoutSeconds $GracefulExitSeconds

    $hostLog = Get-CombinedLogText -LogDirectory $hostLogDirectory
    $clientLog = Get-CombinedLogText -LogDirectory $clientLogDirectory

    $hostPositivePatterns = @(
        @{ Label = "trace role/protocol"; Pattern = '(?im)role=host protocol=\d+ pid=\d+' },
        @{ Label = "listener"; Pattern = $hostReadyPattern },
        @{ Label = "authenticated client"; Pattern = '(?im)peer authenticated slot=1 nick=SmokeClient\b' },
        @{ Label = "deterministic game start"; Pattern = ('(?im)start game broadcast mode=\d+ level={0} players=2 seed={1}\b' -f $Level, $Seed) },
        @{ Label = "scenario start"; Pattern = ('(?im)\bSMOKE_START\b[^\r\n]*scenario={0}\b' -f ([regex]::Escape($scenarioMarker))) },
        @{ Label = "visible host PLAYING checkpoint"; Pattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=PLAYING[^\r\n]*size={1}x{2}\b' -f ([regex]::Escape($scenarioMarker)), $HostWindowWidth, $HostWindowHeight) },
        @{ Label = "host action applied"; Pattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=ACTION_APPLIED\b' -f ([regex]::Escape($scenarioMarker))) },
        @{ Label = "post-action TX hash"; Pattern = '(?im)\bSMOKE_POST_ACTION_TX\b[^\r\n]*\bhash=(?:0x)?[0-9a-f]{8,16}\b' }
    )
    $clientPositivePatterns = @(
        @{ Label = "trace role/protocol"; Pattern = '(?im)role=client protocol=\d+ pid=\d+' },
        @{ Label = "TCP connection"; Pattern = ('(?im)client TCP connected endpoint=127\.0\.0\.1:{0}\b' -f $Port) },
        @{ Label = "WELCOME"; Pattern = '(?im)WELCOME assignedSlot=1 hostNick=SmokeHost\b' },
        @{ Label = "profile sync"; Pattern = '(?im)PROFILE_SYNC applied\b' },
        @{ Label = "matching game start"; Pattern = ('(?im)client START_GAME decoded mode=\d+ level={0} seed={1}\b' -f $Level, $Seed) },
        @{ Label = "gameplay scene"; Pattern = '(?im)gameplay entered client=1\b' },
        @{ Label = "visible client PLAYING checkpoint"; Pattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=PLAYING[^\r\n]*size={1}x{2}\b' -f ([regex]::Escape($scenarioMarker)), $ClientWindowWidth, $ClientWindowHeight) },
        @{ Label = "client action applied"; Pattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=ACTION_APPLIED\b' -f ([regex]::Escape($scenarioMarker))) },
        @{ Label = "post-action applied hash"; Pattern = '(?im)\bSMOKE_POST_ACTION_APPLIED\b[^\r\n]*\bhash=(?:0x)?[0-9a-f]{8,16}\b' }
    )
    if ($isBowlingScenario) {
        $hostPositivePatterns += @{
            Label = "host moving Bowling nut"
            Pattern = '(?im)\bSMOKE_BOWLING_NUT_MOVING\b[^\r\n]*\brole=host\b[^\r\n]*\bid=\d+\b'
        }
        $clientPositivePatterns += @{
            Label = "client moving Bowling nut"
            Pattern = '(?im)\bSMOKE_BOWLING_NUT_MOVING\b[^\r\n]*\brole=client\b[^\r\n]*\bid=\d+\b'
        }
        $hostPositivePatterns += @{
            Label = "host offscreen zombie spawn"
            Pattern = '(?im)\bSMOKE_ZOMBIE_SPAWN_OBSERVED\b[^\r\n]*\brole=host\b[^\r\n]*\bid=\d+\b'
        }
        $clientPositivePatterns += @{
            Label = "client offscreen zombie spawn"
            Pattern = '(?im)\bSMOKE_ZOMBIE_SPAWN_OBSERVED\b[^\r\n]*\brole=client\b[^\r\n]*\bid=\d+\b'
        }
        if ($Scenario -eq "Adventure5BowlingClient") {
            $hostPositivePatterns += @{
                Label = "host accepted remote conveyor packet"
                Pattern = '(?im)\bmouse seed remap\b[^\r\n]*\bslot=1\b[^\r\n]*\bpacketId=[1-9]\d*\b'
            }
            $clientPositivePatterns += @{
                Label = "client issued remote Bowling input"
                Pattern = '(?im)\bSMOKE_BOWLING_CLIENT_INPUT\b[^\r\n]*\bindex=\d+\b[^\r\n]*\bpacketId=[1-9]\d*\b[^\r\n]*\bduplicates=\d+\b'
            }
        }
    }
    if ($Scenario -eq "Adventure15WhackClient") {
        $hostPositivePatterns += @(
            @{
                Label = "host authoritative Whack hit"
                Pattern = '(?im)\bSMOKE_WHACK_HOST_HIT\b[^\r\n]*\btargetId=[1-9]\d*\b'
            },
            @{
                Label = "host Whack removal"
                Pattern = '(?im)\bSMOKE_WHACK_REMOVAL_OBSERVED\b[^\r\n]*\brole=host\b[^\r\n]*\btargetId=[1-9]\d*\b'
            }
        )
        $clientPositivePatterns += @(
            @{
                Label = "client Whack input"
                Pattern = '(?im)\bSMOKE_WHACK_CLIENT_INPUT\b[^\r\n]*\btargetId=[1-9]\d*\b'
            },
            @{
                Label = "client Whack removal"
                Pattern = '(?im)\bSMOKE_WHACK_REMOVAL_OBSERVED\b[^\r\n]*\brole=client\b[^\r\n]*\btargetId=[1-9]\d*\b'
            }
        )
    }
    if ($Scenario -eq "SlotMachineClient") {
        $hostPositivePatterns += @(
            @{
                Label = "host Slot Machine pull presentation"
                Pattern = '(?im)\bSMOKE_SLOT_MACHINE_PRESENTATION\b[^\r\n]*\brole=host\b[^\r\n]*\broll=[1-9]\d*\b'
            },
            @{
                Label = "host Slot Machine return"
                Pattern = '(?im)\bSMOKE_SLOT_MACHINE_RETURN\b[^\r\n]*\brole=host\b[^\r\n]*\broll=[1-9]\d*\b[^\r\n]*\bsounds=1\b'
            },
            @{
                Label = "host Slot Machine sound"
                Pattern = '(?im)\bSMOKE_SLOT_MACHINE_SOUND\b[^\r\n]*\brole=host\b[^\r\n]*\bcount=1\b'
            }
        )
        $clientPositivePatterns += @(
            @{
                Label = "client Slot Machine input"
                Pattern = '(?im)\bSMOKE_SLOT_MACHINE_CLIENT_INPUT\b[^\r\n]*\broll=\d+\b'
            },
            @{
                Label = "client Slot Machine pull presentation"
                Pattern = '(?im)\bSMOKE_SLOT_MACHINE_PRESENTATION\b[^\r\n]*\brole=client\b[^\r\n]*\broll=[1-9]\d*\b'
            },
            @{
                Label = "client Slot Machine transition hook"
                Pattern = '(?im)\bslot-machine presentation pull\b[^\r\n]*\broll=[1-9]\d*\b'
            },
            @{
                Label = "client Slot Machine return"
                Pattern = '(?im)\bSMOKE_SLOT_MACHINE_RETURN\b[^\r\n]*\brole=client\b[^\r\n]*\broll=[1-9]\d*\b[^\r\n]*\bsounds=1\b'
            },
            @{
                Label = "client Slot Machine sound"
                Pattern = '(?im)\bSMOKE_SLOT_MACHINE_SOUND\b[^\r\n]*\brole=client\b[^\r\n]*\bcount=1\b'
            }
        )
    }
    if ($Scenario -eq "Adventure8Soak") {
        $hostPositivePatterns += @(
            @{ Label = "host chooser quota"; Pattern = '(?im)\bSMOKE_SOAK_CHOOSER_VERIFIED\b[^\r\n]*\brole=host\b' },
            @{ Label = "host real plant input"; Pattern = '(?im)\bSMOKE_SOAK_PLANT\b[^\r\n]*\brole=host\b' },
            @{ Label = "host real sun input"; Pattern = '(?im)\bSMOKE_SOAK_SUN_CLICK\b[^\r\n]*\brole=host\b' },
            @{ Label = "host forced stock loss"; Pattern = '(?im)\bSMOKE_SOAK_FORCE_LOSS\b[^\r\n]*\brole=host\b[^\r\n]*\belapsed=6\d{4,}\b' },
            @{ Label = "host observed loss"; Pattern = '(?im)\bSMOKE_SOAK_LOSS_OBSERVED\b[^\r\n]*\brole=host\b' },
            @{ Label = "host loss camera"; Pattern = '(?im)\bSMOKE_SOAK_LOSS_CAMERA\b[^\r\n]*\brole=host\b' },
            @{ Label = "host brains presentation"; Pattern = '(?im)\bSMOKE_SOAK_LOSS_BRAINS\b[^\r\n]*\brole=host\b' },
            @{ Label = "host loss dialog"; Pattern = '(?im)\bSMOKE_SOAK_LOSS_DIALOG\b[^\r\n]*\brole=host\b' }
        )
        $clientPositivePatterns += @(
            @{ Label = "client chooser quota"; Pattern = '(?im)\bSMOKE_SOAK_CHOOSER_VERIFIED\b[^\r\n]*\brole=client\b' },
            @{ Label = "client real plant input"; Pattern = '(?im)\bSMOKE_SOAK_PLANT\b[^\r\n]*\brole=client\b' },
            @{ Label = "client real sun input"; Pattern = '(?im)\bSMOKE_SOAK_SUN_CLICK\b[^\r\n]*\brole=client\b' },
            @{ Label = "client observed synchronized loss"; Pattern = '(?im)\bSMOKE_SOAK_LOSS_OBSERVED\b[^\r\n]*\brole=client\b' },
            @{ Label = "client loss camera"; Pattern = '(?im)\bSMOKE_SOAK_LOSS_CAMERA\b[^\r\n]*\brole=client\b' },
            @{ Label = "client brains presentation"; Pattern = '(?im)\bSMOKE_SOAK_LOSS_BRAINS\b[^\r\n]*\brole=client\b' },
            @{ Label = "client visual loss dialog"; Pattern = '(?im)\bSMOKE_SOAK_LOSS_DIALOG\b[^\r\n]*\brole=client\b' }
        )
    }
    if ($Scenario -eq "Adventure8Victory") {
        $hostPositivePatterns += @(
            @{ Label = "host victory chooser quota"; Pattern = '(?im)\bSMOKE_SOAK_CHOOSER_VERIFIED\b[^\r\n]*\brole=host\b' },
            @{ Label = "host ready-set-plant animation"; Pattern = '(?im)\bREADY_SET_PLANT sent\b[^\r\n]*\breanim=[1-9]\d*\b' },
            @{ Label = "host victory plant input"; Pattern = '(?im)\bSMOKE_SOAK_PLANT\b[^\r\n]*\brole=host\b' },
            @{ Label = "host stock victory fade request"; Pattern = '(?im)\bSMOKE_VICTORY_FORCE_FADE\b[^\r\n]*\brole=host\b[^\r\n]*\belapsed=[2-9]\d{4,}\b' },
            @{ Label = "host observed victory fade"; Pattern = '(?im)\bSMOKE_VICTORY_FADE\b[^\r\n]*\brole=host\b[^\r\n]*\bfade=[1-9]\d*\b' },
            @{ Label = "host award UI"; Pattern = ('(?im)\bSMOKE_VICTORY_AWARD\b[^\r\n]*\brole=host\b[^\r\n]*\baward=0\b[^\r\n]*\bboard=0\b[^\r\n]*\bscreen=1\b[^\r\n]*\bsize={0}x{1}\b' -f $HostWindowWidth, $HostWindowHeight) }
        )
        $clientPositivePatterns += @(
            @{ Label = "client victory chooser quota"; Pattern = '(?im)\bSMOKE_SOAK_CHOOSER_VERIFIED\b[^\r\n]*\brole=client\b' },
            @{ Label = "client ready-set-plant animation"; Pattern = '(?im)\bREADY_SET_PLANT applied\b[^\r\n]*\breanim=[1-9]\d*\b' },
            @{ Label = "client victory plant input"; Pattern = '(?im)\bSMOKE_SOAK_PLANT\b[^\r\n]*\brole=client\b' },
            @{ Label = "client observed synchronized victory fade"; Pattern = '(?im)\bSMOKE_VICTORY_FADE\b[^\r\n]*\brole=client\b[^\r\n]*\bfade=[1-9]\d*\b' },
            @{ Label = "client synchronized award UI"; Pattern = ('(?im)\bSMOKE_VICTORY_AWARD\b[^\r\n]*\brole=client\b[^\r\n]*\baward=0\b[^\r\n]*\bboard=0\b[^\r\n]*\bscreen=1\b[^\r\n]*\bsize={0}x{1}\b' -f $ClientWindowWidth, $ClientWindowHeight) }
        )
    }
    $negativePatterns = @(
        @{ Label = "access violation"; Pattern = '(?im)Access Violation' },
        @{ Label = "fatal network error"; Pattern = '(?im)\bfatal:' },
        @{ Label = "version mismatch"; Pattern = '(?im)version mismatch' },
        @{ Label = "host loss"; Pattern = '(?im)host (?:was )?lost|connection to the host was lost' },
        @{ Label = "protocol violation"; Pattern = '(?im)protocol violation' },
        @{ Label = "queue overflow"; Pattern = '(?im)queue overflow' },
        @{ Label = "rejected snapshot"; Pattern = '(?im)snapshot[^\r\n]*\brejected\b' },
        @{ Label = "malformed start"; Pattern = '(?im)START_GAME[^\r\n]*\bmalformed\b' },
        @{ Label = "unhandled exception"; Pattern = '(?im)unhandled exception|assertion failed' },
        @{ Label = "invalid Bowling ghost target"; Pattern = '(?im)\bSMOKE_BOWLING_TARGET_INVALID\b' },
        @{ Label = "invalid Bowling presentation"; Pattern = '(?im)\bSMOKE_BOWLING_PRESENTATION_INVALID\b' },
        @{ Label = "uninitialized zombie ghost target"; Pattern = '(?im)\bSMOKE_ZOMBIE_TARGET_INVALID\b' },
        @{ Label = "on-lawn zombie spawn"; Pattern = '(?im)\bSMOKE_ZOMBIE_SPAWN_INVALID\b' }
    )

    Assert-PositivePatterns -Role "host" -Text $hostLog -Patterns $hostPositivePatterns
    Assert-PositivePatterns -Role "client" -Text $clientLog -Patterns $clientPositivePatterns
    Assert-NoNegativePatterns -Role "host" -Text $hostLog -Patterns $negativePatterns
    Assert-NoNegativePatterns -Role "client" -Text $clientLog -Patterns $negativePatterns

    $combinedActionLog = $hostLog + [Environment]::NewLine + $clientLog
    $expectedActionOwner = if ($Scenario -eq "Adventure5BowlingClient" -or
        $Scenario -eq "Adventure15WhackClient" -or
        $Scenario -eq "SlotMachineClient" -or
        $Scenario -eq "Vasebreaker" -or $Scenario -eq "Beghouled") { "client" } else { "host" }
    $actionIssuedPattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=ACTION_ISSUED[^\r\n]*owner={1}\b' -f
        ([regex]::Escape($scenarioMarker)), $expectedActionOwner)
    if (-not [regex]::IsMatch($combinedActionLog, $actionIssuedPattern)) {
        throw ("No deterministic ACTION_ISSUED checkpoint was logged by expected owner {0}." -f
            $expectedActionOwner)
    }

    if ($isBowlingScenario) {
        $hostNut = [regex]::Match($hostLog,
            '(?im)\bSMOKE_BOWLING_NUT_MOVING\b[^\r\n]*\brole=host\b[^\r\n]*\bid=(\d+)\b')
        $clientNut = [regex]::Match($clientLog,
            '(?im)\bSMOKE_BOWLING_NUT_MOVING\b[^\r\n]*\brole=client\b[^\r\n]*\bid=(\d+)\b')
        if ((-not $hostNut.Success) -or (-not $clientNut.Success) -or
            ($hostNut.Groups[1].Value -ne $clientNut.Groups[1].Value)) {
            throw ("Bowling nut identity/movement mismatch: host={0}, client={1}." -f
                $hostNut.Groups[1].Value, $clientNut.Groups[1].Value)
        }
        $hostZombie = [regex]::Match($hostLog,
            '(?im)\bSMOKE_ZOMBIE_SPAWN_OBSERVED\b[^\r\n]*\brole=host\b[^\r\n]*\bid=(\d+)\b')
        $clientZombie = [regex]::Match($clientLog,
            '(?im)\bSMOKE_ZOMBIE_SPAWN_OBSERVED\b[^\r\n]*\brole=client\b[^\r\n]*\bid=(\d+)\b')
        if ((-not $hostZombie.Success) -or (-not $clientZombie.Success) -or
            ($hostZombie.Groups[1].Value -ne $clientZombie.Groups[1].Value)) {
            throw ("Zombie spawn identity/presentation mismatch: host={0}, client={1}." -f
                $hostZombie.Groups[1].Value, $clientZombie.Groups[1].Value)
        }
        if ($Scenario -eq "Adventure5BowlingClient") {
            $clientPacket = [regex]::Match($clientLog,
                '(?im)\bSMOKE_BOWLING_CLIENT_INPUT\b[^\r\n]*\bpacketId=([1-9]\d*)\b[^\r\n]*\bduplicates=(\d+)\b')
            $hostRemap = [regex]::Match($hostLog,
                '(?im)\bmouse seed remap\b[^\r\n]*\bslot=1\b[^\r\n]*\bpacketId=([1-9]\d*)\b')
            $hostApply = [regex]::Match($hostLog,
                '(?im)\bmouse apply seed revalidated\b[^\r\n]*\bslot=1\b[^\r\n]*\bpacketId=([1-9]\d*)\b')
            if ((-not $clientPacket.Success) -or (-not $hostRemap.Success) -or
                (-not $hostApply.Success)) {
                throw ("Missing remote Bowling packet evidence: client={0}, remap={1}, apply={2}." -f
                    $clientPacket.Success, $hostRemap.Success, $hostApply.Success)
            }
            $duplicateCount = [int]$clientPacket.Groups[2].Value
            $clientPacketId = [uint32]$clientPacket.Groups[1].Value
            $hostRemapPacketId = [uint32]$hostRemap.Groups[1].Value
            $hostApplyPacketId = [uint32]$hostApply.Groups[1].Value
            if ($duplicateCount -lt 2) {
                throw ("Remote Bowling input did not exercise duplicate cards: duplicates={0}." -f
                    $duplicateCount)
            }
            if (($clientPacketId -ne $hostRemapPacketId) -or
                ($clientPacketId -ne $hostApplyPacketId)) {
                throw ("Remote Bowling packet identity mismatch: client={0}, remap={1}, apply={2}." -f
                    $clientPacketId, $hostRemapPacketId, $hostApplyPacketId)
            }
        }
    }
    if ($Scenario -eq "Adventure15WhackClient") {
        $clientWhack = [regex]::Match($clientLog,
            '(?im)\bSMOKE_WHACK_CLIENT_INPUT\b[^\r\n]*\btargetId=([1-9]\d*)\b')
        $hostWhack = [regex]::Match($hostLog,
            '(?im)\bSMOKE_WHACK_HOST_HIT\b[^\r\n]*\btargetId=([1-9]\d*)\b')
        $hostRemoval = [regex]::Match($hostLog,
            '(?im)\bSMOKE_WHACK_REMOVAL_OBSERVED\b[^\r\n]*\brole=host\b[^\r\n]*\btargetId=([1-9]\d*)\b')
        $clientRemoval = [regex]::Match($clientLog,
            '(?im)\bSMOKE_WHACK_REMOVAL_OBSERVED\b[^\r\n]*\brole=client\b[^\r\n]*\btargetId=([1-9]\d*)\b')
        $whackEvidence = @($clientWhack, $hostWhack, $hostRemoval, $clientRemoval)
        if (@($whackEvidence | Where-Object { -not $_.Success }).Count -ne 0) {
            throw "Missing Whack-a-Zombie target lifecycle evidence."
        }
        $whackIds = @($whackEvidence | ForEach-Object { [uint32]$_.Groups[1].Value } |
            Select-Object -Unique)
        if ($whackIds.Count -ne 1) {
            throw ("Whack-a-Zombie target identity mismatch: {0}." -f
                ($whackIds -join ","))
        }
    }
    if ($Scenario -eq "SlotMachineClient") {
        $clientSlotInput = [regex]::Match($clientLog,
            '(?im)\bSMOKE_SLOT_MACHINE_CLIENT_INPUT\b[^\r\n]*\broll=(\d+)\b')
        $hostSlotPresentation = [regex]::Match($hostLog,
            '(?im)\bSMOKE_SLOT_MACHINE_PRESENTATION\b[^\r\n]*\brole=host\b[^\r\n]*\broll=([1-9]\d*)\b')
        $clientSlotPresentation = [regex]::Match($clientLog,
            '(?im)\bSMOKE_SLOT_MACHINE_PRESENTATION\b[^\r\n]*\brole=client\b[^\r\n]*\broll=([1-9]\d*)\b')
        $hostSlotReturn = [regex]::Match($hostLog,
            '(?im)\bSMOKE_SLOT_MACHINE_RETURN\b[^\r\n]*\brole=host\b[^\r\n]*\broll=([1-9]\d*)\b[^\r\n]*\bsounds=1\b')
        $clientSlotReturn = [regex]::Match($clientLog,
            '(?im)\bSMOKE_SLOT_MACHINE_RETURN\b[^\r\n]*\brole=client\b[^\r\n]*\broll=([1-9]\d*)\b[^\r\n]*\bsounds=1\b')
        if (-not $clientSlotInput.Success -or -not $hostSlotPresentation.Success -or
            -not $clientSlotPresentation.Success -or -not $hostSlotReturn.Success -or
            -not $clientSlotReturn.Success) {
            throw "Missing Slot Machine input/presentation/return correlation evidence."
        }
        $expectedSlotRoll = [uint32]$clientSlotInput.Groups[1].Value + 1
        $hostSlotRoll = [uint32]$hostSlotPresentation.Groups[1].Value
        $clientSlotRoll = [uint32]$clientSlotPresentation.Groups[1].Value
        $hostSlotReturnRoll = [uint32]$hostSlotReturn.Groups[1].Value
        $clientSlotReturnRoll = [uint32]$clientSlotReturn.Groups[1].Value
        if ($hostSlotRoll -ne $expectedSlotRoll -or $clientSlotRoll -ne $expectedSlotRoll -or
            $hostSlotReturnRoll -ne $expectedSlotRoll -or
            $clientSlotReturnRoll -ne $expectedSlotRoll) {
            throw ("Slot Machine roll mismatch: input={0}, expected={1}, hostPull={2}, clientPull={3}, hostReturn={4}, clientReturn={5}." -f
                $clientSlotInput.Groups[1].Value, $expectedSlotRoll, $hostSlotRoll,
                $clientSlotRoll, $hostSlotReturnRoll, $clientSlotReturnRoll)
        }
    }

    $hostProtocol = Assert-SmokeResult -Result $hostResult -Role "host" -ExpectedMode $Mode `
        -ExpectedScenario $scenarioMarker -ExpectedLevel $Level -ExpectedSeed $Seed `
        -RequiredSnapshots $MinimumSnapshots -ExpectedSkipIntro $SkipIntro
    $clientProtocol = Assert-SmokeResult -Result $clientResult -Role "client" -ExpectedMode $Mode `
        -ExpectedScenario $scenarioMarker -ExpectedLevel $Level -ExpectedSeed $Seed `
        -RequiredSnapshots $MinimumSnapshots -ExpectedSkipIntro $SkipIntro
    if ($hostProtocol -ne $clientProtocol) {
        throw ("Protocol mismatch in result files: host={0}, client={1}." -f $hostProtocol, $clientProtocol)
    }

    $commonSnapshotHashes = @(Assert-CommonPostActionHash -HostLog $hostLog -ClientLog $clientLog)
    $success = $true
    Write-SmokeStatus ("PASS: protocol {0}, common snapshot hash {1}." -f
        $hostProtocol, $commonSnapshotHashes[0])
}
catch {
    $failureMessage = $_.Exception.Message
    if (-not [string]::IsNullOrWhiteSpace($_.ScriptStackTrace)) {
        $failureMessage += " | " + $_.ScriptStackTrace
    }
    Write-Host ("[CoopSmoke] FAIL: {0}" -f $failureMessage) -ForegroundColor Red
}
finally {
    $clientStopped = Stop-OwnedProcess -Process $clientProcess -Role "client"
    $hostStopped = Stop-OwnedProcess -Process $hostProcess -Role "host"
    if ((-not $clientStopped) -or (-not $hostStopped)) {
        $success = $false
        if ([string]::IsNullOrWhiteSpace($failureMessage)) {
            $failureMessage = "One or more owned game processes could not be stopped safely."
        }
    }

    $summaryStatus = if ($success) { "pass" } else { "fail" }
    $hostProcessId = if ($null -ne $hostProcess) { $hostProcess.Id } else { $null }
    $clientProcessId = if ($null -ne $clientProcess) { $clientProcess.Id } else { $null }
    $summary = [ordered]@{
        status = $summaryStatus
        run_id = $runId
        started_utc = $startedUtc.ToString("o")
        finished_utc = [DateTime]::UtcNow.ToString("o")
        executable = $GameExe
        port = $Port
        scenario = $Scenario
        mode = $Mode
        level = $Level
        skip_intro = $SkipIntro
        host_window = ("{0}x{1}" -f $HostWindowWidth, $HostWindowHeight)
        client_window = ("{0}x{1}" -f $ClientWindowWidth, $ClientWindowHeight)
        seed = $Seed
        minimum_snapshots = $MinimumSnapshots
        host_pid = $hostProcessId
        client_pid = $clientProcessId
        common_snapshot_hashes = $commonSnapshotHashes
        error = $failureMessage
        artifacts_root = $runRoot
    }
    Write-Utf8NoBom -Path $summaryPath -Text (($summary | ConvertTo-Json -Depth 6) + [Environment]::NewLine)
}

if (-not $success) {
    Write-SmokeStatus ("Failure artifacts preserved at: {0}" -f $runRoot)
    exit 1
}

if ($CleanupOnSuccess) {
    if (-not (Test-SafeRunRoot -RunRoot $runRoot -ArtifactsRoot $ArtifactsRoot -RunId $runId)) {
        Write-Host "[CoopSmoke] Refusing unsafe cleanup; artifacts were preserved." -ForegroundColor Red
        exit 1
    }
    Remove-Item -LiteralPath $runRoot -Recurse -Force
    Write-SmokeStatus "PASS; the verified per-run artifact directory was removed."
}
else {
    Write-SmokeStatus ("PASS artifacts preserved at: {0}" -f $runRoot)
}

exit 0
