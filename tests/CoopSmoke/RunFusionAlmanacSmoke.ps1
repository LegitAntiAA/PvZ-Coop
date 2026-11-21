#requires -Version 5.1
# One owned hidden host; strict FusionValidation manifest, local native book
# only. Requires NetSmokeUpdateFusionAlmanac before normal gameplay smoke hooks.
# Example: -GameExe <built exe> -WindowWidth 800. Captures require visual review.
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateSet(800, 1066)][int]$WindowWidth = 800,
    [ValidateRange(90, 600)][int]$TimeoutSeconds = 180,
    [ValidateRange(0, 65535)][int]$Port = 0,
    [uint32]$Seed = 424242
)

$ErrorActionPreference = 'Stop'
$almanacWidth = $WindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed `
    -HostWindowWidth $almanacWidth -ClientWindowWidth $almanacWidth
$almanacRepo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe = Join-Path $almanacRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$almanacGame = Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot = Join-Path $almanacRepo 'work\fusion\almanac-ui09' }
$almanacId = [Guid]::NewGuid().ToString('N')
$almanacRun = Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_' + $almanacId)
$almanacHost = Join-Path $almanacRun 'host'
$almanacLogs = Join-Path $almanacHost 'logs'
$almanacResult = Join-Path $almanacHost 'result.json'
$almanacProcess = $null
$almanacPassed = $false
$almanacError = ''
$almanacFrames = @()
$almanacAssetsBefore = @()
$almanacAssetsAfter = @()
$almanacAssetsUnchanged = $null
$almanacExeHash = ''
$almanacStarted = [DateTime]::UtcNow
$almanacEnvironment = @{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_ALMANAC', 'PVZ_COOP_SMOKE_FUSION_NATIVE',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY', 'PVZ_COOP_SMOKE_FUSION_CASE', 'PVZ_COOP_SMOKE_FUSION_OBSERVER',
    'PVZ_COOP_SMOKE_GAME_MODES', 'PVZ_COOP_SMOKE_NATIVE_UI', 'PVZ_COOP_SMOKE_HELP',
    'PVZ_COOP_SMOKE_STARTUP_VIDEO', 'PVZ_COOP_SMOKE_MYSTERY', 'PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_OVER', 'PVZ_COOP_SMOKE_COMMUNICATIONS', 'PVZ_COOP_SMOKE_DIFFICULTY')) {
    $almanacEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

function Assert-AlmanacAsset {
    param([string]$Path, [string]$Expected)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw ('Missing deployed almanac asset: ' + $Path) }
    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne $Expected.ToLowerInvariant()) { throw ('Almanac deployed source hash differs: ' + $Path) }
    return [ordered]@{ path = $Path; sha256 = $hash; bytes = (Get-Item -LiteralPath $Path).Length }
}

function Get-AlmanacAssetSnapshot {
    $source = Join-Path $almanacRepo 'assets\fusion'
    $runtime = Join-Path $almanacGame 'fusion'
    foreach ($relative in @('almanac\portraits\manifest.json', 'almanac\fonts\source_cjk.ttf')) {
        $sourceHash = (Get-FileHash -LiteralPath (Join-Path $source $relative) -Algorithm SHA256).Hash
        if ($relative -eq 'almanac\fonts\source_cjk.ttf' -and $sourceHash -ne 'ACBA71B3E7CDA61F032AFE38B6DC68115437E48FE3F5422EB5050277817FE8CF') { throw 'Source CJK font provenance changed.' }
        Assert-AlmanacAsset (Join-Path $runtime $relative) $sourceHash
    }
    $manifest = Get-Content -LiteralPath (Join-Path $source 'almanac\portraits\manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($group in @('plants', 'zombies')) {
        foreach ($property in $manifest.groups.$group.PSObject.Properties) {
            $row = $property.Value
            if ($row.status -eq 'ready' -and [bool]$row.publicAccepted) {
                $relative = [string]$row.package
                if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains(':') -or $relative -match '(^|[/\\])\.\.([/\\]|$)') { throw 'Nonportable almanac package path.' }
                Assert-AlmanacAsset (Join-Path (Join-Path $runtime 'almanac\portraits') $relative) ([string]$row.packageSha256)
            }
        }
    }
    foreach ($texture in $manifest.textures.PSObject.Properties) {
        $relative = [string]$texture.Name
        if ($relative -notmatch '^textures/[0-9a-f]{64}\.png$') { throw 'Invalid shared portrait texture identity.' }
        Assert-AlmanacAsset (Join-Path (Join-Path $runtime 'almanac\portraits') $relative) ([string]$texture.Value.sha256)
    }
    foreach ($batch in Get-ChildItem -LiteralPath (Join-Path $source 'catalog') -Filter 'almanac.ru.*.json' -File | Sort-Object Name) {
        Assert-AlmanacAsset (Join-Path (Join-Path $runtime 'catalog') $batch.Name) (Get-FileHash -LiteralPath $batch.FullName -Algorithm SHA256).Hash
    }
    # The three water-zombie cards reuse their own existing native models.
    # No copied portrait, alternate zombie, actor or animation clock is used.
    foreach ($id in @(11, 12, 13)) {
        $relative = 'pilot/zombie-batch-' + $id + '/animation.json'
        $modelSource = Join-Path $source $relative
        $modelRuntime = Join-Path $runtime $relative
        Assert-AlmanacAsset $modelRuntime (Get-FileHash -LiteralPath $modelSource -Algorithm SHA256).Hash
        $model = Get-Content -LiteralPath $modelSource -Raw -Encoding UTF8 | ConvertFrom-Json
        $textures = @($model.spriteVariants.PSObject.Properties | ForEach-Object { [string]$_.Value.texture } | Sort-Object -Unique)
        foreach ($texture in $textures) {
            Assert-AlmanacAsset (Join-Path (Split-Path -Parent $modelRuntime) $texture) `
                (Get-FileHash -LiteralPath (Join-Path (Split-Path -Parent $modelSource) $texture) -Algorithm SHA256).Hash
        }
    }
}

