#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [string]$SourceVideo = 'C:\PvZ\intro.mp4',
    [ValidateSet('Complete', 'Escape')][string]$PlaybackAction = 'Complete',
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 180,
    [ValidateRange(50, 1000)][int]$MaximumPumpGapMs = 250,
    [ValidateRange(1, 30)][int]$GracefulExitSeconds = 15
)

$videoExeArg = $GameExe
$videoArtifactsArg = $ArtifactsRoot
$videoSourceArg = $SourceVideo
$videoPlaybackAction = $PlaybackAction
$videoTimeout = $TimeoutSeconds
$videoGapLimit = $MaximumPumpGapMs
$videoGracefulSeconds = $GracefulExitSeconds
$videoScripts = $PSScriptRoot
. (Join-Path $videoScripts 'RunCoopSmoke.ps1') -FunctionsOnly
$Background = $true
$videoRepo = Get-NormalizedPath (Join-Path $videoScripts '..\..')
$videoExe = if ($videoExeArg) { Get-NormalizedPath $videoExeArg } else {
    Join-Path $videoRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'
}
$videoSource = Get-NormalizedPath $videoSourceArg
$videoAssets = Join-Path $videoRepo 'assets\videos\intro.mp4'
$videoGameDirectory = Split-Path -Parent $videoExe
$videoOutput = Join-Path $videoGameDirectory 'videos\intro.mp4'
$videoExpectedSha = '4A325C5C1743F02D1923B4D6FCB561155E8ED1DF1E300ED8770E454F7DE5F0AD'
$videoArtifacts = if ($videoArtifactsArg) { Get-NormalizedPath $videoArtifactsArg } else { Join-Path $videoRepo 'work\startup-video-smoke' }
$videoRunId = [Guid]::NewGuid().ToString('N')
$videoRun = Join-Path $videoArtifacts ('run_' + $videoRunId)
$videoHost = Join-Path $videoRun 'host'
$videoLogs = Join-Path $videoHost 'logs'
$videoManifest = Join-Path $videoHost 'smoke.ini'
$videoProofPath = Join-Path $videoHost 'startup-video.json'
$videoEntryPath = Join-Path $videoHost 'startup-video-started.json'
$videoProcess = $null
$videoFailure = ''
$videoProof = $null
$videoEntryProof = $null
$videoPlaybackWindow = [IntPtr]::Zero
$videoActionRecord = $null
$videoActionSentAt = $null
$videoSkipResponseMs = $null
$videoHashes = @()
$videoFrames = @()
$videoLiveness = [Collections.Generic.List[object]]::new()
$videoOldFlag = $env:PVZ_COOP_SMOKE_STARTUP_VIDEO
$videoStarted = [DateTime]::UtcNow

if (-not ('StartupVideoSmokeWindow' -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class StartupVideoSmokeWindow
{
    private delegate bool EnumProc(IntPtr window, IntPtr ignored);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr ignored);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")] private static extern IntPtr GetWindow(IntPtr window, uint command);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool PostMessage(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr SendMessageTimeout(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam, uint flags, uint timeout, out UIntPtr result);
    public static IntPtr FindOwned(int processId)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr window, IntPtr ignored) {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner == (uint)processId && GetWindow(window, 4) == IntPtr.Zero)
            { found = window; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static bool Ping(IntPtr window)
    {
        UIntPtr result;
        return SendMessageTimeout(window, 0, IntPtr.Zero, IntPtr.Zero, 3, 250, out result) != IntPtr.Zero;
    }
    public static uint OwnedThread(IntPtr window, int processId)
    {
        uint owner;
        uint thread = GetWindowThreadProcessId(window, out owner);
        return owner == (uint)processId ? thread : 0;
    }
    public static bool PostEscape(IntPtr window, int processId)
    {
        if (OwnedThread(window, processId) == 0) return false;
        return PostMessage(window, 0x0100, new IntPtr(0x1b), new IntPtr(0x00010001)) &&
            PostMessage(window, 0x0101, new IntPtr(0x1b), new IntPtr(unchecked((int)0xC0010001)));
    }
}
"@
}

