#requires -Version 5.1

[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$OnlineEndpoint = "",
    [string]$ArtifactsRoot = "",
    [string]$HostConfigSource = "",
    [string]$ClientConfigSource = "",
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
    [ValidateSet("Adventure1", "Adventure5Bowling", "Adventure5BowlingClient", "Adventure8Soak", "Adventure8Victory", "Adventure15WhackClient", "SlotMachineClient", "TowerDefenseClient", "PortalCombatClient", "PvP1v1", "EndlessST", "EndlessOverflow", "Vasebreaker", "Beghouled")]
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
    [switch]$VerifyUnicodeChat,
    [switch]$VerifyInteractions,
    [switch]$VerifyPresentation,
    [switch]$VerifySavedAvatars,
    [switch]$VerifyModes,
    [switch]$VerifyFirstAdventure,
    [switch]$Background,
    [switch]$CleanupOnSuccess,
    [switch]$FunctionsOnly
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

    [DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    public static bool CloseOwnedWindow(int processId)
    {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr ignored)
        {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == (uint)processId && GetWindow(hwnd, 4) == IntPtr.Zero)
            { result = hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return result != IntPtr.Zero && PostMessage(result, 0x0010, IntPtr.Zero, IntPtr.Zero);
    }

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

    if ($Background) {
        Write-SmokeStatus ("Background {0} PID {1}: game window stays hidden; renderer captures only." -f $Role, $Process.Id)
        return
    }
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
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][bool]$EnableUnicodeChat
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
    if ($env:PVZ_COOP_SMOKE_SPECTATOR -eq "1") {
        $childEnvironment["PVZ_COOP_SMOKE_SPECTATOR_ROLE"] = $Role
    }
    $childEnvironment["PVZ_COOP_SMOKE_UNICODE_CHAT"] = $(if ($EnableUnicodeChat) { "1" } else { "0" })
    $childEnvironment["PVZ_COOP_SMOKE_INTERACTIONS"] = $(if ($VerifyInteractions) { "1" } else { "0" })
    $childEnvironment["PVZ_COOP_SMOKE_PRESENTATION"] = $(if ($VerifyPresentation -or $VerifySavedAvatars) { "1" } else { "0" })
    $childEnvironment["PVZ_COOP_SMOKE_SAVED_AVATARS"] = $(if ($VerifySavedAvatars) { "1" } else { "0" })
    $childEnvironment["PVZ_COOP_SMOKE_AVATAR_EDITOR"] = "0"
    $childEnvironment["PVZ_COOP_SMOKE_MODES"] = $(if ($VerifyModes) { "1" } else { "0" })
    $childEnvironment["PVZ_COOP_SMOKE_FIRST_ADVENTURE"] = $(if ($VerifyFirstAdventure) { "1" } else { "0" })
    $nativeUiWorkflow = $env:PVZ_COOP_SMOKE_NATIVE_UI -in @("appearance", "appearance-restart", "browser", "browser-probe")
    $childEnvironment["PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT"] = $(if ($VerifyFirstAdventure -or $nativeUiWorkflow) { "1" } else { "0" })
    $childEnvironment["PVZ_COOP_SMOKE_BACKGROUND"] = $(if ($Background) { "1" } else { "0" })
    $childEnvironment["PVZ_COOP_SMOKE_ONLINE_ENDPOINT"] = $OnlineEndpoint
    if ($Background) {
        $childEnvironment["PVZ_COOP_SMOKE_CAPTURE_DIR"] = [System.IO.Path]::GetDirectoryName($ManifestPath)
    }

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
    $closeRequested = if ($Background) { [CoopSmokeNativeWindow]::CloseOwnedWindow($Process.Id) } else { $Process.CloseMainWindow() }
    if (-not $closeRequested) {
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
        if ($Background) { [void][CoopSmokeNativeWindow]::CloseOwnedWindow($Process.Id) }
        else { [void]$Process.CloseMainWindow() }
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

if ($FunctionsOnly) { return }

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
    "TowerDefenseClient" { [ordered]@{ Mode = "tower_defense_client"; Level = 1; Marker = "tower_defense_client" } }
    "PortalCombatClient" { [ordered]@{ Mode = "portal_combat_client"; Level = 1; Marker = "portal_combat_client" } }
    "PvP1v1" { [ordered]@{ Mode = "pvp_1v1"; Level = 1; Marker = "pvp_1v1" } }
    "EndlessST" { [ordered]@{ Mode = "endless_st"; Level = 1; Marker = "endless_st" } }
    "EndlessOverflow" { [ordered]@{ Mode = "endless_overflow"; Level = 1; Marker = "endless_overflow" } }
    "Vasebreaker" { [ordered]@{ Mode = "vasebreaker"; Level = 1; Marker = "vasebreaker" } }
    "Beghouled" { [ordered]@{ Mode = "beghouled"; Level = 1; Marker = "beghouled" } }
    default { throw ("Unsupported smoke scenario: {0}" -f $Scenario) }
}

if (($HostConfigSource -or $ClientConfigSource) -and -not $VerifySavedAvatars) {
    throw "Personal config sources are restricted to the isolated saved-avatar workflow."
}
if ($VerifySavedAvatars -and (-not $HostConfigSource -or -not $ClientConfigSource -or
    -not $Background -or $Scenario -ne "Adventure1" -or $SkipIntro -ne 0 -or $VerifyPresentation -or
    $VerifyInteractions -or $VerifyModes -or $VerifyFirstAdventure -or $VerifyUnicodeChat -or $OnlineEndpoint -or
    $env:PVZ_COOP_SMOKE_SPECTATOR -eq "1" -or
    $HostWindowWidth -ne 800 -or $HostWindowHeight -ne 600 -or
    $ClientWindowWidth -ne 800 -or $ClientWindowHeight -ne 600)) {
    throw "VerifySavedAvatars requires both editor-produced config sources and isolated Background Adventure1 SkipIntro0 at 800x600."
}
if ($VerifyPresentation -and (-not $Background -or $Scenario -ne "Adventure1" -or $SkipIntro -ne 0 -or
    $VerifyInteractions -or $VerifyModes -or $VerifyFirstAdventure -or $VerifyUnicodeChat -or $OnlineEndpoint -or
    $env:PVZ_COOP_SMOKE_SPECTATOR -eq "1" -or
    $HostWindowWidth -ne 800 -or $HostWindowHeight -ne 600 -or
    $ClientWindowWidth -ne 800 -or $ClientWindowHeight -ne 600)) {
    throw "VerifyPresentation requires isolated -Background -Scenario Adventure1 -SkipIntro 0 at 800x600, without other opt-in workflows."
}
if ($VerifyInteractions -and (-not $Background -or $Scenario -ne "Adventure1" -or
    $HostWindowWidth -ne 800 -or $HostWindowHeight -ne 600 -or
    $ClientWindowWidth -ne 800 -or $ClientWindowHeight -ne 600)) {
    throw "VerifyInteractions requires -Background -Scenario Adventure1 and host/client 800x600 dimensions."
}
if ($VerifyFirstAdventure -and (-not $Background -or $Scenario -ne "Adventure1" -or
    $SkipIntro -ne 0 -or $TimeoutSeconds -lt 180 -or $VerifyInteractions -or $VerifyUnicodeChat -or
    $OnlineEndpoint -or $env:PVZ_COOP_SMOKE_SPECTATOR -eq "1" -or
    $HostWindowWidth -ne 800 -or $HostWindowHeight -ne 600 -or
    $ClientWindowWidth -ne 800 -or $ClientWindowHeight -ne 600)) {
    throw "VerifyFirstAdventure requires -Background -Scenario Adventure1 -SkipIntro 0 -TimeoutSeconds 180+ and 800x600 peers; run other optional workflows separately."
}
$Mode = [string]$scenarioSpec.Mode
if ($VerifyModes -and (-not $Background -or $Scenario -notin @("PvP1v1", "TowerDefenseClient") -or
    $HostWindowWidth -ne 800 -or $HostWindowHeight -ne 600 -or
    $ClientWindowWidth -ne 800 -or $ClientWindowHeight -ne 600)) {
    throw "VerifyModes requires -Background and PvP1v1 or TowerDefenseClient with host/client 800x600."
}
$Level = [int]$scenarioSpec.Level
$scenarioMarker = [string]$scenarioSpec.Marker
$isBowlingScenario = $Scenario -in @("Adventure5Bowling", "Adventure5BowlingClient")
if ($Scenario -in @("Adventure8Soak", "Adventure8Victory") -and $SkipIntro -ne 0) {
    throw ("{0} requires -SkipIntro 0 so the two-player seed chooser is exercised." -f $Scenario)
}
if ($Scenario -eq "TowerDefenseClient" -and $SkipIntro -ne 0) {
    throw "TowerDefenseClient requires -SkipIntro 0 so the generated path and client plant action are exercised."
}
if ($Scenario -eq "EndlessST" -and $SkipIntro -ne 0) {
    throw "EndlessST requires -SkipIntro 0 so both the initial chooser and ten-wave repick chooser are exercised."
}
if ($Scenario -eq "EndlessOverflow" -and $SkipIntro -ne 0) {
    throw "EndlessOverflow requires -SkipIntro 0 so both stock seed choosers are exercised."
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

    # Build Cyrillic names from code points so Windows PowerShell 5.1 does not
    # depend on whether this UTF-8 script itself carries a BOM.
    $unicodeHostNickname = -join @([char]0x425, [char]0x43E, [char]0x441, [char]0x442)
    $unicodeClientNickname = -join @([char]0x41A, [char]0x43B, [char]0x438,
        [char]0x435, [char]0x43D, [char]0x442)
    $hostNickname = $(if ($VerifyUnicodeChat) { $unicodeHostNickname } else { "SmokeHost" })
    $clientNickname = $(if ($VerifyUnicodeChat) { $unicodeClientNickname } else { "SmokeClient" })
    Write-CoopConfig -Path $hostConfigPath -Nickname $hostNickname `
        -WindowWidth $HostWindowWidth -WindowHeight $HostWindowHeight
    Write-CoopConfig -Path $clientConfigPath -Nickname $clientNickname `
        -WindowWidth $ClientWindowWidth -WindowHeight $ClientWindowHeight
    if ($VerifySavedAvatars) {
        foreach ($role in @('host', 'client')) {
            $source = if ($role -eq 'host') { $HostConfigSource } else { $ClientConfigSource }
            $destination = if ($role -eq 'host') { $hostConfigPath } else { $clientConfigPath }
            $source = (Resolve-Path -LiteralPath $source).Path
            $editorProof = Get-Content -LiteralPath (Join-Path (Split-Path -Parent $source) 'result.json') -Raw | ConvertFrom-Json
            if ($editorProof.status -ne 'pass' -or $editorProof.test -ne 'avatar-editor' -or
                $editorProof.input -ne 'SDL_queue' -or $editorProof.bytes -ne 13 -or
                $editorProof.appearance -notmatch '^[A-F0-9]{26}$' -or
                [IO.File]::ReadAllText($source) -notmatch ('(?m)^zombatar=' + [regex]::Escape($editorProof.appearance) + '\r?$')) {
                throw 'A personal config source was not produced by the isolated native SDL avatar editor fixture.'
            }
            Copy-Item -LiteralPath $source -Destination $destination -Force
            if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
                (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash) { throw 'Saved personal INI copy changed bytes.' }
            if ($role -eq 'host') { $hostSavedAppearance = [string]$editorProof.appearance }
            else { $clientSavedAppearance = [string]$editorProof.appearance }
        }
        if ($hostSavedAppearance -eq $clientSavedAppearance) { throw 'Saved personal appearances must be distinct.' }
    }
    Write-SmokeManifest -Path $hostManifestPath -Role "host" -Nickname $hostNickname `
        -AppDataDirectory $hostAppDataDirectory -ConfigPath $hostConfigPath `
        -LogDirectory $hostLogDirectory -ResultPath $hostResultPath -SelectedPort $Port `
        -SelectedMode $Mode -SelectedLevel $Level -SelectedSeed $Seed `
        -SelectedTimeoutMs $timeoutMs -RequiredSnapshots $MinimumSnapshots `
        -SelectedSkipIntro $SkipIntro
    Write-SmokeManifest -Path $clientManifestPath -Role "client" -Nickname $clientNickname `
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
        -ManifestPath $hostManifestPath -Role "host" -RunId $runId `
        -EnableUnicodeChat ([bool]$VerifyUnicodeChat)
    Set-SmokeWindowLayout -Process $hostProcess -Role "host" -X 0 -Y 0 `
        -Width $HostWindowWidth -Height $HostWindowHeight
    Save-PidLedger -Path $pidLedgerPath -Executable $GameExe `
        -HostProcess $hostProcess -ClientProcess $null

    $hostReadyPattern = if ($OnlineEndpoint) { '(?im)WebRTC room hosting build=' } else {
        '(?im)host listening port=' + [regex]::Escape([string]$Port) + '\b'
    }
    Wait-ForLogPattern -LogDirectory $hostLogDirectory -Pattern $hostReadyPattern `
        -Process $hostProcess -Role "host" -TimeoutSeconds $HostReadyTimeoutSeconds

    Write-SmokeStatus $(if ($OnlineEndpoint) { "WebRTC room created; starting client." } else { "Host is listening; starting client." })
    $clientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDirectory `
        -ManifestPath $clientManifestPath -Role "client" -RunId $runId `
        -EnableUnicodeChat ([bool]$VerifyUnicodeChat)
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

    if ($Background -and $OnlineEndpoint) {
        # The renderer refreshes scene BMPs every two seconds. Keep the
        # successful game alive long enough to capture its applied action.
        Start-Sleep -Milliseconds 2200
    }

    # Close the client first so normal host shutdown cannot be misreported by the
    # client as a lost-host failure. Both exact Process objects remain PID-scoped.
    Request-OwnedProcessClose -Process $clientProcess -Role "client"
    Wait-ForCleanProcessExit -Process $clientProcess -Role "client" -TimeoutSeconds $GracefulExitSeconds
    # Let the still-running host complete its real disconnect/ownership path.
    $dropDeadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $dropLog = Get-CombinedLogText -LogDirectory $hostLogDirectory
        if ($dropLog -match '(?im)drop peer slot=1 complete') { break }
        if ($hostProcess.HasExited) { break }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $dropDeadline)
    Request-OwnedProcessClose -Process $hostProcess -Role "host"
    Wait-ForCleanProcessExit -Process $hostProcess -Role "host" -TimeoutSeconds $GracefulExitSeconds

    $hostLog = Get-CombinedLogText -LogDirectory $hostLogDirectory
    $clientLog = Get-CombinedLogText -LogDirectory $clientLogDirectory

    $clientTransportPattern = if ($OnlineEndpoint) {
        '(?im)SMOKE_ONLINE_CHANNEL role=client adopted=1 transport=webrtc'
    } else { ('(?im)client TCP connected endpoint=127\.0\.0\.1:{0}\b' -f $Port) }
    if ($OnlineEndpoint -and ($hostLog + $clientLog) -match '(?im)host listening port=|client TCP connected endpoint=|connection adopted socket=') {
        throw "Online smoke unexpectedly used the TCP transport."
    }

    $hostPositivePatterns = @(
        @{ Label = "trace role/protocol"; Pattern = '(?im)role=host protocol=\d+ pid=\d+' },
        @{ Label = "listener"; Pattern = $hostReadyPattern },
        @{ Label = "authenticated client"; Pattern = ('(?im)peer authenticated slot=1 nick=' + [regex]::Escape($clientNickname) + '(?:\s|$)') },
        @{ Label = "deterministic game start"; Pattern = ('(?im)start game broadcast mode=\d+ level={0} players=2 seed={1}\b' -f $Level, $Seed) },
        @{ Label = "scenario start"; Pattern = ('(?im)\bSMOKE_START\b[^\r\n]*scenario={0}\b' -f ([regex]::Escape($scenarioMarker))) },
        @{ Label = "visible host PLAYING checkpoint"; Pattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=PLAYING[^\r\n]*size={1}x{2}\b' -f ([regex]::Escape($scenarioMarker)), $HostWindowWidth, $HostWindowHeight) },
        @{ Label = "host action applied"; Pattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=ACTION_APPLIED\b' -f ([regex]::Escape($scenarioMarker))) },
        @{ Label = "post-action TX hash"; Pattern = '(?im)\bSMOKE_POST_ACTION_TX\b[^\r\n]*\bhash=(?:0x)?[0-9a-f]{8,16}\b' }
    )
    $clientPositivePatterns = @(
        @{ Label = "trace role/protocol"; Pattern = '(?im)role=client protocol=\d+ pid=\d+' },
        @{ Label = "transport connection"; Pattern = $clientTransportPattern },
        @{ Label = "WELCOME"; Pattern = ('(?im)WELCOME assignedSlot=1 hostNick=' + [regex]::Escape($hostNickname) + '(?:\s|$)') },
        @{ Label = "profile sync"; Pattern = '(?im)PROFILE_SYNC applied\b' },
        @{ Label = "matching game start"; Pattern = ('(?im)client START_GAME decoded mode=\d+ level={0} seed={1}\b' -f $Level, $Seed) },
        @{ Label = "gameplay scene"; Pattern = '(?im)gameplay entered client=1\b' },
        @{ Label = "visible client PLAYING checkpoint"; Pattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=PLAYING[^\r\n]*size={1}x{2}\b' -f ([regex]::Escape($scenarioMarker)), $ClientWindowWidth, $ClientWindowHeight) },
        @{ Label = "client action applied"; Pattern = ('(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario={0}[^\r\n]*step=ACTION_APPLIED\b' -f ([regex]::Escape($scenarioMarker))) },
        @{ Label = "post-action applied hash"; Pattern = '(?im)\bSMOKE_POST_ACTION_APPLIED\b[^\r\n]*\bhash=(?:0x)?[0-9a-f]{8,16}\b' }
    )
    if ($OnlineEndpoint) {
        $hostPositivePatterns += @{ Label = "WebRTC game lifecycle"; Pattern = '(?im)SMOKE_ONLINE_CHANNEL role=host adopted=1 transport=webrtc' }
        $hostPositivePatterns += @{ Label = "native WebRTC channel adopted"; Pattern = '(?im)connection adopted transport=webrtc peer=' }
        $clientPositivePatterns += @{ Label = "native WebRTC channel adopted"; Pattern = '(?im)connection adopted transport=webrtc peer=' }
    }
    if ($VerifyUnicodeChat) {
        $unicodeChatPatterns = @(
            @{
                Label = "replicated UTF-8 nicknames"
                Pattern = '(?im)\bSMOKE_UTF8_NICKS\b[^\r\n]*\bvalid=1\b'
            },
            @{
                Label = "Enter chat send"
                Pattern = '(?im)\bSMOKE_CHAT_ENTER\b[^\r\n]*\butf8Bytes=\d+\b'
            },
            @{
                Label = "host-authoritative chat broadcast"
                Pattern = '(?im)\bSMOKE_CHAT_BROADCAST\b[^\r\n]*\bhost=1 client=1\b'
            },
            @{
                Label = "chat overlay visible"
                Pattern = '(?im)\bSMOKE_CHAT_OVERLAY\b[^\r\n]*\bphase=visible\b[^\r\n]*\balpha=230\b'
            },
            @{
                Label = "chat overlay fade"
                Pattern = '(?im)\bSMOKE_CHAT_OVERLAY\b[^\r\n]*\bphase=fade\b[^\r\n]*\balpha=(?:[1-9]|[1-9]\d|1\d\d|2[0-2]\d)\b'
            }
        )
        $hostPositivePatterns += $unicodeChatPatterns
        $clientPositivePatterns += $unicodeChatPatterns
    }
    if ($VerifyInteractions) {
        $interactionPatterns = @(
            @{ Label = "SDL middle-button ping cooldown"; Pattern = '(?im)\bSMOKE_INTERACTIONS_PING\b[^\r\n]*\baccepted=2\b[^\r\n]*\battempts=3\b[^\r\n]*\bcooldownMs=500\b[^\r\n]*\bauthority=received\b' },
            @{ Label = "SDL Control drawing lifecycle"; Pattern = '(?im)\bSMOKE_INTERACTIONS_STROKE\b[^\r\n]*\bpoints=(?:[4-9]|[1-9]\d+)\b[^\r\n]*\bbegin=1\b[^\r\n]*\bend=1\b' },
            @{ Label = "SDL Alt seed identity and cooldown chat"; Pattern = '(?im)\bSMOKE_INTERACTIONS_SEED\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bname=1\b[^\r\n]*\bcooldownText=1\b' },
            @{ Label = "drawing stays opaque before eight seconds"; Pattern = '(?im)\bSMOKE_INTERACTIONS_HOLD\b[^\r\n]*\bfirst=255\b[^\r\n]*\blast=255\b' },
            @{ Label = "drawing fades from end to start"; Pattern = '(?im)\bSMOKE_INTERACTIONS_FADE\b[^\r\n]*\bdirection=end_to_start\b' },
            @{ Label = "drawing fully clears"; Pattern = '(?im)\bSMOKE_INTERACTIONS_CLEAR\b[^\r\n]*\bfirst=0\b[^\r\n]*\blast=0\b' },
            @{ Label = "interaction workflow completed without planting"; Pattern = '(?im)\bSMOKE_INTERACTIONS_COMPLETE\b[^\r\n]*\bping=1\b[^\r\n]*\bstroke=1\b[^\r\n]*\bseed=1\b[^\r\n]*\bfade=1\b[^\r\n]*\bnoPlantMutation=1\b' }
        )
        $hostPositivePatterns += $interactionPatterns
        $clientPositivePatterns += $interactionPatterns
    }
    if ($VerifyPresentation) {
        foreach ($stage in @('normal', 'arm', 'head', 'jack_pop', 'jack_blast', 'snail', 'registry', 'avatar', 'retained')) {
            $bounds = if ($stage -eq 'registry') { 0 } else { 1 }
            $pattern = @{ Label = ('real presentation state and resource bounds: ' + $stage); Pattern = ('(?im)SMOKE_PRESENTATION_OBSERVED[^\r\n]*stage={0}\b[^\r\n]*assetBounds={1}' -f $stage, $bounds) }
            $hostPositivePatterns += $pattern
            $clientPositivePatterns += $pattern
        }
        $complete = @{ Label = 'presentation workflow complete'; Pattern = '(?im)SMOKE_PRESENTATION_COMPLETE[^\r\n]*limbs=2 jack=1 snail=1 clientAvatar=1 stableAppearance=1' }
        $hostPositivePatterns += $complete
        $clientPositivePatterns += $complete
    }
    if ($VerifySavedAvatars) {
        $savedPattern = @{ Label = 'natural union of personal avatars loaded before joining'; Pattern = '(?im)SMOKE_SAVED_AVATARS[^\r\n]*hostZombie=[1-9]\d* clientZombie=[1-9]\d*[^\r\n]*bytes=13[^\r\n]*source=natural_init' }
        $hostPositivePatterns += $savedPattern
        $clientPositivePatterns += $savedPattern
    }
    if ($VerifyModes -and $Scenario -eq "PvP1v1") {
        $hostPositivePatterns += @{ Label = "PvP server shovel authorization"; Pattern = '(?im)SMOKE_MODES_SHOVEL role=host sdl=blocked raw=blocked forgedCursor=blocked plantTool=working' }
        $clientPositivePatterns += @{ Label = "PvP zombie shovel UI and raw input rejected"; Pattern = '(?im)SMOKE_MODES_SHOVEL role=client sdl=blocked raw=blocked plantPreserved=1 zombieTool=0' }
        $clientPositivePatterns += @{ Label = "host-confirmed zombie hand and lane silhouette"; Pattern = '(?im)SMOKE_MODES_ZOMBIE_HELD role=client packetId=[1-9]\d* index=\d+ type=\d+ hostEcho=1 handVisible=1 previewVisible=1 row=2 col=6' }
        foreach ($role in @("host", "client")) {
            $goalPatterns = @(
                @{ Label = "PvP real99 nonterminal state"; Pattern = ('(?im)SMOKE_MODES_GOAL role={0} kills=99 award=0 fade=-1 complete=0 fixtureSeed=98 actualDeaths=1' -f $role) },
                @{ Label = "PvP real100 monotonic victory fade"; Pattern = ('(?im)SMOKE_MODES_GOAL role={0} kills=100 fadeEdge=1 counter=\d+ previous=\d+ fixtureSeed=98 actualDeaths=2' -f $role) }
            )
            if ($role -eq "host") { $hostPositivePatterns += $goalPatterns } else { $clientPositivePatterns += $goalPatterns }
            $captureRoot = if ($role -eq "host") { $hostRoot } else { $clientRoot }
            foreach ($frame in @('modes_goal99.bmp', 'modes_goal100.bmp')) {
                if (-not (Test-Path -LiteralPath (Join-Path $captureRoot $frame) -PathType Leaf)) {
                    throw ("PvP goal renderer evidence missing: {0}/{1}" -f $role, $frame)
                }
            }
        }
        foreach ($frame in @('modes_zombie_held.bmp', 'modes_zombie_hud.bmp')) {
            if (-not (Test-Path -LiteralPath (Join-Path $clientRoot $frame) -PathType Leaf)) {
                throw ("PvP zombie renderer evidence missing: {0}" -f $frame)
            }
        }
        if (-not (Test-Path -LiteralPath (Join-Path $hostRoot 'modes_plant_hud.bmp') -PathType Leaf)) {
            throw "PvP plant renderer evidence missing."
        }
    }
    if ($VerifyModes -and $Scenario -eq "TowerDefenseClient") {
        $hostPositivePatterns += @{ Label = "difficulty10 wave budget roster and stock HP"; Pattern = '(?im)SMOKE_MODES_DIFFICULTY role=host difficulty=10 baseWaves=[1-9]\d* waves=[1-9]\d* basePoints=\d+ points=\d+ zombies=[1-9]\d* garg=1 football=1 hpUnchanged=1' }
        $hostPositivePatterns += @{ Label = "departed active client's five cards redistributed without orphan ownership"; Pattern = '(?im)seed redistribution slot=1 packets=10 owners=10,0,0,0 orphaned=0' }
        $clientPositivePatterns += @{ Label = "difficulty10 client wave plan and roster"; Pattern = '(?im)SMOKE_MODES_DIFFICULTY role=client difficulty=10 baseWaves=[1-9]\d* waves=[1-9]\d* basePoints=\d+ points=\d+ zombies=[1-9]\d* garg=1 football=1 hpUnchanged=0' }
        $hostPositivePatterns += @{ Label = "remote semantic cursor mapped into a different local card index"; Pattern = '(?im)SMOKE_MODES_CURSOR role=host packetId=[1-9]\d* localIndex=[5-9] remoteIndex=0 mapped=1 pointInside=1 renderSettled=1' }
        $clientPositivePatterns += @{ Label = "client SDL motion over own first card"; Pattern = '(?im)SMOKE_MODES_CURSOR role=client packetId=[1-9]\d* localIndex=0 mapped=1 pointInside=1 renderSettled=1' }
        $hostCursor = [regex]::Match($hostLog, '(?im)SMOKE_MODES_CURSOR role=host packetId=(\d+)')
        $clientCursor = [regex]::Match($clientLog, '(?im)SMOKE_MODES_CURSOR role=client packetId=(\d+)')
        if (-not $hostCursor.Success -or -not $clientCursor.Success -or
            $hostCursor.Groups[1].Value -ne $clientCursor.Groups[1].Value) {
            throw "Relative cursor smoke did not observe the same stable packet ID on both layouts."
        }
    }
    if ($VerifyPresentation) {
        $appearance = '010B0F100D100F100B0E100D10'
        foreach ($stage in @('normal', 'arm', 'head', 'jack_pop', 'jack_blast', 'snail', 'avatar', 'retained')) {
            $pattern = '(?im)SMOKE_PRESENTATION_OBSERVED[^\r\n]*stage={0}\b[^\r\n]*id=(\d+)\b[^\r\n]*appearance=([A-F0-9]{{26}}) bytes=13' -f $stage
            $hostState = [regex]::Match($hostLog, $pattern)
            $clientState = [regex]::Match($clientLog, $pattern)
            if (-not $hostState.Success -or -not $clientState.Success -or
                $hostState.Groups[1].Value -ne $clientState.Groups[1].Value -or
                $hostState.Groups[2].Value -ne $clientState.Groups[2].Value -or
                (($stage -eq 'avatar' -or $stage -eq 'retained') -and $hostState.Groups[2].Value -ne $appearance)) {
                throw ('Presentation peer identity/13-byte appearance evidence differs at ' + $stage)
            }
        }
        if ($hostLog -notmatch 'SMOKE_PRESENTATION_OBSERVED[^\r\n]*stage=avatar\b[^\r\n]*attempts=(?:[1-9]|[1-8]\d|9[0-6])\b') {
            throw 'Native occasional client avatar selection exceeded the bounded spawn fixture.'
        }
        Add-Type -AssemblyName System.Drawing
        foreach ($roleRoot in @($hostRoot, $clientRoot)) {
            foreach ($stage in @('normal', 'arm', 'head', 'jack_pop', 'jack_blast', 'snail', 'registry', 'avatar', 'retained')) {
                $frame = Join-Path $roleRoot ('presentation_' + $stage + '.bmp')
                if (-not (Test-Path -LiteralPath $frame -PathType Leaf)) { throw ('Missing presentation frame: ' + $frame) }
                $bitmap = [Drawing.Bitmap]::new($frame)
                try {
                    $hasColor = $false
                    for ($x = 20; $x -lt $bitmap.Width -and -not $hasColor; $x += 80) {
                        for ($y = 20; $y -lt $bitmap.Height; $y += 80) {
                            $pixel = $bitmap.GetPixel($x, $y)
                            if ($pixel.R -or $pixel.G -or $pixel.B) { $hasColor = $true; break }
                        }
                    }
                    if (-not $hasColor) { throw ('Presentation frame is blank: ' + $frame) }
                }
                finally { $bitmap.Dispose() }
            }
        }
    }
    if ($VerifyFirstAdventure) {
        $firstAdventurePatterns = @(
            @{ Label = "fresh zero-progress profile"; Pattern = '(?im)\bSMOKE_FRESH_PROFILE\b[^\r\n]*\blevel=1 finished=0\b' },
            @{ Label = "stock opening demo plants and zombies"; Pattern = '(?im)\bSMOKE_FRESH_INTRO\b[^\r\n]*\bplants=[1-9]\d* zombies=[1-9]\d*\b[^\r\n]*\bcapture=1\b' },
            @{ Label = "moving sod roll"; Pattern = '(?im)\bSMOKE_FRESH_SOD\b[^\r\n]*\bactive=1 advanced=1\b' },
            @{ Label = "complete long UTF-8 tutorial label"; Pattern = '(?im)\bSMOKE_FRESH_LABEL\b[^\r\n]*\bbytes=[1-9]\d{2,} utf8=1 complete=1\b' },
            @{ Label = "loaded native GitHub menu icon"; Pattern = '(?im)\bSMOKE_FRESH_GITHUB\b[^\r\n]*\bloaded=1\b' },
            @{ Label = "native CO-OP options below controls"; Pattern = '(?im)\bSMOKE_FRESH_OPTIONS\b[^\r\n]*\bbelow=1 bounds=1\b' },
            @{ Label = "animated aligned unclipped sunflower preview"; Pattern = '(?im)\bSMOKE_FRESH_PREVIEW\b[^\r\n]*\bsample=2\b[^\r\n]*\baligned=1 unclipped=1 advanced=1\b' },
            @{ Label = "native online dialog frame contains its controls"; Pattern = '(?im)\bSMOKE_FRESH_ONLINE\b[^\r\n]*\bframeFit=1\b[^\r\n]*\bdirectoryConnected=0\b' }
        )
        foreach ($firstStep in @("flash", "baseline_flash_off", "award_idle_rays", "award_pickup_rays",
            "award_sunflower", "next_level_clean", "menu_unlocks", "profile_switched", "challenge_open",
            "challenge_closed", "store_open", "store_page", "store_closed", "profile_restored",
            "options_coop", "settings_sunflower_a", "settings_sunflower_b", "settings_closed",
            "options_closed", "online_browser", "online_closed", "complete")) {
            $firstAdventurePatterns += @{
                Label = ("fresh adventure " + $firstStep)
                Pattern = ('(?im)\bSMOKE_FRESH_CHECK\b[^\r\n]*\bstep=' + [regex]::Escape($firstStep) + '\b[^\r\n]*\bcapture=1\b')
            }
        }
        $hostPositivePatterns += $firstAdventurePatterns
        $clientPositivePatterns += $firstAdventurePatterns
    }
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
        if ($Scenario -eq "Adventure5Bowling" -and $SkipIntro -eq 0) {
            $hostPositivePatterns += @{
                Label = "host stock shovel pickup"
                Pattern = '(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*\bstep=INTRO_SHOVEL_PICKUP\b'
            }
            $clientPositivePatterns += @{
                Label = "client rendered tinted/outlined remote stock shovel"
                Pattern = '(?im)\bSMOKE_REMOTE_CURSOR_RENDER\b[^\r\n]*\brole=client\b[^\r\n]*\bslot=0\b[^\r\n]*\btype=6\b[^\r\n]*\bstock=1\b[^\r\n]*\btint=(?:25[0-5]|2[0-4]\d|1?\d?\d),(?:25[0-5]|2[0-4]\d|1?\d?\d),(?:25[0-5]|2[0-4]\d|1?\d?\d)\b[^\r\n]*\boutline=1\b[^\r\n]*\bfallback=0\b'
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
            },
            @{
                Label = "host rendered tinted/outlined remote stock hammer"
                Pattern = '(?im)\bSMOKE_REMOTE_CURSOR_RENDER\b[^\r\n]*\brole=host\b[^\r\n]*\bslot=1\b[^\r\n]*\btype=7\b[^\r\n]*\bstock=1\b[^\r\n]*\btint=(?:25[0-5]|2[0-4]\d|1?\d?\d),(?:25[0-5]|2[0-4]\d|1?\d?\d),(?:25[0-5]|2[0-4]\d|1?\d?\d)\b[^\r\n]*\boutline=1\b[^\r\n]*\bfallback=0\b'
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
            },
            @{
                Label = "client rendered tinted/outlined remote stock hammer"
                Pattern = '(?im)\bSMOKE_REMOTE_CURSOR_RENDER\b[^\r\n]*\brole=client\b[^\r\n]*\bslot=0\b[^\r\n]*\btype=7\b[^\r\n]*\bstock=1\b[^\r\n]*\btint=(?:25[0-5]|2[0-4]\d|1?\d?\d),(?:25[0-5]|2[0-4]\d|1?\d?\d),(?:25[0-5]|2[0-4]\d|1?\d?\d)\b[^\r\n]*\boutline=1\b[^\r\n]*\bfallback=0\b'
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
    if ($Scenario -eq "TowerDefenseClient") {
        $positiveDecimal = '(?:[1-9]\d*\.[0-9][0-9]|0\.(?!00)[0-9][0-9])'
        $hostPositivePatterns += @(
            @{ Label = "host Tower Defense path"; Pattern = '(?im)\bSMOKE_TD_PATH\b[^\r\n]*\brole=host\b[^\r\n]*\bbytes=[1-9]\d*\b[^\r\n]*\bhash=[0-9a-f]{8}\b[^\r\n]*\bedgeVisits=3\b[^\r\n]*\bedgePattern=(?:TBT|BTB)\b[^\r\n]*\bseparated=1\b[^\r\n]*\bno2x2=1\b[^\r\n]*\bdirt=[1-9]\d*\b[^\r\n]*\bgrass=1\b[^\r\n]*\bimage=[1-9]\d*x[1-9]\d*\b' },
            @{ Label = "host Tower Defense road draw"; Pattern = '(?im)\bSMOKE_TD_ROAD_RENDER_COMPLETE\b[^\r\n]*\brole=host\b[^\r\n]*\bboard=(?!0+\b)[0-9a-f]+\b[^\r\n]*\bcells=[1-9]\d*\b[^\r\n]*\bimage=[1-9]\d*x[1-9]\d*\b' },
            @{ Label = "host Tower Defense off-lawn zombie entry"; Pattern = ('(?im)\bSMOKE_TD_ZOMBIE_ENTRY\b[^\r\n]*\brole=host\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bghost=0\b[^\r\n]*\bx={0}\b[^\r\n]*\brow=[0-5]\b[^\r\n]*\banimRate={0}\b' -f $positiveDecimal) },
            @{ Label = "host Tower Defense zombie turn"; Pattern = ('(?im)\bSMOKE_TD_ZOMBIE_TURN\b[^\r\n]*\brole=host\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\brow=[0-5]\b[^\r\n]*\bmoved={0}\b[^\r\n]*\banimRate={0}\b[^\r\n]*\bpathPos={0}\b' -f $positiveDecimal) },
            @{ Label = "host observed Tower Defense client plant"; Pattern = '(?im)\bSMOKE_TD_CLIENT_PLANT\b[^\r\n]*\brole=host\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bcell=\([0-8],[0-5]\)[^\r\n]*\broad=0\b' }
        )
        $clientPositivePatterns += @(
            @{ Label = "client Tower Defense path"; Pattern = '(?im)\bSMOKE_TD_PATH\b[^\r\n]*\brole=client\b[^\r\n]*\bbytes=[1-9]\d*\b[^\r\n]*\bhash=[0-9a-f]{8}\b[^\r\n]*\bedgeVisits=3\b[^\r\n]*\bedgePattern=(?:TBT|BTB)\b[^\r\n]*\bseparated=1\b[^\r\n]*\bno2x2=1\b[^\r\n]*\bdirt=[1-9]\d*\b[^\r\n]*\bgrass=1\b[^\r\n]*\bimage=[1-9]\d*x[1-9]\d*\b' },
            @{ Label = "client Tower Defense road draw"; Pattern = '(?im)\bSMOKE_TD_ROAD_RENDER_COMPLETE\b[^\r\n]*\brole=client\b[^\r\n]*\bboard=(?!0+\b)[0-9a-f]+\b[^\r\n]*\bcells=[1-9]\d*\b[^\r\n]*\bimage=[1-9]\d*x[1-9]\d*\b' },
            @{ Label = "client Tower Defense off-lawn zombie entry"; Pattern = ('(?im)\bSMOKE_TD_ZOMBIE_ENTRY\b[^\r\n]*\brole=client\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bghost=1\b[^\r\n]*\bx={0}\b[^\r\n]*\brow=[0-5]\b[^\r\n]*\banimRate={0}\b' -f $positiveDecimal) },
            @{ Label = "client Tower Defense zombie turn"; Pattern = ('(?im)\bSMOKE_TD_ZOMBIE_TURN\b[^\r\n]*\brole=client\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\brow=[0-5]\b[^\r\n]*\bmoved={0}\b[^\r\n]*\banimRate={0}\b[^\r\n]*\bpathPos=(?:0\.00|{0})\b' -f $positiveDecimal) },
            @{ Label = "client Tower Defense plant"; Pattern = '(?im)\bSMOKE_TD_CLIENT_PLANT\b[^\r\n]*\brole=client\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bcell=\([0-8],[0-5]\)[^\r\n]*\broad=0\b' }
        )
    }
    if ($Scenario -eq "PvP1v1") {
        $pvpDeckPattern = '(?im)\bSMOKE_PVP_DECK\b[^\r\n]*\btypes=([^\s]+)\s+owners=([^\s]+)'
        $hostPvpDeck = [regex]::Match($hostLog, $pvpDeckPattern)
        $clientPvpDeck = [regex]::Match($clientLog, $pvpDeckPattern)
        if (-not $hostPvpDeck.Success -or -not $clientPvpDeck.Success -or
            $hostPvpDeck.Groups[1].Value -ne $clientPvpDeck.Groups[1].Value -or
            $hostPvpDeck.Groups[2].Value -ne $clientPvpDeck.Groups[2].Value) {
            throw "PvP card type/order/owner manifests are missing or differ between peers."
        }

        $pvpZombiePattern = '(?im)\bSMOKE_PVP_ZOMBIE_ENTRY\b[^\r\n]*\bid=([1-9]\d*)\b[^\r\n]*\bx=(-?\d+\.\d{2})\b[^\r\n]*\brow=([0-5])\b'
        $hostPvpZombie = [regex]::Match($hostLog, $pvpZombiePattern)
        $clientPvpZombie = [regex]::Match($clientLog, $pvpZombiePattern)
        if (-not $hostPvpZombie.Success -or -not $clientPvpZombie.Success -or
            $hostPvpZombie.Groups[1].Value -ne $clientPvpZombie.Groups[1].Value -or
            $hostPvpZombie.Groups[3].Value -ne $clientPvpZombie.Groups[3].Value -or
            [double]$hostPvpZombie.Groups[2].Value -le 800.0 -or
            [double]$clientPvpZombie.Groups[2].Value -le 800.0) {
            throw "PvP client-owned zombie identity/row/offscreen entry did not correlate."
        }

        $pvpPassivePattern = '(?im)\bSMOKE_PVP_BRAINS\b[^\r\n]*\bphase=passive\b[^\r\n]*\bbefore=(\d+)\b[^\r\n]*\bafter=(\d+)\b[^\r\n]*\bdelta=(\d+)\b'
        $hostPvpPassive = [regex]::Match($hostLog, $pvpPassivePattern)
        $clientPvpPassive = [regex]::Match($clientLog, $pvpPassivePattern)
        if (-not $hostPvpPassive.Success -or -not $clientPvpPassive.Success -or
            $hostPvpPassive.Groups[2].Value -ne $clientPvpPassive.Groups[2].Value -or
            [int]$hostPvpPassive.Groups[3].Value -ne 50 -or
            [int]$clientPvpPassive.Groups[3].Value -ne 50) {
            throw "PvP passive brain award was missing, not exactly +50, or not synchronized."
        }
    }

    if ($Scenario -eq "PortalCombatClient") {
        $hostPositivePatterns += @(
            @{ Label = "host Portal Combat layout"; Pattern = '(?im)\bSMOKE_PORTAL_LAYOUT\b[^\r\n]*\brole=host\b[^\r\n]*\bcount=4\b[^\r\n]*\bcircles=2\b[^\r\n]*\bsquares=2\b[^\r\n]*\bresources=4\b[^\r\n]*\btracks=[1-9]\d*\b' },
            @{ Label = "host visible Portal conveyor"; Pattern = '(?im)\bSMOKE_PORTAL_CONVEYOR\b[^\r\n]*\brole=host\b[^\r\n]*\bcount=[1-9]\d*\b[^\r\n]*\bcounter=(\d+)\b[^\r\n]*\binitial=(?!\1\b)\d+\b[^\r\n]*\bimages=[1-9]\d*x[1-9]\d*,[1-9]\d*x[1-9]\d*\b' },
            @{ Label = "host Portal Combat stock zombie entry"; Pattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_ENTRY\b[^\r\n]*\brole=host\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bghost=0\b[^\r\n]*\banimRate=(?!0\.00\b)-?\d+\.\d{2}\b' },
            @{ Label = "host Portal Combat teleport"; Pattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_TELEPORT\b[^\r\n]*\brole=host\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bsnap=1\b[^\r\n]*\brender=1\b[^\r\n]*\bdestination=1\b[^\r\n]*\banimRate=(?!0\.00\b)-?\d+\.\d{2}\b' },
            @{ Label = "host observed Portal client plant"; Pattern = '(?im)\bSMOKE_PORTAL_CLIENT_PLANT\b[^\r\n]*\brole=host\b[^\r\n]*\bid=[1-9]\d*\b' },
            @{ Label = "host Portal relocation"; Pattern = '(?im)\bSMOKE_PORTAL_RELOCATION\b[^\r\n]*\brole=host\b[^\r\n]*\blive=4\b[^\r\n]*\bopen=4\b[^\r\n]*\bpairs=2\b' },
            @{ Label = "host Portal advice show"; Pattern = '(?im)\bSMOKE_PORTAL_ADVICE\b[^\r\n]*\brole=host\b[^\r\n]*\bphase=show\b' },
            @{ Label = "host Portal advice clear"; Pattern = '(?im)\bSMOKE_PORTAL_ADVICE\b[^\r\n]*\brole=host\b[^\r\n]*\bphase=clear\b' },
            @{ Label = "host Portal advice TX show"; Pattern = '(?im)\badvice TX op=show\b' },
            @{ Label = "host Portal advice TX clear"; Pattern = '(?im)\badvice TX op=clear\b' }
        )
        $clientPositivePatterns += @(
            @{ Label = "client Portal Combat layout"; Pattern = '(?im)\bSMOKE_PORTAL_LAYOUT\b[^\r\n]*\brole=client\b[^\r\n]*\bcount=4\b[^\r\n]*\bcircles=2\b[^\r\n]*\bsquares=2\b[^\r\n]*\bresources=4\b[^\r\n]*\btracks=[1-9]\d*\b' },
            @{ Label = "client visible Portal conveyor"; Pattern = '(?im)\bSMOKE_PORTAL_CONVEYOR\b[^\r\n]*\brole=client\b[^\r\n]*\bcount=[1-9]\d*\b[^\r\n]*\bcounter=(\d+)\b[^\r\n]*\binitial=(?!\1\b)\d+\b[^\r\n]*\bimages=[1-9]\d*x[1-9]\d*,[1-9]\d*x[1-9]\d*\b' },
            @{ Label = "client Portal Combat stock zombie entry"; Pattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_ENTRY\b[^\r\n]*\brole=client\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bghost=1\b[^\r\n]*\banimRate=(?!0\.00\b)-?\d+\.\d{2}\b' },
            @{ Label = "client Portal Combat teleport snap"; Pattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_TELEPORT\b[^\r\n]*\brole=client\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bsnap=1\b[^\r\n]*\brender=1\b[^\r\n]*\bdestination=1\b[^\r\n]*\banimRate=(?!0\.00\b)-?\d+\.\d{2}\b' },
            @{ Label = "client Portal plant"; Pattern = '(?im)\bSMOKE_PORTAL_CLIENT_PLANT\b[^\r\n]*\brole=client\b[^\r\n]*\bid=[1-9]\d*\b' },
            @{ Label = "client held conveyor packet echo"; Pattern = '(?im)\bcursor held packet echo\b[^\r\n]*\bslot=1\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bindex=\d+\b' },
            @{ Label = "client Portal relocation"; Pattern = '(?im)\bSMOKE_PORTAL_RELOCATION\b[^\r\n]*\brole=client\b[^\r\n]*\blive=4\b[^\r\n]*\bopen=4\b[^\r\n]*\bpairs=2\b' },
            @{ Label = "client Portal advice show"; Pattern = '(?im)\bSMOKE_PORTAL_ADVICE\b[^\r\n]*\brole=client\b[^\r\n]*\bphase=show\b' },
            @{ Label = "client Portal advice clear"; Pattern = '(?im)\bSMOKE_PORTAL_ADVICE\b[^\r\n]*\brole=client\b[^\r\n]*\bphase=clear\b' },
            @{ Label = "client Portal advice applied show"; Pattern = '(?im)\badvice applied op=0\b' },
            @{ Label = "client Portal advice applied clear"; Pattern = '(?im)\badvice applied op=1\b' }
        )
    }
    if ($Scenario -eq "PvP1v1") {
        $hostPositivePatterns += @(
            @{ Label = "host PvP 1v1 roster"; Pattern = '(?im)\bSMOKE_PVP_ROSTER\b[^\r\n]*\brole=host\b[^\r\n]*\bplayers=2\b[^\r\n]*\bhostTeam=plants\b[^\r\n]*\bclientTeam=zombies\b[^\r\n]*\blocalSlot=0\b[^\r\n]*\blocalTeam=plants\b' },
            @{ Label = "host PvP deck"; Pattern = '(?im)\bSMOKE_PVP_DECK\b[^\r\n]*\brole=host\b[^\r\n]*\bplants=10\b[^\r\n]*\bzombies=16\b[^\r\n]*\bshared=16\b[^\r\n]*\btotal=26\b' },
            @{ Label = "host PvP initial brains"; Pattern = '(?im)\bSMOKE_PVP_BRAINS\b[^\r\n]*\brole=host\b[^\r\n]*\bphase=initial\b[^\r\n]*\bbrains=100\b' },
            @{ Label = "host PvP offscreen client zombie"; Pattern = '(?im)\bSMOKE_PVP_ZOMBIE_ENTRY\b[^\r\n]*\brole=host\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bx=(?:8\d\d|9\d\d|[1-9]\d{3,})\.\d{2}\b[^\r\n]*\bghost=0\b' },
            @{ Label = "host PvP plant action"; Pattern = '(?im)\bSMOKE_PVP_HOST_PLANT\b[^\r\n]*\brole=host\b' },
            @{ Label = "host PvP passive brains"; Pattern = '(?im)\bSMOKE_PVP_BRAINS\b[^\r\n]*\brole=host\b[^\r\n]*\bphase=passive\b[^\r\n]*\bdelta=50\b' },
            @{ Label = "host PvP remote cursor hidden"; Pattern = '(?im)\bSMOKE_PVP_CURSOR_HIDDEN\b[^\r\n]*\brole=host\b[^\r\n]*\boverlaySuppressed=1\b' }
        )
        $clientPositivePatterns += @(
            @{ Label = "client PvP 1v1 roster"; Pattern = '(?im)\bSMOKE_PVP_ROSTER\b[^\r\n]*\brole=client\b[^\r\n]*\bplayers=2\b[^\r\n]*\blocalSlot=1\b[^\r\n]*\blocalTeam=zombies\b' },
            @{ Label = "client PvP deck"; Pattern = '(?im)\bSMOKE_PVP_DECK\b[^\r\n]*\brole=client\b[^\r\n]*\bplants=10\b[^\r\n]*\bzombies=16\b[^\r\n]*\bshared=16\b[^\r\n]*\btotal=26\b' },
            @{ Label = "client PvP initial brains"; Pattern = '(?im)\bSMOKE_PVP_BRAINS\b[^\r\n]*\brole=client\b[^\r\n]*\bphase=initial\b[^\r\n]*\bbrains=100\b' },
            @{ Label = "client PvP zombie input"; Pattern = '(?im)\bSMOKE_PVP_CLIENT_ZOMBIE_INPUT\b[^\r\n]*\bindex=10\b[^\r\n]*\bowner=255\b[^\r\n]*\bbrains=100\b' },
            @{ Label = "client PvP offscreen zombie ghost"; Pattern = '(?im)\bSMOKE_PVP_ZOMBIE_ENTRY\b[^\r\n]*\brole=client\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bx=(?:8\d\d|9\d\d|[1-9]\d{3,})\.\d{2}\b[^\r\n]*\bghost=1\b' },
            @{ Label = "client sees host PvP plant"; Pattern = '(?im)\bSMOKE_PVP_HOST_PLANT\b[^\r\n]*\brole=client\b' },
            @{ Label = "client PvP passive brains"; Pattern = '(?im)\bSMOKE_PVP_BRAINS\b[^\r\n]*\brole=client\b[^\r\n]*\bphase=passive\b[^\r\n]*\bdelta=50\b' },
            @{ Label = "client PvP remote cursor hidden"; Pattern = '(?im)\bSMOKE_PVP_CURSOR_HIDDEN\b[^\r\n]*\brole=client\b[^\r\n]*\boverlaySuppressed=1\b' }
        )
    }
    if ($Scenario -eq "EndlessST") {
        $hostPositivePatterns += @(
            @{ Label = "host EndlessST combined mode"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_MODE\b[^\r\n]*\brole=host\b[^\r\n]*\bendless=1\b[^\r\n]*\btimed=1\b[^\r\n]*\brepick=1\b[^\r\n]*\bwavesPerStage=10\b[^\r\n]*\bauthority=host\b' },
            @{ Label = "host EndlessST visible time/weather transition"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_TIME\b[^\r\n]*\brole=host\b[^\r\n]*\bfrom=[01]\b[^\r\n]*\bto=[01]\b[^\r\n]*\bvisible=1\b' },
            @{ Label = "host EndlessST stock wave progression"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_WAVE\b[^\r\n]*\brole=host\b[^\r\n]*\bfrom=-?\d+\b[^\r\n]*\bto=\d+\b[^\r\n]*\bstockSpawner=1\b' },
            @{ Label = "host EndlessST ten-wave repick opened"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_REPICK_OPEN\b[^\r\n]*\brole=host\b[^\r\n]*\bstage=[1-9]\d*\b[^\r\n]*\bseedChoosing=1\b[^\r\n]*\bchooser=(?!0+\b)[0-9a-f]+\b[^\r\n]*\bwavesPerStage=10\b' },
            @{ Label = "host EndlessST repick completed"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_REPICK_COMPLETE\b[^\r\n]*\brole=host\b[^\r\n]*\bfromStage=0\b[^\r\n]*\btoStage=[1-9]\d*\b[^\r\n]*\bchooser=1\b[^\r\n]*\bwavesPerStage=10\b' },
            @{ Label = "host EndlessST presentation thawed"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_THAW\b[^\r\n]*\brole=host\b[^\r\n]*\bthawed=1\b[^\r\n]*\breanim=(?!0+\b)[0-9a-f]+\b[^\r\n]*\brate=(?!0\.00\b)\d+\.\d+\b[^\r\n]*\bfrom=\d+\.\d+\b[^\r\n]*\bto=\d+\.\d+\b[^\r\n]*\bmusicPaused=0\b' },
            @{ Label = "host EndlessST health scaling"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_HEALTH_SCALE\b[^\r\n]*\brole=host\b[^\r\n]*\bstage=[1-9]\d*\b[^\r\n]*\bbase=[1-9]\d*\b[^\r\n]*\bscaled=[1-9]\d*\b[^\r\n]*\bexpected=[1-9]\d*\b[^\r\n]*\bfactor=1\.12\b' }
        )
        $clientPositivePatterns += @(
            @{ Label = "client EndlessST combined snapshot mode"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_MODE\b[^\r\n]*\brole=client\b[^\r\n]*\bendless=1\b[^\r\n]*\btimed=1\b[^\r\n]*\brepick=1\b[^\r\n]*\bwavesPerStage=10\b[^\r\n]*\bauthority=snapshot\b' },
            @{ Label = "client EndlessST visible time/weather transition"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_TIME\b[^\r\n]*\brole=client\b[^\r\n]*\bfrom=[01]\b[^\r\n]*\bto=[01]\b[^\r\n]*\bvisible=1\b' },
            @{ Label = "client EndlessST synchronized wave progression"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_WAVE\b[^\r\n]*\brole=client\b[^\r\n]*\bfrom=-?\d+\b[^\r\n]*\bto=\d+\b[^\r\n]*\bstockSpawner=1\b' },
            @{ Label = "client EndlessST synchronized repick opened"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_REPICK_OPEN\b[^\r\n]*\brole=client\b[^\r\n]*\bstage=[1-9]\d*\b[^\r\n]*\bseedChoosing=1\b[^\r\n]*\bchooser=(?!0+\b)[0-9a-f]+\b[^\r\n]*\bwavesPerStage=10\b' },
            @{ Label = "client EndlessST repick completed"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_REPICK_COMPLETE\b[^\r\n]*\brole=client\b[^\r\n]*\bfromStage=0\b[^\r\n]*\btoStage=[1-9]\d*\b[^\r\n]*\bchooser=1\b[^\r\n]*\bwavesPerStage=10\b' },
            @{ Label = "client EndlessST presentation thawed"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_THAW\b[^\r\n]*\brole=client\b[^\r\n]*\bthawed=1\b[^\r\n]*\breanim=(?!0+\b)[0-9a-f]+\b[^\r\n]*\brate=(?!0\.00\b)\d+\.\d+\b[^\r\n]*\bfrom=\d+\.\d+\b[^\r\n]*\bto=\d+\.\d+\b[^\r\n]*\bmusicPaused=0\b' },
            @{ Label = "client EndlessST health scaling"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_HEALTH_SCALE\b[^\r\n]*\brole=client\b[^\r\n]*\bstage=[1-9]\d*\b[^\r\n]*\bbase=[1-9]\d*\b[^\r\n]*\bscaled=[1-9]\d*\b[^\r\n]*\bexpected=[1-9]\d*\b[^\r\n]*\bfactor=1\.12\b' }
        )
    }
    if ($Scenario -eq "EndlessOverflow") {
        $hostPositivePatterns += @(
            @{ Label = "host EndlessOverflow mode"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_MODE\b[^\r\n]*\brole=host\b[^\r\n]*\bendless=1\b[^\r\n]*\btimed=1\b[^\r\n]*\brepick=1\b[^\r\n]*\bwavesPerStage=10\b[^\r\n]*\bauthority=host\b' },
            @{ Label = "host exact stage 255"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_STAGE\b[^\r\n]*\brole=host\b[^\r\n]*\bphase=pre\b[^\r\n]*\bstage=255\b[^\r\n]*\bnoWrap=1\b' },
            @{ Label = "host oversized red-eye health"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_HEALTH\b[^\r\n]*\brole=host\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bstage=255\b[^\r\n]*\bbody=\d+\b[^\r\n]*\bmax=\d+\b[^\r\n]*\bghost=0\b[^\r\n]*\bover16=1\b' },
            @{ Label = "host stock stage trigger"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_STAGE_TRIGGER\b[^\r\n]*\brole=host\b[^\r\n]*\bfrom=255\b[^\r\n]*\bstockCheckForGameEnd=1\b' },
            @{ Label = "host stage 256 repick"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_REPICK_OPEN\b[^\r\n]*\brole=host\b[^\r\n]*\bstage=256\b[^\r\n]*\bseedChoosing=1\b' },
            @{ Label = "host exact stage 256"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_STAGE\b[^\r\n]*\brole=host\b[^\r\n]*\bphase=post\b[^\r\n]*\bstage=256\b[^\r\n]*\bnoWrap=1\b' },
            @{ Label = "host second chooser complete"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_REPICK_COMPLETE\b[^\r\n]*\brole=host\b[^\r\n]*\bfrom=255\b[^\r\n]*\bto=256\b[^\r\n]*\bchooser=1\b' }
        )
        $clientPositivePatterns += @(
            @{ Label = "client EndlessOverflow mode"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_MODE\b[^\r\n]*\brole=client\b[^\r\n]*\bendless=1\b[^\r\n]*\btimed=1\b[^\r\n]*\brepick=1\b[^\r\n]*\bwavesPerStage=10\b[^\r\n]*\bauthority=snapshot\b' },
            @{ Label = "client snapshot stage 255"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_STAGE\b[^\r\n]*\brole=client\b[^\r\n]*\bphase=pre\b[^\r\n]*\bstage=255\b[^\r\n]*\bauthority=snapshot\b[^\r\n]*\bnoWrap=1\b' },
            @{ Label = "client oversized red-eye health"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_HEALTH\b[^\r\n]*\brole=client\b[^\r\n]*\bid=[1-9]\d*\b[^\r\n]*\bstage=255\b[^\r\n]*\bbody=\d+\b[^\r\n]*\bmax=\d+\b[^\r\n]*\bghost=1\b[^\r\n]*\bover16=1\b' },
            @{ Label = "client stage 256 repick"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_REPICK_OPEN\b[^\r\n]*\brole=client\b[^\r\n]*\bstage=256\b[^\r\n]*\bseedChoosing=1\b' },
            @{ Label = "client snapshot stage 256"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_STAGE\b[^\r\n]*\brole=client\b[^\r\n]*\bphase=post\b[^\r\n]*\bstage=256\b[^\r\n]*\bauthority=snapshot\b[^\r\n]*\bnoWrap=1\b' },
            @{ Label = "client second chooser complete"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_REPICK_COMPLETE\b[^\r\n]*\brole=client\b[^\r\n]*\bfrom=255\b[^\r\n]*\bto=256\b[^\r\n]*\bchooser=1\b' }
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
        @{ Label = "on-lawn zombie spawn"; Pattern = '(?im)\bSMOKE_ZOMBIE_SPAWN_INVALID\b' },
        @{ Label = "rejected ready-set-plant"; Pattern = '(?im)\bREADY_SET_PLANT rejected\b' },
        @{ Label = "dropped ready-set-plant"; Pattern = '(?im)\bboard event dropped outside gameplay type=24\b' }
    )
    if ($Scenario -eq "TowerDefenseClient") {
        $negativePatterns += @(
            @{ Label = "invalid Tower Defense path topology"; Pattern = '(?im)\bSMOKE_TD_PATH\b[^\r\n]*(?:\bbytes=0\b|\bhash=00000000\b|\bedgeVisits=(?!3\b)\d+|\bedgePattern=(?!(?:TBT|BTB)\b)\S+|\bseparated=(?!1\b)\d+|\bno2x2=(?!1\b)\d+|\bdirt=0\b|\bgrass=(?!1\b)\d+)' },
            @{ Label = "empty Tower Defense road draw"; Pattern = '(?im)\bSMOKE_TD_ROAD_RENDER_COMPLETE\b[^\r\n]*(?:\bboard=0+\b|\bcells=0\b)' },
            @{ Label = "invalid Tower Defense zombie entry"; Pattern = '(?im)(?:\bSMOKE_TD_ZOMBIE_ENTRY_INVALID\b|\bSMOKE_TD_ZOMBIE_ENTRY\b[^\r\n]*\banimRate=(?:-\d+\.\d{2}|0\.00)\b)' },
            @{ Label = "invalid Tower Defense zombie turn"; Pattern = '(?im)\bSMOKE_TD_ZOMBIE_TURN\b[^\r\n]*(?:\bmoved=(?:-\d+\.\d{2}|0\.00)\b|\banimRate=(?:-\d+\.\d{2}|0\.00)\b|\bpathPos=-\d+\.\d{2}\b)' },
            @{ Label = "Tower Defense plant placed on road"; Pattern = '(?im)\bSMOKE_TD_CLIENT_PLANT\b[^\r\n]*\broad=(?!0\b)\d+' }
        )
    }
    if ($Scenario -eq "PortalCombatClient") {
        $negativePatterns += @(
            @{ Label = "invalid Portal layout"; Pattern = '(?im)\bSMOKE_PORTAL_LAYOUT_INVALID\b' },
            @{ Label = "invalid Portal conveyor"; Pattern = '(?im)\bSMOKE_PORTAL_CONVEYOR_INVALID\b' },
            @{ Label = "invalid Portal resource"; Pattern = '(?im)\bSMOKE_PORTAL_RESOURCE_INVALID\b' },
            @{ Label = "invalid Portal zombie entry"; Pattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_ENTRY_INVALID\b' },
            @{ Label = "invalid Portal teleport"; Pattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_TELEPORT_INVALID\b' },
            @{ Label = "invalid Portal camera sync"; Pattern = '(?im)\bSMOKE_CAMERA_SYNC_INVALID\b' },
            @{ Label = "interpolated Portal teleport"; Pattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_TELEPORT\b[^\r\n]*\bsnap=(?!1\b)\d+' },
            @{ Label = "invalid Portal relocation"; Pattern = '(?im)\bSMOKE_PORTAL_RELOCATION\b[^\r\n]*(?:\blive=(?!4\b)\d+|\bopen=(?!4\b)\d+|\bpairs=(?!2\b)\d+)' },
            @{ Label = "rejected Portal advice"; Pattern = '(?im)\badvice (?:TX|applied)[^\r\n]*\brejected\b' }
        )
    }
    if ($Scenario -eq "PvP1v1") {
        $negativePatterns += @(
            @{ Label = "invalid PvP deck"; Pattern = '(?im)\bSMOKE_PVP_DECK_INVALID\b' },
            @{ Label = "invalid PvP brains"; Pattern = '(?im)\bSMOKE_PVP_BRAINS_INVALID\b' },
            @{ Label = "on-lawn PvP zombie entry"; Pattern = '(?im)\bSMOKE_PVP_ZOMBIE_ENTRY_INVALID\b' },
            @{ Label = "remote cursor rendered during PvP"; Pattern = '(?im)\bSMOKE_REMOTE_CURSOR_RENDER\b' }
        )
    }
    if ($Scenario -eq "EndlessST") {
        $negativePatterns += @(
            @{ Label = "invalid EndlessST combined mode"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_MODE\b[^\r\n]*(?:\bendless=(?!1\b)\d+|\btimed=(?!1\b)\d+|\brepick=(?!1\b)\d+|\bwavesPerStage=(?!10\b)\d+)' },
            @{ Label = "invalid EndlessST repick"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_REPICK_(?:OPEN|COMPLETE)\b[^\r\n]*(?:\bseedChoosing=(?!1\b)\d+|\bchooser=0+\b|\bwavesPerStage=(?!10\b)\d+)' },
            @{ Label = "invalid EndlessST thaw"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_THAW\b[^\r\n]*(?:\bthawed=(?!1\b)\d+|\breanim=0+\b|\brate=0\.00\b|\bmusicPaused=(?!0\b)\d+)' },
            @{ Label = "invalid EndlessST health scale"; Pattern = '(?im)\bSMOKE_ENDLESS_ST_HEALTH_SCALE\b[^\r\n]*(?:\bbase=0\b|\bscaled=0\b|\bexpected=0\b|\bfactor=(?!1\.12\b)\S+)' }
        )
    }
    if ($Scenario -eq "EndlessOverflow") {
        $negativePatterns += @(
            @{ Label = "invalid EndlessOverflow stage"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_STAGE_INVALID\b' },
            @{ Label = "clipped EndlessOverflow health"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_HEALTH_INVALID\b' },
            @{ Label = "wrapped EndlessOverflow stage"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_(?:STAGE|REPICK_COMPLETE)\b[^\r\n]*(?:\bstage=0\b|\bto=0\b|\bwrap=1\b)' },
            @{ Label = "16-bit ceiling health"; Pattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_HEALTH\b[^\r\n]*(?:\bbody=65535\b|\bmax=65535\b)' }
        )
    }

    Assert-PositivePatterns -Role "host" -Text $hostLog -Patterns $hostPositivePatterns
    Assert-PositivePatterns -Role "client" -Text $clientLog -Patterns $clientPositivePatterns
    if ($VerifySavedAvatars) {
        $pattern = 'SMOKE_SAVED_AVATARS role={0} hostZombie=(\d+) clientZombie=(\d+) hostAppearance=([A-F0-9]{{26}}) clientAppearance=([A-F0-9]{{26}}) bytes=13 attempts=(\d+) source=natural_init'
        $hostUnion = [regex]::Match($hostLog, ($pattern -f 'host'))
        $clientUnion = [regex]::Match($clientLog, ($pattern -f 'client'))
        if (-not $hostUnion.Success -or -not $clientUnion.Success) { throw 'Missing native saved-avatar union evidence.' }
        foreach ($group in 1..4) {
            if ($hostUnion.Groups[$group].Value -ne $clientUnion.Groups[$group].Value) { throw 'Personal avatar IDs/13-byte appearances differ across peers.' }
        }
        if ($hostUnion.Groups[3].Value -ne $hostSavedAppearance -or $hostUnion.Groups[4].Value -ne $clientSavedAppearance -or
            [int]$hostUnion.Groups[5].Value -lt 2 -or [int]$hostUnion.Groups[5].Value -gt 128) {
            throw 'Natural initialization did not select both editor-produced personal appearances within its bounded fixture.'
        }
        foreach ($role in @('host', 'client')) {
            $roleRoot = if ($role -eq 'host') { $hostRoot } else { $clientRoot }
            $expected = if ($role -eq 'host') { $hostSavedAppearance } else { $clientSavedAppearance }
            $proof = Get-Content -LiteralPath (Join-Path $roleRoot 'avatar_before_join.json') -Raw | ConvertFrom-Json
            if ($proof.status -ne 'pass' -or $proof.active -ne $false -or $proof.bytes -ne 13 -or $proof.appearance -ne $expected) {
                throw 'The restarted process did not load its personal appearance before connecting.'
            }
            if (-not (Test-Path -LiteralPath (Join-Path $roleRoot 'avatar_saved_union.bmp') -PathType Leaf)) {
                throw 'Missing saved-avatar union renderer capture.'
            }
        }
    }
    if ($VerifyInteractions) {
        $packetPattern = '(?im)\bSMOKE_INTERACTIONS_SEED\b[^\r\n]*\bid=([1-9]\d*)\b'
        $hostPacket = [regex]::Match($hostLog, $packetPattern)
        $clientPacket = [regex]::Match($clientLog, $packetPattern)
        if (-not $hostPacket.Success -or -not $clientPacket.Success -or
            $hostPacket.Groups[1].Value -ne $clientPacket.Groups[1].Value) {
            throw "Alt-click packet ping resolved to different authoritative identities."
        }
    }
    if ($VerifyFirstAdventure) {
        $introIdentityPattern = '(?im)\bSMOKE_FRESH_INTRO\b[^\r\n]*\bfirstPlant=(\d+) firstZombie=(\d+)\b'
        $hostIntroIdentity = [regex]::Match($hostLog, $introIdentityPattern)
        $clientIntroIdentity = [regex]::Match($clientLog, $introIdentityPattern)
        if (-not $hostIntroIdentity.Success -or -not $clientIntroIdentity.Success -or
            $hostIntroIdentity.Groups[1].Value -ne $clientIntroIdentity.Groups[1].Value -or
            $hostIntroIdentity.Groups[2].Value -ne $clientIntroIdentity.Groups[2].Value) {
            throw "Stock opening demo used different plant/zombie identities across peers."
        }
        foreach ($roleRoot in @($hostRoot, $clientRoot)) {
            foreach ($frameName in @("intro", "sod", "flash", "baseline_flash_off", "award_idle_rays",
                "award_pickup_rays", "award_sunflower", "next_level_clean", "menu_unlocks", "profile_switched",
                "challenge_open", "challenge_closed", "store_open", "store_page", "store_closed", "profile_restored",
                "options_coop", "settings_sunflower_a", "settings_sunflower_b", "settings_closed", "options_closed",
                "online_browser", "online_closed", "complete")) {
                if (-not (Test-Path -LiteralPath (Join-Path $roleRoot ("fresh_" + $frameName + ".bmp")) -PathType Leaf)) {
                    throw ("Missing first-adventure render capture: {0}/{1}" -f $roleRoot, $frameName)
                }
            }
        }
    }
    Assert-NoNegativePatterns -Role "host" -Text $hostLog -Patterns $negativePatterns
    Assert-NoNegativePatterns -Role "client" -Text $clientLog -Patterns $negativePatterns

    $combinedActionLog = $hostLog + [Environment]::NewLine + $clientLog
    $expectedActionOwner = if ($Scenario -eq "Adventure5BowlingClient" -or
        $Scenario -eq "Adventure15WhackClient" -or
        $Scenario -eq "SlotMachineClient" -or
        $Scenario -eq "TowerDefenseClient" -or
        $Scenario -eq "PortalCombatClient" -or
        $Scenario -eq "PvP1v1" -or
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
    if ($Scenario -eq "TowerDefenseClient") {
        $hostTdPath = [regex]::Match($hostLog,
            '(?im)\bSMOKE_TD_PATH\b[^\r\n]*\brole=host\b[^\r\n]*\bbytes=([1-9]\d*)\b[^\r\n]*\bhash=([0-9a-f]{8})\b[^\r\n]*\bedgeVisits=3\b[^\r\n]*\bedgePattern=(TBT|BTB)\b[^\r\n]*\bseparated=1\b[^\r\n]*\bno2x2=1\b')
        $clientTdPath = [regex]::Match($clientLog,
            '(?im)\bSMOKE_TD_PATH\b[^\r\n]*\brole=client\b[^\r\n]*\bbytes=([1-9]\d*)\b[^\r\n]*\bhash=([0-9a-f]{8})\b[^\r\n]*\bedgeVisits=3\b[^\r\n]*\bedgePattern=(TBT|BTB)\b[^\r\n]*\bseparated=1\b[^\r\n]*\bno2x2=1\b')
        $hostTdRoad = [regex]::Match($hostLog,
            '(?im)\bSMOKE_TD_ROAD_RENDER_COMPLETE\b[^\r\n]*\brole=host\b[^\r\n]*\bboard=(?!0+\b)[0-9a-f]+\b[^\r\n]*\bcells=([1-9]\d*)\b')
        $clientTdRoad = [regex]::Match($clientLog,
            '(?im)\bSMOKE_TD_ROAD_RENDER_COMPLETE\b[^\r\n]*\brole=client\b[^\r\n]*\bboard=(?!0+\b)[0-9a-f]+\b[^\r\n]*\bcells=([1-9]\d*)\b')
        $tdPathEvidence = @($hostTdPath, $clientTdPath, $hostTdRoad, $clientTdRoad)
        if (@($tdPathEvidence | Where-Object { -not $_.Success }).Count -ne 0) {
            throw "Missing Tower Defense path/road correlation evidence."
        }
        if ($hostTdPath.Groups[1].Value -ne $clientTdPath.Groups[1].Value -or
            $hostTdPath.Groups[2].Value.ToLowerInvariant() -ne
                $clientTdPath.Groups[2].Value.ToLowerInvariant() -or
            $hostTdPath.Groups[3].Value -ne $clientTdPath.Groups[3].Value) {
            throw ("Tower Defense path mismatch: host bytes/hash/pattern={0}/{1}/{2}, client bytes/hash/pattern={3}/{4}/{5}." -f
                $hostTdPath.Groups[1].Value, $hostTdPath.Groups[2].Value, $hostTdPath.Groups[3].Value,
                $clientTdPath.Groups[1].Value, $clientTdPath.Groups[2].Value, $clientTdPath.Groups[3].Value)
        }
        if ($hostTdRoad.Groups[1].Value -ne $clientTdRoad.Groups[1].Value) {
            throw ("Tower Defense road cell-count mismatch: host={0}, client={1}." -f
                $hostTdRoad.Groups[1].Value, $clientTdRoad.Groups[1].Value)
        }

        $hostTdEntry = [regex]::Match($hostLog,
            '(?im)\bSMOKE_TD_ZOMBIE_ENTRY\b[^\r\n]*\brole=host\b[^\r\n]*\bid=([1-9]\d*)\b')
        $clientTdEntry = [regex]::Match($clientLog,
            '(?im)\bSMOKE_TD_ZOMBIE_ENTRY\b[^\r\n]*\brole=client\b[^\r\n]*\bid=([1-9]\d*)\b')
        $hostTdTurn = [regex]::Match($hostLog,
            '(?im)\bSMOKE_TD_ZOMBIE_TURN\b[^\r\n]*\brole=host\b[^\r\n]*\bid=([1-9]\d*)\b')
        $clientTdTurn = [regex]::Match($clientLog,
            '(?im)\bSMOKE_TD_ZOMBIE_TURN\b[^\r\n]*\brole=client\b[^\r\n]*\bid=([1-9]\d*)\b')
        $tdZombieEvidence = @($hostTdEntry, $clientTdEntry, $hostTdTurn, $clientTdTurn)
        if (@($tdZombieEvidence | Where-Object { -not $_.Success }).Count -ne 0) {
            throw "Missing Tower Defense zombie entry/turn identity evidence."
        }
        $tdZombieIds = @($tdZombieEvidence | ForEach-Object { [uint32]$_.Groups[1].Value } |
            Select-Object -Unique)
        if ($tdZombieIds.Count -ne 1) {
            throw ("Tower Defense zombie identity mismatch: {0}." -f ($tdZombieIds -join ","))
        }

        $hostTdPlant = [regex]::Match($hostLog,
            '(?im)\bSMOKE_TD_CLIENT_PLANT\b[^\r\n]*\brole=host\b[^\r\n]*\bid=([1-9]\d*)\b[^\r\n]*\bcell=\(([0-8],[0-5])\)')
        $clientTdPlant = [regex]::Match($clientLog,
            '(?im)\bSMOKE_TD_CLIENT_PLANT\b[^\r\n]*\brole=client\b[^\r\n]*\bid=([1-9]\d*)\b[^\r\n]*\bcell=\(([0-8],[0-5])\)')
        if (-not $hostTdPlant.Success -or -not $clientTdPlant.Success) {
            throw "Missing Tower Defense client-plant identity evidence."
        }
        if ($hostTdPlant.Groups[1].Value -ne $clientTdPlant.Groups[1].Value -or
            $hostTdPlant.Groups[2].Value -ne $clientTdPlant.Groups[2].Value) {
            throw ("Tower Defense client plant mismatch: host id/cell={0}/{1}, client id/cell={2}/{3}." -f
                $hostTdPlant.Groups[1].Value, $hostTdPlant.Groups[2].Value,
                $clientTdPlant.Groups[1].Value, $clientTdPlant.Groups[2].Value)
        }
    }

    if ($Scenario -eq "PortalCombatClient") {
        $hostCameraBootstrap = [regex]::Match($hostLog,
            '(?im)\bcamera bootstrap encoded board=\((-?\d+),(-?\d+)\) tree=(-?\d+) pole=(-?\d+)')
        $clientCameraBootstrap = [regex]::Match($clientLog,
            '(?im)\bcamera bootstrap applied board=\((-?\d+),(-?\d+)\) tree=(-?\d+) pole=(-?\d+)')
        if (-not $hostCameraBootstrap.Success -or -not $clientCameraBootstrap.Success) {
            throw "Missing reliable Portal Combat START_PLAYING camera bootstrap evidence."
        }
		$hostPlaying = [regex]::Match($hostLog,
			'(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario=portal_combat_client[^\r\n]*step=PLAYING\b')
		$clientPlaying = [regex]::Match($clientLog,
			'(?im)\bSMOKE_CHECKPOINT\b[^\r\n]*scenario=portal_combat_client[^\r\n]*step=PLAYING\b')
		if (-not $hostPlaying.Success -or -not $clientPlaying.Success -or
			$hostCameraBootstrap.Index -ge $hostPlaying.Index -or
			$clientCameraBootstrap.Index -ge $clientPlaying.Index) {
			throw "Portal Combat camera bootstrap was not applied before the first PLAYING checkpoint."
		}
        for ($group = 1; $group -le 4; ++$group) {
            if ($hostCameraBootstrap.Groups[$group].Value -ne $clientCameraBootstrap.Groups[$group].Value) {
                throw ("Portal Combat START_PLAYING camera bootstrap mismatch in group {0}: host={1}, client={2}." -f
                    $group, $hostCameraBootstrap.Groups[$group].Value, $clientCameraBootstrap.Groups[$group].Value)
            }
        }

        $portalCameraPattern = '(?im)\bSMOKE_CAMERA_SYNC\b[^\r\n]*\bphase=start\b[^\r\n]*\bboard=\((-?\d+),(-?\d+)\)[^\r\n]*\btarget=\((-?\d+),(-?\d+)\)[^\r\n]*\btree=(-?\d+)\s+targetTree=(-?\d+)\s+pole=(-?\d+)\s+targetPole=(-?\d+)[^\r\n]*\bdelta=\((\d+),(\d+)\)[^\r\n]*\bvalid=1\b'
        $hostPortalCamera = [regex]::Match($hostLog, $portalCameraPattern)
        $clientPortalCamera = [regex]::Match($clientLog, $portalCameraPattern)
        if (-not $hostPortalCamera.Success -or -not $clientPortalCamera.Success) {
            throw "Missing Portal Combat canonical camera evidence."
        }
        if ($hostPortalCamera.Groups[1].Value -ne $clientPortalCamera.Groups[1].Value -or
            $hostPortalCamera.Groups[2].Value -ne $clientPortalCamera.Groups[2].Value -or
			$hostPortalCamera.Groups[5].Value -ne $clientPortalCamera.Groups[5].Value -or
			$hostPortalCamera.Groups[7].Value -ne $clientPortalCamera.Groups[7].Value -or
			[int]$hostPortalCamera.Groups[9].Value -gt 4 -or
			[int]$hostPortalCamera.Groups[10].Value -gt 4 -or
			[int]$clientPortalCamera.Groups[9].Value -gt 4 -or
			[int]$clientPortalCamera.Groups[10].Value -gt 4) {
            throw ("Portal Combat camera mismatch: host=({0},{1}) client=({2},{3})." -f
                $hostPortalCamera.Groups[1].Value, $hostPortalCamera.Groups[2].Value,
                $clientPortalCamera.Groups[1].Value, $clientPortalCamera.Groups[2].Value)
        }

		$portalSteadyCameraPattern = '(?im)\bSMOKE_CAMERA_SYNC\b[^\r\n]*\bphase=steady\b[^\r\n]*\bsamples=([2-9]\d|[1-9]\d{2,})\b[^\r\n]*\bmaxDelta=([0-4])\b[^\r\n]*\bintroTargetValid=0\b[^\r\n]*\btargetValid=1\b'
		$hostSteadyCamera = [regex]::Match($hostLog, $portalSteadyCameraPattern)
		$clientSteadyCamera = [regex]::Match($clientLog, $portalSteadyCameraPattern)
		if (-not $hostSteadyCamera.Success -or -not $clientSteadyCamera.Success) {
			throw "Missing 20-sample steady Portal Combat camera evidence."
		}

        $portalLayoutPattern = '(?im)\bSMOKE_PORTAL_LAYOUT\b[^\r\n]*\bhash=([0-9a-f]{8})\b[^\r\n]*\bids=([^\s]+)\s+types=([^\s]+)\s+cells=([^\s]+)\s+states=([^\s]+)'
        $hostPortalLayout = [regex]::Match($hostLog, $portalLayoutPattern)
        $clientPortalLayout = [regex]::Match($clientLog, $portalLayoutPattern)
        if (-not $hostPortalLayout.Success -or -not $clientPortalLayout.Success) {
            throw "Missing Portal Combat canonical layout evidence."
        }
        for ($group = 1; $group -le 5; ++$group) {
            if ($hostPortalLayout.Groups[$group].Value -ne $clientPortalLayout.Groups[$group].Value) {
                throw ("Portal Combat initial layout mismatch in group {0}: host={1}, client={2}." -f
                    $group, $hostPortalLayout.Groups[$group].Value, $clientPortalLayout.Groups[$group].Value)
            }
        }

        $portalConveyorPattern = '(?im)\bSMOKE_PORTAL_CONVEYOR\b[^\r\n]*\bcount=([1-9]\d*)\b[^\r\n]*\bids=([^\s]+)\s+types=([^\s]+)\s+positions=([^\s]+)'
        $hostPortalConveyor = [regex]::Match($hostLog, $portalConveyorPattern)
        $clientPortalConveyor = [regex]::Match($clientLog, $portalConveyorPattern)
        if (-not $hostPortalConveyor.Success -or -not $clientPortalConveyor.Success) {
            throw "Missing Portal Combat conveyor layout evidence."
        }
        for ($group = 1; $group -le 4; ++$group) {
            if ($hostPortalConveyor.Groups[$group].Value -ne $clientPortalConveyor.Groups[$group].Value) {
                throw ("Portal Combat conveyor mismatch in group {0}: host={1}, client={2}." -f
                    $group, $hostPortalConveyor.Groups[$group].Value, $clientPortalConveyor.Groups[$group].Value)
            }
        }

        $portalEntryPattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_ENTRY\b[^\r\n]*\bid=([1-9]\d*)\b[^\r\n]*\brow=([0-5])\b[^\r\n]*\bx=(-?\d+\.\d{2})\b'
        $portalTeleportPattern = '(?im)\bSMOKE_PORTAL_ZOMBIE_TELEPORT\b[^\r\n]*\bid=([1-9]\d*)\b[^\r\n]*\bfromRow=([0-5])\b[^\r\n]*\btoRow=([0-5])\b[^\r\n]*\bbeforeX=(-?\d+\.\d{2})\b[^\r\n]*\bafterX=(-?\d+\.\d{2})\b'
        $portalZombieEvidence = @(
            [regex]::Match($hostLog, $portalEntryPattern),
            [regex]::Match($clientLog, $portalEntryPattern),
            [regex]::Match($hostLog, $portalTeleportPattern),
            [regex]::Match($clientLog, $portalTeleportPattern)
        )
        if (@($portalZombieEvidence | Where-Object { -not $_.Success }).Count -ne 0) {
            throw "Missing Portal Combat zombie entry/teleport correlation evidence."
        }
        $portalZombieIds = @($portalZombieEvidence | ForEach-Object { [uint32]$_.Groups[1].Value } | Select-Object -Unique)
        if ($portalZombieIds.Count -ne 1) {
            throw ("Portal Combat zombie identity mismatch: {0}." -f ($portalZombieIds -join ","))
        }
        if ($portalZombieEvidence[2].Groups[2].Value -ne $portalZombieEvidence[3].Groups[2].Value -or
            $portalZombieEvidence[2].Groups[3].Value -ne $portalZombieEvidence[3].Groups[3].Value) {
            throw ("Portal Combat teleport row transition mismatch: host={0}->{1}, client={2}->{3}." -f
                $portalZombieEvidence[2].Groups[2].Value, $portalZombieEvidence[2].Groups[3].Value,
                $portalZombieEvidence[3].Groups[2].Value, $portalZombieEvidence[3].Groups[3].Value)
        }

        $portalPlantPattern = '(?im)\bSMOKE_PORTAL_CLIENT_PLANT\b[^\r\n]*\bid=([1-9]\d*)\b[^\r\n]*\btype=(\d+)\b[^\r\n]*\bcell=\(([0-8],[0-5])\)'
        $hostPortalPlant = [regex]::Match($hostLog, $portalPlantPattern)
        $clientPortalPlant = [regex]::Match($clientLog, $portalPlantPattern)
        if (-not $hostPortalPlant.Success -or -not $clientPortalPlant.Success -or
            $hostPortalPlant.Groups[1].Value -ne $clientPortalPlant.Groups[1].Value -or
            $hostPortalPlant.Groups[2].Value -ne $clientPortalPlant.Groups[2].Value -or
            $hostPortalPlant.Groups[3].Value -ne $clientPortalPlant.Groups[3].Value) {
            throw "Portal Combat client plant identity/type/cell mismatch."
        }
		$portalInput = [regex]::Match($clientLog,
			'(?im)\bSMOKE_PORTAL_CLIENT_INPUT\b[^\r\n]*\bpacketId=([1-9]\d*)\b[^\r\n]*\bindex=(\d+)\b')
		$portalEcho = [regex]::Match($clientLog,
			'(?im)\bcursor held packet echo\b[^\r\n]*\bslot=1\b[^\r\n]*\bid=([1-9]\d*)\b[^\r\n]*\bindex=(\d+)\b')
		if (-not $portalInput.Success -or -not $portalEcho.Success -or
			$portalInput.Groups[1].Value -ne $portalEcho.Groups[1].Value -or
			$portalInput.Groups[2].Value -ne $portalEcho.Groups[2].Value) {
			throw "Portal Combat held conveyor packet echo id/index mismatch."
		}

        $portalRelocationPattern = '(?im)\bSMOKE_PORTAL_RELOCATION\b[^\r\n]*\bbefore=([0-9a-f]{8})\b[^\r\n]*\bafter=([0-9a-f]{8})\b'
        $hostPortalRelocation = [regex]::Match($hostLog, $portalRelocationPattern)
        $clientPortalRelocation = [regex]::Match($clientLog, $portalRelocationPattern)
        if (-not $hostPortalRelocation.Success -or -not $clientPortalRelocation.Success -or
            $hostPortalRelocation.Groups[1].Value -ne $clientPortalRelocation.Groups[1].Value -or
            $hostPortalRelocation.Groups[2].Value -ne $clientPortalRelocation.Groups[2].Value -or
            $hostPortalRelocation.Groups[1].Value -eq $hostPortalRelocation.Groups[2].Value) {
            throw "Portal Combat relocation hashes are missing, mismatched, or unchanged."
        }
    }

    if ($Scenario -eq "EndlessST") {
        $endlessTimePattern = '(?im)\bSMOKE_ENDLESS_ST_TIME\b[^\r\n]*\bfrom=([01])\b[^\r\n]*\bto=([01])\b'
        $endlessRepickPattern = '(?im)\bSMOKE_ENDLESS_ST_REPICK_COMPLETE\b[^\r\n]*\bfromStage=(\d+)\b[^\r\n]*\btoStage=(\d+)\b'
        $endlessHealthPattern = '(?im)\bSMOKE_ENDLESS_ST_HEALTH_SCALE\b[^\r\n]*\btype=(\d+)\b[^\r\n]*\bstage=(\d+)\b[^\r\n]*\bbase=(\d+)\b[^\r\n]*\bscaled=(\d+)\b[^\r\n]*\bexpected=(\d+)\b'
        $hostEndlessTime = [regex]::Match($hostLog, $endlessTimePattern)
        $clientEndlessTime = [regex]::Match($clientLog, $endlessTimePattern)
        $hostEndlessRepick = [regex]::Match($hostLog, $endlessRepickPattern)
        $clientEndlessRepick = [regex]::Match($clientLog, $endlessRepickPattern)
        $hostEndlessHealth = [regex]::Match($hostLog, $endlessHealthPattern)
        $clientEndlessHealth = [regex]::Match($clientLog, $endlessHealthPattern)
        $endlessEvidence = @($hostEndlessTime, $clientEndlessTime,
            $hostEndlessRepick, $clientEndlessRepick,
            $hostEndlessHealth, $clientEndlessHealth)
        if (@($endlessEvidence | Where-Object { -not $_.Success }).Count -ne 0) {
            throw "Missing EndlessST time, repick, or health-scaling correlation evidence."
        }
        if ($hostEndlessTime.Groups[1].Value -eq $hostEndlessTime.Groups[2].Value -or
            $hostEndlessTime.Groups[1].Value -ne $clientEndlessTime.Groups[1].Value -or
            $hostEndlessTime.Groups[2].Value -ne $clientEndlessTime.Groups[2].Value) {
            throw "EndlessST day/night phase did not change identically on host and client."
        }
        for ($group = 1; $group -le 2; ++$group) {
            if ($hostEndlessRepick.Groups[$group].Value -ne
                $clientEndlessRepick.Groups[$group].Value) {
                throw "EndlessST repick stage transition mismatch."
            }
        }
        if ([int]$hostEndlessRepick.Groups[2].Value -ne
            ([int]$hostEndlessRepick.Groups[1].Value + 1)) {
            throw "EndlessST repick did not advance exactly one ten-wave stage."
        }
        for ($group = 1; $group -le 5; ++$group) {
            if ($hostEndlessHealth.Groups[$group].Value -ne
                $clientEndlessHealth.Groups[$group].Value) {
                throw ("EndlessST health scaling mismatch in group {0}." -f $group)
            }
        }
        if ($hostEndlessHealth.Groups[4].Value -ne $hostEndlessHealth.Groups[5].Value -or
            [int]$hostEndlessHealth.Groups[4].Value -le
            [int]$hostEndlessHealth.Groups[3].Value) {
            throw "EndlessST post-repick health did not equal the authoritative scaled value."
        }
    }

    if ($Scenario -eq "EndlessOverflow") {
        $overflowPrePattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_STAGE\b[^\r\n]*\bphase=pre\b[^\r\n]*\bstage=(\d+)\b'
        $overflowPostPattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_STAGE\b[^\r\n]*\bphase=post\b[^\r\n]*\bstage=(\d+)\b'
        $overflowHealthPattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_HEALTH\b[^\r\n]*\bid=([1-9]\d*)\b[^\r\n]*\btype=(\d+)\b[^\r\n]*\bstage=(\d+)\b[^\r\n]*\bbody=(\d+)\b[^\r\n]*\bmax=(\d+)\b'
        $overflowRepickPattern = '(?im)\bSMOKE_ENDLESS_OVERFLOW_REPICK_COMPLETE\b[^\r\n]*\bfrom=(\d+)\b[^\r\n]*\bto=(\d+)\b[^\r\n]*\bchooser=1\b'
        $hostOverflowPre = [regex]::Match($hostLog, $overflowPrePattern)
        $clientOverflowPre = [regex]::Match($clientLog, $overflowPrePattern)
        $hostOverflowPost = [regex]::Match($hostLog, $overflowPostPattern)
        $clientOverflowPost = [regex]::Match($clientLog, $overflowPostPattern)
        $hostOverflowHealth = [regex]::Match($hostLog, $overflowHealthPattern)
        $clientOverflowHealth = [regex]::Match($clientLog, $overflowHealthPattern)
        $hostOverflowRepick = [regex]::Match($hostLog, $overflowRepickPattern)
        $clientOverflowRepick = [regex]::Match($clientLog, $overflowRepickPattern)
        $overflowEvidence = @($hostOverflowPre, $clientOverflowPre,
            $hostOverflowPost, $clientOverflowPost, $hostOverflowHealth,
            $clientOverflowHealth, $hostOverflowRepick, $clientOverflowRepick)
        if (@($overflowEvidence | Where-Object { -not $_.Success }).Count -ne 0) {
            throw "Missing EndlessOverflow stage, health, or repick correlation evidence."
        }
        if ([int]$hostOverflowPre.Groups[1].Value -ne 255 -or
            $hostOverflowPre.Groups[1].Value -ne $clientOverflowPre.Groups[1].Value -or
            [int]$hostOverflowPost.Groups[1].Value -ne 256 -or
            $hostOverflowPost.Groups[1].Value -ne $clientOverflowPost.Groups[1].Value) {
            throw "EndlessOverflow stage boundary mismatch or wrap (expected 255 then 256 on both peers)."
        }
        for ($group = 1; $group -le 5; ++$group) {
            if ($hostOverflowHealth.Groups[$group].Value -ne
                $clientOverflowHealth.Groups[$group].Value) {
                throw ("EndlessOverflow red-eye identity/health mismatch in group {0}." -f $group)
            }
        }
        if ([int64]$hostOverflowHealth.Groups[4].Value -le 65535 -or
            [int64]$hostOverflowHealth.Groups[5].Value -le 65535 -or
            [int]$hostOverflowHealth.Groups[3].Value -ne 255) {
            throw "EndlessOverflow red-eye health did not cross the 16-bit boundary at stage 255."
        }
        for ($group = 1; $group -le 2; ++$group) {
            if ($hostOverflowRepick.Groups[$group].Value -ne
                $clientOverflowRepick.Groups[$group].Value) {
                throw "EndlessOverflow stock repick transition mismatch."
            }
        }
        if ([int]$hostOverflowRepick.Groups[1].Value -ne 255 -or
            [int]$hostOverflowRepick.Groups[2].Value -ne 256) {
            throw "EndlessOverflow repick did not transition exactly from stage 255 to 256."
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
        transport = $(if ($OnlineEndpoint) { "webrtc" } else { "tcp" })
        scenario = $Scenario
        mode = $Mode
        level = $Level
        skip_intro = $SkipIntro
        verify_first_adventure = [bool]$VerifyFirstAdventure
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