function Get-AlmanacRegionHash {
    param([Drawing.Bitmap]$Bitmap, [Drawing.Rectangle]$Region)
    if ($Region.X -lt 0 -or $Region.Y -lt 0 -or $Region.Right -gt $Bitmap.Width -or $Region.Bottom -gt $Bitmap.Height) { throw 'Almanac body viewport lies outside the actual renderer frame.' }
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        for ($y = $Region.Top; $y -lt $Region.Bottom; $y += 2) {
            for ($x = $Region.Left; $x -lt $Region.Right; $x += 2) { $writer.Write([int]$Bitmap.GetPixel($x, $y).ToArgb()) }
        }
        $writer.Flush()
        return [BitConverter]::ToString($sha.ComputeHash($stream.ToArray())).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose(); $writer.Dispose(); $stream.Dispose() }
}

function Assert-AlmanacFrames {
    param([string]$Text)
    Add-Type -AssemblyName System.Drawing
    $names = @('menu_classic', 'classic_index', 'classic_plants', 'classic_zombies', 'menu_fusion', 'fusion_index',
        'edge_plant_bottom', 'edge_plant_top', 'plant_1000', 'plant_ru', 'plant_new_ru', 'plant_925', 'long_top', 'long_bottom', 'long_drag_top',
        'edge_zombie_bottom', 'edge_zombie_top', 'portrait_7', 'runtime_11', 'runtime_12', 'runtime_13', 'missing_106', 'zombie_224', 'reopened_index', 'reopened', 'complete')
    $names += 1..8 | ForEach-Object { 'plants_p' + $_ }
    $names += 1..4 | ForEach-Object { 'zombies_p' + $_ }
    foreach ($name in $names) {
        $proof = [regex]::Match($Text, '(?im)SMOKE_ALMANAC_FRAME tag=' + [regex]::Escape($name) + ' bookX=(-?\d+) bookY=(-?\d+) noBoard=1 freshPaintMs=300 nativeSDL=1\b')
        if (-not $proof.Success) { throw ('Missing real renderer stage proof: ' + $name) }
        $fresh = [regex]::Match($Text, '(?im)SMOKE_ALMANAC_FRAME tag=' + [regex]::Escape($name) + '[^\r\n]* requestedDraw=(\d+) capturedDraw=(\d+)')
        if (-not $fresh.Success -or [int]$fresh.Groups[2].Value -lt [int]$fresh.Groups[1].Value) { throw ('Almanac frame did not read back its completed draw: ' + $name) }
        $path = Join-Path $almanacHost ('almanac_' + $name + '.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual almanac SDL frame: ' + $path) }
        $bitmap = [Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $almanacWidth -or $bitmap.Height -ne 600) { throw ('Unexpected almanac renderer dimensions: ' + $name) }
            $colors = [Collections.Generic.HashSet[int]]::new()
            for ($x = 15; $x -lt $bitmap.Width; $x += 31) {
                for ($y = 15; $y -lt $bitmap.Height; $y += 29) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) }
            }
            if ($colors.Count -lt 8) { throw ('Almanac renderer frame is blank/flat: ' + $name) }
            $bodyHash = $null
            if ($name -in @('long_top', 'long_bottom', 'long_drag_top')) {
                $region = [Drawing.Rectangle]::new([int]$proof.Groups[1].Value+484, [int]$proof.Groups[2].Value+315, 248, 233)
                $bodyHash = Get-AlmanacRegionHash $bitmap $region
            }
            [ordered]@{ tag = $name; path = $path; width = $bitmap.Width; height = $bitmap.Height;
                sampled_colors = $colors.Count; body_sha256 = $bodyHash; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
        }
        finally { $bitmap.Dispose() }
    }
}