function Get-VideoField {
    param([object]$Value, [string]$Name)
    return Get-RequiredProperty -Object $Value -Name $Name -Role 'startup-video'
}

function Assert-VideoHashes {
    $result = @()
    foreach ($path in @($videoSource, $videoAssets, $videoOutput)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Intro MP4 missing: ' + $path) }
        $sha = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($sha -ne $videoExpectedSha) { throw ('Intro source/deployed SHA256 mismatch: ' + $path) }
        $result += [ordered]@{ path = $path; sha256 = $sha }
    }
    return $result
}

function Assert-VideoEntry {
    param([object]$Entry)
    if ([string](Get-VideoField $Entry 'phase') -ne 'entered' -or
        [int](Get-VideoField $Entry 'pid') -ne $videoProcess.Id -or
        [int](Get-VideoField $Entry 'windowProcessId') -ne $videoProcess.Id -or
        (Get-NormalizedPath ([string](Get-VideoField $Entry 'sourceUrl'))) -ne (Get-NormalizedPath $videoOutput)) {
        throw 'Startup entry marker does not identify this owned player/source.'
    }
    $windowValue = [long]::Parse([string](Get-VideoField $Entry 'windowHandle'), [Globalization.CultureInfo]::InvariantCulture)
    if ($windowValue -le 0) { throw 'Startup player did not report its actual SDL HWND.' }
    $window = [IntPtr]::new($windowValue)
    $windowThread = [StartupVideoSmokeWindow]::OwnedThread($window, $videoProcess.Id)
    if ($windowThread -eq 0 -or $windowThread -ne [uint32](Get-VideoField $Entry 'windowThreadId') -or
        $windowThread -ne [uint32](Get-VideoField $Entry 'callerThreadId') -or
        $windowThread -ne [uint32](Get-VideoField $Entry 'primaryThreadId')) {
        throw 'Startup playback is not running on the owned SDL/main-window thread.'
    }
    $age = [double](Get-VideoField $Entry 'processAgeMs')
    if ([double]::IsNaN($age) -or [double]::IsInfinity($age) -or $age -lt 0) { throw 'Startup entry marker has an invalid process age.' }
    return $window
}