try {
    if (-not (Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw ('Game executable missing: ' + $GameExe) }
    $almanacExeHash = (Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    $almanacAssetsBefore = @(Get-AlmanacAssetSnapshot)
    foreach ($name in $almanacEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_ALMANAC = '1'
    Ensure-Directory $almanacLogs
    Ensure-Directory (Join-Path $almanacHost 'appdata')
    if ($Port -eq 0) { $Port = Get-FreeLoopbackPort }
    Write-CoopConfig -Path (Join-Path $almanacHost 'coop.ini') -Nickname AlmanacHost -WindowWidth $almanacWidth -WindowHeight 600
    Write-SmokeManifest -Path (Join-Path $almanacHost 'smoke.ini') -Role host -Nickname AlmanacHost `
        -AppDataDirectory (Join-Path $almanacHost 'appdata') -ConfigPath (Join-Path $almanacHost 'coop.ini') `
        -LogDirectory $almanacLogs -ResultPath $almanacResult -SelectedPort $Port -SelectedMode fusion_validation `
        -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 1 -SelectedSkipIntro 0
    Write-SmokeStatus ('Almanac: one owned hidden ' + $almanacWidth + 'x600 window, native menu/book only; ' + $almanacRun)
    $almanacProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $almanacGame `
        -ManifestPath (Join-Path $almanacHost 'smoke.ini') -Role host -RunId $almanacId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $almanacRun 'pids.json') -Executable $GameExe -HostProcess $almanacProcess -ClientProcess $null
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not (Test-Path -LiteralPath $almanacResult -PathType Leaf)) {
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Almanac UI result timed out.' }
        if ($almanacProcess.HasExited) { throw ('Almanac host exited without result: ' + $almanacProcess.ExitCode) }
        if (-not (Test-OwnedProcessIdentity $almanacProcess)) { throw 'Almanac owned PID/path/start identity changed.' }
        Assert-NoLiveFailure -HostLogDirectory $almanacLogs -ClientLogDirectory $almanacLogs
        Start-Sleep -Milliseconds 200
    }
    $result = Get-Content -LiteralPath $almanacResult -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($result.status -ne 'pass' -or $result.role -ne 'host' -or $result.mode -ne 'fusion_validation' -or
        $result.scenario -ne 'fusion_validation' -or [int]$result.level -ne 1 -or [uint32]$result.seed -ne $Seed -or [bool]$result.skipIntro) {
        throw ('Almanac native fixture did not pass with the strict manifest identity: ' + ($result | ConvertTo-Json -Compress))
    }
    $text = Get-CombinedLogText $almanacLogs
    if ($text -match '(?im)\bSMOKE_FAIL\b|Access Violation|unhandled exception|assertion failed') { throw 'Almanac runtime failure; inspect host logs.' }
    foreach ($group in @('plants', 'zombies')) {
        $pageCount = if ($group -eq 'plants') { 8 } else { 4 }
        for ($page = 0; $page -lt $pageCount; ++$page) {
            if ($text -notmatch ('SMOKE_ALMANAC_PAGE group=' + $group + ' page=' + $page + ' firstId=\d+ nativeSDL=1')) { throw ('Missing real SDL page navigation: ' + $group + '/' + $page) }
        }
    }
    foreach ($id in @(1000,1192,925,1025)) {
        if ($text -notmatch ('SMOKE_ALMANAC_SELECT group=plants id=' + $id + ' page=\d+ expectedText=ru unsupported=0 noGameplayPreview=1 nativeSDL=1')) { throw ('Missing actual Russian Fusion plant selection: ' + $id) }
    }
    $missing = @([regex]::Matches($text,'SMOKE_ALMANAC_SELECT group=zombies id=(\d+) page=\d+ expectedText=ru unsupported=1 noGameplayPreview=1 nativeSDL=1') | ForEach-Object { [int]$_.Groups[1].Value } | Sort-Object -Unique)
    $manifest = Get-Content -LiteralPath (Join-Path $almanacRepo 'assets\fusion\almanac\portraits\manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $expectedMissing = @($manifest.groups.zombies.PSObject.Properties | Where-Object { ($_.Value.status -ne 'ready' -or -not $_.Value.publicAccepted) -and [int]$_.Value.id -notin @(11,12,13) } | ForEach-Object { [int]$_.Value.id } | Sort-Object)
    if ($missing.Count -ne 13 -or ($missing -join ',') -ne ($expectedMissing -join ',')) { throw 'The actual UI did not select all13 genuinely unavailable source zombie portraits.' }
    foreach ($pattern in @(
        'SMOKE_ALMANAC_RENDER group=plants id=1025 russian=1 portrait=1 runtimeModel=0 actualDraw=1',
        'SMOKE_ALMANAC_RENDER group=zombies id=7 russian=1 portrait=1 runtimeModel=0 actualDraw=1',
        'SMOKE_ALMANAC_RENDER group=zombies id=11 russian=1 portrait=1 runtimeModel=1 actualDraw=1',
        'SMOKE_ALMANAC_RENDER group=zombies id=12 russian=1 portrait=1 runtimeModel=1 actualDraw=1',
        'SMOKE_ALMANAC_RENDER group=zombies id=13 russian=1 portrait=1 runtimeModel=1 actualDraw=1',
        'SMOKE_ALMANAC_RENDER group=zombies id=106 russian=1 portrait=0 runtimeModel=0 actualDraw=1')) {
        if ($text -notmatch $pattern) { throw ('Missing actual Russian text/portrait draw proof: ' + $pattern) }
    }
    foreach ($pattern in @('SMOKE_ALMANAC_OPEN mode=classic route=dialog_api genuineMenu=1 noBoard=1',
        'SMOKE_ALMANAC_OPEN mode=fusion route=menu_native_SDL genuineMenu=1 noBoard=1',
        'SMOKE_ALMANAC_RUSSIAN plants=281 zombies=88 checked=369 cjk=0 sourceBound=1',
        'SMOKE_ALMANAC_SCROLL kind=wheel bottom=1', 'SMOKE_ALMANAC_SCROLL kind=thumb top=1',
        'SMOKE_ALMANAC_NAME id=925[^\r\n]*layoutFits=1',
        'SMOKE_ALMANAC_COMPLETE pagesPlants=8 pagesZombies=4 unsupportedSelected=13 boardCreated=0 reopened=1 name925Fits=1')) {
        if ($text -notmatch $pattern) { throw ('Missing native almanac acceptance proof: ' + $pattern) }
    }
    foreach ($tag in @('plants_first_bound', 'plants_last_bound', 'plants_empty_last', 'zombies_empty_last')) {
        if ($text -notmatch ('SMOKE_ALMANAC_BOUNDARY tag=' + $tag + ' selectionUnchanged=1 nativeSDL=1')) { throw ('Missing native disabled/empty card boundary proof: ' + $tag) }
    }
    $almanacFrames = @(Assert-AlmanacFrames $text)
    $top = ($almanacFrames | Where-Object tag -eq 'long_top').body_sha256
    $bottom = ($almanacFrames | Where-Object tag -eq 'long_bottom').body_sha256
    if ($top -eq $bottom) { throw 'Native wheel scrolling did not change actual rendered body pixels.' }
    $almanacAssetsAfter = @(Get-AlmanacAssetSnapshot)
    if (($almanacAssetsBefore | ConvertTo-Json -Depth 4 -Compress) -ne ($almanacAssetsAfter | ConvertTo-Json -Depth 4 -Compress) -or
        (Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -ne $almanacExeHash) { throw 'The owned almanac run changed its executable or deployed source assets.' }
    $almanacAssetsUnchanged = $true
    Request-OwnedProcessClose -Process $almanacProcess -Role host
    Wait-ForCleanProcessExit -Process $almanacProcess -Role host -TimeoutSeconds 15
    $almanacPassed = $true
}
catch { $almanacError = $_.Exception.Message; if ($_.ScriptStackTrace) { $almanacError += ' | ' + $_.ScriptStackTrace }; Write-SmokeStatus ('FAIL almanac: ' + $almanacError) }
finally {
    foreach ($name in $almanacEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $almanacEnvironment[$name], 'Process') }
    $stopped = Stop-OwnedProcess -Process $almanacProcess -Role host
    if (-not $stopped) { $almanacPassed = $false; $almanacError += ' Owned almanac process cleanup failed.' }
    Ensure-Directory $almanacRun
    $summary = [ordered]@{ status = $(if ($almanacPassed) { 'Passed' } else { 'Failed' }); task = 'FUSION-ALMANAC-271';
        local_ui_only = $true; network_synchronization_tested = $false; width = $almanacWidth; process_cleaned = [bool]$stopped;
        started_utc = $almanacStarted.ToString('o'); finished_utc = [DateTime]::UtcNow.ToString('o'); error = $almanacError;
        executable_sha256 = $almanacExeHash; frames = $almanacFrames; deployed_asset_snapshot = $almanacAssetsBefore; deployed_assets_unchanged = $almanacAssetsUnchanged;
        visual_review = 'Not executed by this runner; inspect Russian text, new7/runtime11..13/missing106 portraits and925-name frames.' }
    Write-Utf8NoBom -Path (Join-Path $almanacRun 'summary.json') -Text (($summary | ConvertTo-Json -Depth 8) + [Environment]::NewLine)
}
if (-not $almanacPassed) { throw ('Fusion almanac smoke Failed: ' + $almanacError + ' Artifacts: ' + $almanacRun) }
Write-SmokeStatus ('Fusion almanac smoke Passed; actual UI frames require visual review: ' + $almanacRun)