function Assert-VideoFrame {
    param([object]$Capture, [string]$Name, [double]$RequestedPts)
    if ([string](Get-VideoField $Capture 'file') -ne $Name -or
        (Get-VideoField $Capture 'success') -isnot [bool] -or -not [bool]$Capture.success) {
        throw ('Startup readback capture was not successful: ' + $Name)
    }
    $pts = [double](Get-VideoField $Capture 'pts')
    if ([double]::IsNaN($pts) -or [double]::IsInfinity($pts) -or $pts -lt ($RequestedPts - 0.05) -or $pts -gt ($RequestedPts + 0.2)) {
        throw ('Startup capture missed its early source timestamp: ' + $Name + ' pts=' + $pts)
    }
    $path = Join-Path $videoHost $Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Actual startup renderer BMP missing: ' + $path) }
    $bitmap = [Drawing.Bitmap]::new($path)
    try {
        if ($bitmap.Width -lt 320 -or $bitmap.Height -lt 180) { throw ('Startup renderer BMP is unexpectedly small: ' + $Name) }
        $colors = [Collections.Generic.HashSet[int]]::new()
        $lit = 0
        $count = 0
        $stepX = [Math]::Max(1, [int]($bitmap.Width / 64))
        $stepY = [Math]::Max(1, [int]($bitmap.Height / 36))
        for ($x = 0; $x -lt $bitmap.Width; $x += $stepX) {
            for ($y = 0; $y -lt $bitmap.Height; $y += $stepY) {
                $pixel = $bitmap.GetPixel($x, $y)
                [void]$colors.Add($pixel.ToArgb())
                if (($pixel.R + $pixel.G + $pixel.B) -gt 24) { $lit++ }
                $count++
            }
        }
        if ($colors.Count -lt 8 -or $lit -lt 4) { throw ('Startup renderer BMP is blank/flat: ' + $Name) }
        return [ordered]@{ file = $Name; pts = $pts; width = $bitmap.Width; height = $bitmap.Height;
            sampled_colors = $colors.Count; lit_samples = $lit; total_samples = $count;
            sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    }
    finally { $bitmap.Dispose() }
}

try {
    if (-not (Test-Path -LiteralPath $videoExe -PathType Leaf)) { throw ('Game executable missing: ' + $videoExe) }
    $videoHashes = @(Assert-VideoHashes)
    Ensure-Directory $videoLogs
    Ensure-Directory (Join-Path $videoHost 'appdata')
    Write-CoopConfig -Path (Join-Path $videoHost 'coop.ini') -Nickname StartupVideoHost -WindowWidth 1066 -WindowHeight 600
    Write-SmokeManifest -Path $videoManifest -Role host -Nickname StartupVideoHost `
        -AppDataDirectory (Join-Path $videoHost 'appdata') -ConfigPath (Join-Path $videoHost 'coop.ini') `
        -LogDirectory $videoLogs -ResultPath (Join-Path $videoHost 'result.json') `
        -SelectedPort (Get-FreeLoopbackPort) -SelectedMode adventure -SelectedLevel 1 -SelectedSeed 424242 `
        -SelectedTimeoutMs ($videoTimeout * 1000) -RequiredSnapshots 1 -SelectedSkipIntro 0
    $env:PVZ_COOP_SMOKE_STARTUP_VIDEO = '1'
    $videoProcess = Start-SmokeProcess -Executable $videoExe -WorkingDirectory $videoGameDirectory `
        -ManifestPath $videoManifest -Role host -RunId $videoRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $videoRun 'pids.json') -Executable $videoExe -HostProcess $videoProcess -ClientProcess $null
    Write-SmokeStatus ('Startup video ' + $videoPlaybackAction + ': one owned hidden host; artifacts: ' + $videoRun)
    $deadline = [DateTime]::UtcNow.AddSeconds($videoTimeout)
    while ([DateTime]::UtcNow -lt $deadline) {
        $text = Get-CombinedLogText $videoLogs
        if ($text -match '(?im)\bSMOKE_FAIL\b|Access Violation|unhandled exception|assertion failed') { throw 'Startup runtime failure; inspect owned host logs.' }
        if ($videoProcess.HasExited) { throw ('Startup host exited without movie proof: ' + $videoProcess.ExitCode) }
        if (-not (Test-OwnedProcessIdentity $videoProcess)) { throw 'Startup host PID/path/start identity no longer matches.' }
        if ($null -eq $videoEntryProof) {
            $entry = Read-JsonIfComplete -Path $videoEntryPath
            if ($null -ne $entry) {
                $videoPlaybackWindow = Assert-VideoEntry $entry
                $videoEntryProof = $entry
            }
        }
        $videoProof = Read-JsonIfComplete -Path $videoProofPath
        if ($null -ne $videoProof) {
            if ($null -ne $videoActionSentAt) { $videoSkipResponseMs = ([DateTime]::UtcNow-$videoActionSentAt).TotalMilliseconds }
            break
        }
        if ($null -ne $videoActionSentAt -and ([DateTime]::UtcNow-$videoActionSentAt).TotalSeconds -gt 2) {
            throw 'Startup player did not acknowledge Escape within2seconds.'
        }
        $phase = if ($null -eq $videoEntryProof) { 'pre-playback' } else { 'playback' }
        # Initial resource/definition loading creates windows before PlayVideo.
        # Those samples are retained as diagnostics. Strict responsiveness starts
        # at the actual player entry, including decoder/open before frame0.
        $window = if ($phase -eq 'playback') { $videoPlaybackWindow } else { [StartupVideoSmokeWindow]::FindOwned($videoProcess.Id) }
        if ($window -ne [IntPtr]::Zero) {
            if ($phase -eq 'playback' -and [StartupVideoSmokeWindow]::OwnedThread($window, $videoProcess.Id) -eq 0) {
                throw 'The startup player SDL HWND no longer belongs to the owned process.'
            }
            $watch = [Diagnostics.Stopwatch]::StartNew()
            $responsive = [StartupVideoSmokeWindow]::Ping($window)
            $watch.Stop()
            $videoLiveness.Add([ordered]@{ phase = $phase; hwnd = $window.ToInt64().ToString();
                elapsed_seconds = ([DateTime]::UtcNow-$videoStarted).TotalSeconds;
                responded = $responsive; response_ms = $watch.ElapsedMilliseconds })
            if (-not $responsive -and $phase -eq 'playback') { throw 'Owned playback SDL window did not answer WM_NULL within250ms.' }
        }
        if ($videoPlaybackAction -eq 'Escape' -and $phase -eq 'playback' -and $null -eq $videoActionRecord) {
            $entryUtc = $videoProcess.StartTime.ToUniversalTime().AddMilliseconds([double]$videoEntryProof.processAgeMs)
            $sinceEntry = ([DateTime]::UtcNow-$entryUtc).TotalSeconds
            if ($sinceEntry -ge 2.0) {
                # Revalidate the exact player HWND immediately before scoped
                # messages. This never changes focus or injects global input.
                $window = Assert-VideoEntry $videoEntryProof
                $videoActionSentAt = [DateTime]::UtcNow
                $sent = [StartupVideoSmokeWindow]::PostEscape($window, $videoProcess.Id)
                $videoActionRecord = [ordered]@{ action = 'Escape'; transport = 'PostMessage_WM_KEYDOWN_WM_KEYUP';
                    virtual_key = 27; hwnd = $window.ToInt64().ToString(); pid = $videoProcess.Id;
                    sent = $sent; sent_utc = $videoActionSentAt.ToString('o'); seconds_after_entry = $sinceEntry }
                if (-not $sent) { throw 'Could not post Escape down/up to the validated owned SDL HWND.' }
                if ($sinceEntry -gt 3.0) { throw 'Runner could not send Escape near the requested2seconds after player entry.' }
            }
        }
        Start-Sleep -Milliseconds 200
    }
    if ($null -eq $videoProof) { throw 'Startup video proof timed out.' }
    if ($null -eq $videoEntryProof) { throw 'Actual startup player entry was not observed.' }
    if (@($videoLiveness.ToArray() | Where-Object { $_.phase -eq 'playback' -and $_.responded }).Count -lt 1) {
        throw 'No successful window-liveness sample was observed during actual playback.'
    }
    foreach ($flag in @('completed', 'skipped', 'failed')) {
        if ((Get-VideoField $videoProof $flag) -isnot [bool]) { throw ('Startup proof flag must be boolean: ' + $flag) }
    }
    if ($videoPlaybackAction -eq 'Complete') {
        if (-not $videoProof.completed -or $videoProof.skipped -or $videoProof.failed) { throw 'Startup movie did not complete normally.' }
    } else {
        if ($null -eq $videoActionRecord -or -not $videoActionRecord.sent -or
            -not $videoProof.skipped -or $videoProof.completed -or $videoProof.failed) { throw 'Startup Escape did not produce a clean skipped movie result.' }
        if ($null -eq $videoSkipResponseMs -or $videoSkipResponseMs -gt 2000) { throw 'Startup Escape result exceeded the2second response bound.' }
        if ([int64](Get-VideoField $videoProof 'audioFramesQueued') -lt 1) { throw 'No audio frames were queued before startup Escape.' }
    }
    $first = [double](Get-VideoField $videoProof 'firstFramePts')
    if ([double]::IsNaN($first) -or [double]::IsInfinity($first) -or [Math]::Abs($first) -gt 0.05) { throw 'Startup movie did not present its first frame nearPTS0.' }
    if ($videoPlaybackAction -eq 'Complete') {
        if ([int64](Get-VideoField $videoProof 'frameCount') -lt 300 -or [int64](Get-VideoField $videoProof 'preludeFrames') -lt 90) {
            throw 'Startup movie has too few rendered frames, including the first4seconds.'
        }
    } else {
        if ([int64](Get-VideoField $videoProof 'frameCount') -lt 30 -or
            [int64](Get-VideoField $videoProof 'preludeFrames') -lt 30 -or
            [int64]$videoProof.frameCount -ne [int64]$videoProof.preludeFrames) {
            throw 'Startup Escape did not preserve enough actual early frames before the scoped key event.'
        }
    }
    $gap = [double](Get-VideoField $videoProof 'maxEventPumpGapMs')
    if ([double]::IsNaN($gap) -or [double]::IsInfinity($gap) -or $gap -lt 0 -or $gap -gt $videoGapLimit) { throw ('Startup event pump gap exceeds bound: ' + $gap) }
    if ((Get-NormalizedPath ([string](Get-VideoField $videoProof 'sourceUrl'))) -ne (Get-NormalizedPath $videoOutput)) {
        throw 'Startup player used a different MP4 than the deployed executable-relative intro.'
    }
    $videoHashes = @(Assert-VideoHashes)
    Add-Type -AssemblyName System.Drawing
    $captures = @(Get-VideoField $videoProof 'captures')
    $requiredCaptures = @(@('startup-video-0500.bmp', 0.5), @('startup-video-1500.bmp', 1.5))
    if ($videoPlaybackAction -eq 'Complete') { $requiredCaptures += @(@('startup-video-3000.bmp', 3.0), @('startup-video-4500.bmp', 4.5)) }
    foreach ($entry in $requiredCaptures) {
        $matching = @($captures | Where-Object { [string](Get-VideoField $_ 'file') -eq $entry[0] })
        if ($matching.Count -ne 1) { throw ('Startup capture missing/duplicated: ' + $entry[0]) }
        $videoFrames += Assert-VideoFrame $matching[0] $entry[0] $entry[1]
    }
    Request-OwnedProcessClose -Process $videoProcess -Role host
    Wait-ForCleanProcessExit -Process $videoProcess -Role host -TimeoutSeconds $videoGracefulSeconds
}
catch { $videoFailure = $_.Exception.Message }
finally {
    if (-not (Stop-OwnedProcess -Process $videoProcess -Role host)) { $videoFailure += ' Owned startup process cleanup failed.' }
    $env:PVZ_COOP_SMOKE_STARTUP_VIDEO = $videoOldFlag
    Ensure-Directory $videoRun
    $summary = [ordered]@{ status = $(if ($videoFailure) { 'fail' } else { 'pass' }); error = $videoFailure;
        test = 'startup-video'; scope = 'startup playback only; multiplayer Adventure1 not exercised'; run_id = $videoRunId;
        artifacts = $videoRun; source_files = $videoHashes; entry = $videoEntryProof; proof = $videoProof; frames = $videoFrames;
        playback_action = $videoPlaybackAction; input = $videoActionRecord; skip_response_ms = $videoSkipResponseMs;
        window_liveness = @($videoLiveness.ToArray()); heard_audio = 'Not tested';
        skip_workflow = $(if ($videoPlaybackAction -eq 'Escape') { if ($videoFailure) { 'Failed' } else { 'Passed' } } else { 'Not executed' }) }
    Write-Utf8NoBom -Path (Join-Path $videoRun 'summary.json') -Text ((ConvertTo-Json $summary -Depth 10) + [Environment]::NewLine)
}
Write-Host ('Startup video artifacts: ' + $videoRun)
if ($videoFailure) { throw $videoFailure }
Write-Host ('Startup video ' + $videoPlaybackAction + ' proof Passed; early renderer frames, requested outcome, event pumping and source hashes verified.')
