#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = '',
    [string]$ArtifactsRoot = '',
    [ValidateRange(60, 3600)][int]$TimeoutSeconds = 180,
    [ValidateRange(0, 65535)][int]$Port = 0,
    [ValidateSet(800, 1066)][int]$HostWindowWidth = 800,
    [ValidateSet(800, 1066)][int]$ClientWindowWidth = 800,
    [uint32]$Seed = 424242
)

$ErrorActionPreference = 'Stop'
$nativeRequestedHostWidth = $HostWindowWidth
$nativeRequestedClientWidth = $ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed `
    -HostWindowWidth $nativeRequestedHostWidth -ClientWindowWidth $nativeRequestedClientWidth
if ($HostWindowWidth -ne $nativeRequestedHostWidth -or $ClientWindowWidth -ne $nativeRequestedClientWidth) {
    throw 'Shared smoke helper changed the requested native host/client viewport widths.'
}

$nativeRepo = Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe = Join-Path $nativeRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe = Get-NormalizedPath $GameExe
$nativeGameDirectory = Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot = Join-Path $nativeRepo 'work\fusion-native-world' }
$nativeRunId = [Guid]::NewGuid().ToString('N')
$nativeRunRoot = Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_' + $nativeRunId)
$nativeHostRoot = Join-Path $nativeRunRoot 'host'
$nativeClientRoot = Join-Path $nativeRunRoot 'client'
$nativeHostLog = Join-Path $nativeHostRoot 'logs'
$nativeClientLog = Join-Path $nativeClientRoot 'logs'
$nativeHostResult = Join-Path $nativeHostRoot 'result.json'
$nativeClientResult = Join-Path $nativeClientRoot 'result.json'
$nativeHostProcess = $null
$nativeClientProcess = $null
$nativePassed = $false
$nativeError = ''
$nativeHashes = @()
$nativeUiProof = @()
$nativeSunProof = @()
$nativeGridProof = @()
$nativeFieldProof = @()
$nativeStarted = [DateTime]::UtcNow
$nativePreviousGate = $env:PVZ_COOP_SMOKE_FUSION_NATIVE
$nativePreviousModes = $env:PVZ_COOP_SMOKE_GAME_MODES
$nativePreviousCase = $env:PVZ_COOP_SMOKE_FUSION_CASE
$nativePreviousObserver = $env:PVZ_COOP_SMOKE_FUSION_OBSERVER

function Assert-NativeResult {
    param([object]$Result, [string]$Role)
    if ([string]$Result.status -ne 'pass' -or [string]$Result.role -ne $Role -or
        [string]$Result.mode -ne 'fusion_validation' -or [string]$Result.scenario -ne 'fusion_validation' -or
        [int]$Result.level -ne 1 -or [uint32]$Result.seed -ne $Seed -or [bool]$Result.skipIntro -or [int]$Result.protocol -le 0) {
        throw ('Invalid native ' + $Role + ' result/manifest identity.')
    }
    $samples = if ($Role -eq 'host') { [int]$Result.snapshotsSent } else { [int]$Result.snapshotsApplied }
    if ($samples -lt 8) { throw ('Fewer than eight actual native ' + $Role + ' snapshots.') }
    return [int]$Result.protocol
}

function Assert-NativeFrames {
    param([string]$Directory, [string]$Role)
    Add-Type -AssemblyName System.Drawing
    $names = @('chooser', 'picks', 'ready_half', 'intro', 'playing', 'peashooter', 'fused', 'zombie_walk', 'headless', 'sun_collected', 'complete',
        'col9_planted', 'col9_fused', 'col9_shoveled', 'field_native', 'field_fusion', 'field_pressed', 'field_button',
        'field_independent_a', 'field_independent_b')
    if ($Role -eq 'host') { $names += @('shovel_held', 'col9_shovel_held', 'field_cancel_modal') }
    else { $names += @('client_card_held', 'col9_card_held') }
    $width = if ($Role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth }
    foreach ($name in $names) {
        $path = Join-Path $Directory ('fusion_native_' + $name + '.bmp')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing actual native renderer frame: ' + $path) }
        $bitmap = [Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne $width -or $bitmap.Height -ne 600) { throw ('Native frame does not match the requested SDL viewport: ' + $path) }
            $colors = New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x = 16; $x -lt $bitmap.Width; $x += 37) {
                for ($y = 16; $y -lt $bitmap.Height; $y += 31) { [void]$colors.Add($bitmap.GetPixel($x, $y).ToArgb()) }
            }
            if ($colors.Count -lt 8) { throw ('Native frame is blank/flat: ' + $path) }
        }
        finally { $bitmap.Dispose() }
    }
}

function Get-NativeValidatedDecoderText {
    param([string]$Text)
    foreach ($case in @('column10', 'row5')) {
        $pattern = '(?m)^[^\r\n]*SMOKE_FUSION_NATIVE_DECODER_BEGIN role=client case=' + $case +
            ' forgedSun=123456 checksumValid=1 localDecoder=1\r?\n' +
            '[^\r\n]*native Fusion snapshot preflight rejected epoch=\d+ bytes=\d+\r?\n' +
            '[^\r\n]*SMOKE_FUSION_NATIVE_DECODER_REJECT role=client case=' + $case +
            ' money=280 stableIds=1 stableSelection=1 stableCD=1 stableGhosts=1 applied=0 counterUnchanged=1 localDecoder=1\r?$'
        if ([regex]::Matches($Text, $pattern).Count -ne 1) { throw ('Missing isolated native decoder rollback proof: ' + $case) }
        # Only the exact expected three-line local regression is exempted.
        # All other native snapshot rejection/failure lines still fail below.
        $Text = [regex]::Replace($Text, $pattern, '')
    }
    return $Text
}

function Assert-NativeGrid {
    param([string]$HostText, [string]$ClientText)
    $identities = @()
    foreach ($role in @('host', 'client')) {
        $text = if ($role -eq 'host') { $HostText } else { $ClientText }
        $width = if ($role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth }
        $offset = ($width-800)/2
        if ($text -notmatch ('(?im)SMOKE_FUSION_NATIVE_GRID role=' + $role +
            ' columns=10 rows=5 pitch=72 left=40 right=760 top=80 bottom=580 roundtrips=50 tenthGrass=1 storageRows=6 rawMaxColumns=9 viewport=' +
            $width + 'x600 offset=' + $offset + ',0\b')) { throw ('Missing actual10x5 geometry/viewport proof: ' + $role) }
        foreach ($case in @('transport_bounds', 'right_gutter', 'bottom_gutter')) {
            $parameters = switch ($case) {
                'transport_bounds' { 'transport=1 column=10 row=5 x=-1 y=-1' }
                'right_gutter' { 'transport=0 column=-1 row=4 x=760 y=530' }
                'bottom_gutter' { 'transport=0 column=9 row=-1 x=724 y=580' }
            }
            if ($text -notmatch ('(?im)SMOKE_FUSION_NATIVE_GRID_REJECT role=' + $role + ' case=' + $case + ' ' + $parameters +
                ' sun=280 charge=0 recharge=0 stableIds=1 packet=\d+ samples=\d+\b')) { throw ('Missing unchanged native boundary case: ' + $role + '/' + $case) }
        }
        $parent = [regex]::Match($text, '(?im)SMOKE_FUSION_NATIVE_COL9_PLANT role=' + $role +
            ' id=(\d+) kind=1 column=9 row=4 x=684 y=480 center=724 owner=0 packet=(\d+) price=50 sun=230 sourceCD=750 nativeActor=1 nativeSDL=1 samples=\d+\b')
        $fused = [regex]::Match($text, '(?im)SMOKE_FUSION_NATIVE_COL9_FUSION role=' + $role +
            ' old=(\d+) id=(\d+) kind=1000 parents=1,0 column=9 row=4 hp=300 owner=1 origin=0 packet=(\d+) price=200 sun=30 sourceCD=750 nativeActor=1 nativeSDL=1 samples=\d+\b')
        $shovel = [regex]::Match($text, '(?im)SMOKE_FUSION_NATIVE_COL9_SHOVEL role=' + $role +
            ' removed=(\d+) original=(\d+) column=9 row=4 sun=30 nativeSDL=1 samples=\d+\b')
        if (-not $parent.Success -or -not $fused.Success -or -not $shovel.Success -or
            [int]$parent.Groups[1].Value -le 0 -or $parent.Groups[1].Value -ne $fused.Groups[1].Value -or
            $parent.Groups[1].Value -eq $fused.Groups[2].Value -or $fused.Groups[2].Value -ne $shovel.Groups[1].Value -or
            $shovel.Groups[1].Value -eq $shovel.Groups[2].Value) { throw ('Native tenth-column real actor/fusion/shovel identity proof failed: ' + $role) }
        $identities += [ordered]@{ role = $role; parent = $parent.Groups[1].Value; host_packet = $parent.Groups[2].Value;
            fused = $fused.Groups[2].Value; client_packet = $fused.Groups[3].Value; original = $shovel.Groups[2].Value;
            viewport = ($width.ToString()+'x600'); geometry = '10x5'; pitch = 72; boundary_cases = @('transport_col10', 'transport_row5', 'right_gutter', 'bottom_gutter') }
    }
    foreach ($key in @('parent', 'host_packet', 'fused', 'client_packet', 'original')) {
        if ($identities[0][$key] -ne $identities[1][$key]) { throw ('Native tenth-column host/client disagree: ' + $key) }
    }
    foreach ($case in @(@{ name='transport_col10'; column=10; row=4 }, @{ name='transport_row5'; column=9; row=5 })) {
        if ($ClientText -notmatch ('(?im)SMOKE_FUSION_NATIVE_GRID_SENT role=client case=' + $case.name + ' column=' + $case.column + ' row=' + $case.row +
            ' packet=' + $identities[1].client_packet + ' price=200 ready=1 sun=280\b') -or
            $HostText -notmatch ('(?im)Fusion native command slot=1 action=5 sequence=\d+ applied=0 column=' + $case.column + ' row=' + $case.row +
            ' packet=' + $identities[1].client_packet + '\b')) { throw ('Missing exact authenticated host bounds rejection: ' + $case.name) }
    }
    foreach ($case in @(@{ name='right_gutter'; x=760; y=530 }, @{ name='bottom_gutter'; x=724; y=580 })) {
        if ($ClientText -notmatch ('(?im)SMOKE_FUSION_NATIVE_GRID_MOUSE role=client case=' + $case.name + ' x=' + $case.x + ' y=' + $case.y +
            ' heldPacket=' + $identities[1].client_packet + ' nativeSDL=1\b')) { throw ('Missing real held-card SDL boundary click: ' + $case.name) }
    }
    return $identities
}

function Assert-NativeFields {
    param([string]$Text, [string]$Role, [string]$Directory, [object]$Grid)
    $rounds = @()
    foreach ($round in @(0, 1, 2)) {
        $skin = if (($round -eq 0 -and $Role -eq 'host') -or ($round -eq 1 -and $Role -eq 'client')) { 'native' } else { 'fusion' }
        $cursor = if ($Role -eq 'host') { 'shovel' } else { 'normal' }
        $pattern = '(?im)SMOKE_FUSION_NATIVE_FIELD role=' + $Role + ' round=' + $round + ' skin=' + $skin +
            ' id=' + $Grid.fused + ' original=' + $Grid.original + ' packet=' + $Grid.client_packet +
            ' sun=30 columns=10 pitch=72 persisted=1 beforeCD=(\d+) afterCD=(\d+) sourceCD=750 refreshing=([01]) stableIds=1 nativeFieldSDL=1 cursor=' +
            $cursor + ' freshPaintMs=300 samples=(\d+)\b'
        $matches = @([regex]::Matches($Text, $pattern))
        if ($matches.Count -ne 1) { throw ('Missing independent actual FIELD/persisted field round: ' + $Role + '/' + $round) }
        $record = $matches[0]
        if ($record.Groups[3].Value -eq '1' -and [int]$record.Groups[2].Value+3 -lt [int]$record.Groups[1].Value) {
            throw ('Live native cooldown restarted during field switch: ' + $Role + '/' + $round)
        }
        $snapshotMarker = if ($Role -eq 'host') { 'SMOKE_SNAPSHOT_TX' } else { 'SMOKE_SNAPSHOT_APPLIED' }
        $snapshot = [regex]::Match($Text, '(?im)' + $snapshotMarker + ' index=' + $record.Groups[4].Value + ' bytes=\d+ hash=([0-9a-f]{8})\b')
        if (-not $snapshot.Success -or ($Role -eq 'client' -and $nativeHashes -notcontains $snapshot.Groups[1].Value)) {
            throw ('Actual field frame lacks its host-authored native snapshot hash: ' + $Role + '/' + $round)
        }
        $rounds += [ordered]@{ round = $round; skin = $skin; before_cd = [int]$record.Groups[1].Value; after_cd = [int]$record.Groups[2].Value;
            frame_snapshot_index = [int]$record.Groups[4].Value; frame_snapshot_hash = $snapshot.Groups[1].Value }
    }
    $via = if ($Role -eq 'host') { 'modal' } else { 'focus' }
    if ($Text -notmatch ('(?im)SMOKE_FUSION_NATIVE_FIELD_CANCEL role=' + $Role + ' via=' + $via +
        ' interruptedDown=1 ownedUp=1 extraUp=1 toggle=0 stableIds=1 sun=30 sourceCD=750 noCDReset=1 nativeSDL=1\b')) {
        throw ('Interrupted/extra FIELD release did not preserve local state: ' + $Role)
    }
    $clicks = @([regex]::Matches($Text, '(?im)SMOKE_FUSION_NATIVE_FIELD_BUTTON role=' + $Role +
        ' round=([012]) x=(\d+) y=(\d+) width=117 height=46 belowMenu=1 parentBoard=1 hoverObserved=1 nativeSDL=1\b'))
    $clickRounds = if ($Role -eq 'host') { @(0, 1) } else { @(1, 2) }
    $viewportWidth = if ($Role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth }
    foreach ($round in $clickRounds) {
        $click = @($clicks | Where-Object { [int]$_.Groups[1].Value -eq $round })
        if ($click.Count -ne 1 -or [int]$click[0].Groups[2].Value+117 -gt $viewportWidth -or [int]$click[0].Groups[3].Value+46 -gt 80) {
            throw ('Missing real top-right native FIELD click/recovery: ' + $Role + '/' + $round)
        }
    }
    foreach ($skin in @('native', 'fusion')) {
        if ($Text -notmatch ('(?im)Fusion field preference saved skin=' + $skin + ' localOnly=1 columns=10 pitch=72\b')) {
            throw ('Real native FIELD button did not save each local field preference: ' + $Role + '/' + $skin)
        }
    }
    if ((Get-Content -LiteralPath (Join-Path $Directory 'coop.ini') -Raw) -notmatch '(?im)^fusion_classic_field=0\s*$') { throw 'Final local field preference was not restored/persisted.' }
    $native = [Drawing.Bitmap]::new((Join-Path $Directory 'fusion_native_field_native.bmp'))
    $fusion = [Drawing.Bitmap]::new((Join-Path $Directory 'fusion_native_field_fusion.bmp'))
    $changed = 0; $total = 0
    $offset = $(if ($Role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth })/2-400
    try {
        # Empty first-row grass excludes the two live plants, coins, HUD and
        # cursor, so actor animation cannot impersonate a background switch.
        for ($x = 90; $x -le 730; $x += 25) {
            for ($y = 96; $y -le 130; $y += 8) {
                $a = $native.GetPixel($x+$offset, $y); $b = $fusion.GetPixel($x+$offset, $y)
                if ([Math]::Abs($a.R-$b.R)+[Math]::Abs($a.G-$b.G)+[Math]::Abs($a.B-$b.B) -gt 24) { ++$changed }
                ++$total
            }
        }
        if ($changed -lt 12) { throw ('Actual field renderer frames do not show different empty-lawn textures: ' + $Role) }
    }
    finally { $native.Dispose(); $fusion.Dispose() }
    return [ordered]@{ role = $Role; rounds = $rounds; persisted = $true; opposite_peer_skins = $true;
        control = 'live_top_right_native_FIELD'; cancelled_press = $via; held_cursor_preserved = $cursor;
        unchanged_fused_id = $Grid.fused; unchanged_original_id = $Grid.original; changed_empty_lawn_samples = $changed; lawn_samples = $total }
}

function Get-NativePoolEvidence {
    param([string]$Text,[string]$Role,[string]$Prefix)
    $layouts=@([regex]::Matches($Text,($Prefix+'_POOL_LAYOUT role='+$Role+' eligible=(\d+) target=(\d+) quota0=(\d+) quota1=(\d+) mandatory=0,1,2,4 unfiltered=1')))
    if (-not $layouts.Count) { throw ('Missing explicit live source pool: '+$Role) }
    $eligible=[int]$layouts[0].Groups[1].Value; $target=[int]$layouts[0].Groups[2].Value
    $quota0=[int]$layouts[0].Groups[3].Value; $quota1=[int]$layouts[0].Groups[4].Value
    if ($target -ne [Math]::Min(10,$eligible) -or $quota0+$quota1 -ne $target -or $quota0 -lt 4 -or $quota1 -lt 4) { throw 'Observed live source pool has invalid native bank target/mandatory baseline quotas.' }
    foreach ($layout in $layouts) {
        if ([int]$layout.Groups[1].Value -ne $eligible -or [int]$layout.Groups[2].Value -ne $target -or
            [int]$layout.Groups[3].Value -ne $quota0 -or [int]$layout.Groups[4].Value -ne $quota1) { throw 'Fixture live pool changed across the activity/recovery.' }
    }
    $byIndex=@{}; $cards=@{}
    foreach ($row in [regex]::Matches($Text,($Prefix+'_ELIGIBLE role='+$Role+' index=(\d+) kind=(\d+) extra=([01]) baseCost=(\d+) sourceCD=(\d+) maxHP=(\d+) alias=(\d+)'))) {
        $index=[int]$row.Groups[1].Value
        $signature=($row.Groups | Select-Object -Skip 2 | ForEach-Object { $_.Value }) -join ':'
        if ($byIndex.ContainsKey($index) -and $byIndex[$index].signature -ne $signature) { throw 'Source definition changed at an existing pool index.' }
        $key=$row.Groups[2].Value+':'+$row.Groups[3].Value
        $card=[ordered]@{ index=$index; kind=[int]$row.Groups[2].Value; extra=[int]$row.Groups[3].Value;
            cost=[int]$row.Groups[4].Value; cd=[int]$row.Groups[5].Value; hp=[int]$row.Groups[6].Value; alias=[int]$row.Groups[7].Value; signature=$signature }
        if ($index -ge $eligible -or $card.hp -le 0 -or ($cards.ContainsKey($key) -and $cards[$key].index -ne $index)) { throw 'Observed source pool repeats an identity or contains invalid index/stats.' }
        $byIndex[$index]=$card; $cards[$key]=$card
    }
    if ($byIndex.Count -ne $eligible -or $cards.Count -ne $eligible) { throw 'Source pool evidence omits an eligible identity.' }
    $sourceBaseline=@{0=@(100,750);1=@(50,750);2=@(150,5000);4=@(25,3000)}
    foreach ($kind in $sourceBaseline.Keys) {
        foreach ($extra in @(0,1)) {
            $key=([string]$kind)+':'+$extra
            if (-not $cards.ContainsKey($key) -or $cards[$key].cost -ne $sourceBaseline[$kind][0]*($extra+1) -or
                $cards[$key].cd -ne $sourceBaseline[$kind][1]) { throw 'Mandatory0/1/2/4 source price/CD or variant coverage regressed.' }
        }
    }
    $signature=($eligible,$target,$quota0,$quota1 -join ',')+'|'+((0..($eligible-1) | ForEach-Object {
        if (-not $byIndex.ContainsKey($_)) { throw 'Observed source pool indices are not contiguous.' }; $byIndex[$_].signature
    }) -join '|')
    return [ordered]@{eligible=$eligible;target=$target;quotas=@($quota0,$quota1);cards=$cards;signature=$signature}
}

function Assert-NativeOwnerUi {
    param([string]$Text, [string]$Role, [int]$Viewer, [object]$Pool)
    $ownQuota=$Pool.quotas[$Viewer]
    $baseline = @{}
    foreach ($checkpoint in @('picks', 'ready_half', 'playing', 'fused', 'combat')) {
        $marker = '(?im)SMOKE_FUSION_NATIVE_OWNER_ORDER role=' + $Role + ' checkpoint=' + $checkpoint +
            ' viewer=' + $Viewer + ' ownPrefix=' + $ownQuota + ' ownFirst=1 sharedOwnerPolicy=1 stableIds=1 stableCD=1 chooser=' +
            $(if ($checkpoint -in @('picks', 'ready_half')) { '1' } else { '0' }) + ' samples=\d+'
        if ($Text -notmatch $marker) { throw ('Missing native owner-first checkpoint: ' + $Role + '/' + $checkpoint) }
        $pattern = '(?im)SMOKE_FUSION_NATIVE_PACKET_ORDER role=' + $Role + ' checkpoint=' + $checkpoint +
            ' index=(\d+) packet=(\d+) selection=(\d+) owner=(\d+) sourceCD=(\d+) liveCD=(\d+) refreshing=([01]) kind=(\d+) extra=([01])'
        $rows = @([regex]::Matches($Text, $pattern) | Sort-Object { [int]$_.Groups[1].Value })
        if ($rows.Count -ne $Pool.target) { throw ('Native physical owner order does not describe the observed target packets: ' + $Role + '/' + $checkpoint) }
        for ($index = 0; $index -lt $Pool.target; ++$index) {
            $row = $rows[$index]
            $id = $row.Groups[2].Value
            $selection = $row.Groups[3].Value
            $owner = [int]$row.Groups[4].Value
            $sourceCd = [int]$row.Groups[5].Value
            $liveCd = [int]$row.Groups[6].Value
            $kind = [int]$row.Groups[8].Value
            $extra = [int]$row.Groups[9].Value
            $identity=([string]$kind)+':'+$extra
            if (-not $Pool.cards.ContainsKey($identity)) { throw 'Native bank contains a card outside the full observed source pool.' }
            $expectedCd = $Pool.cards[$identity].cd
            $expectedOwner = if ($index -lt $ownQuota) { $Viewer } else { 1-$Viewer }
            if ([int]$row.Groups[1].Value -ne $index -or [int]$id -le 0 -or [int]$selection -le 0 -or
                $owner -ne $expectedOwner -or $extra -ne $owner -or
                $sourceCd -ne $expectedCd -or $liveCd -notin @(0, $sourceCd) -or
                ($row.Groups[7].Value -eq '1' -and $liveCd -ne $sourceCd)) {
                throw ('Native physical own-prefix or cooldown proof failed: ' + $Role + '/' + $checkpoint)
            }
            if ($checkpoint -eq 'picks') {
                if ($baseline.ContainsKey($id)) { throw 'Native packet order repeats an id.' }
                $baseline[$id] = [ordered]@{ selection = $selection; owner = $owner; source_cd = $sourceCd; kind = $kind; extra = $extra }
            } elseif (-not $baseline.ContainsKey($id) -or $baseline[$id].selection -ne $selection -or
                $baseline[$id].owner -ne $owner -or $baseline[$id].source_cd -ne $sourceCd -or
                $baseline[$id].kind -ne $kind -or $baseline[$id].extra -ne $extra) {
                throw ('Native bank changed stable identities/CD across snapshots: ' + $Role + '/' + $checkpoint)
            }
        }
    }
    $half = '(?im)SMOKE_FUSION_NATIVE_READY_HALF role=' + $Role +
        ' mask=1 label=1/2_ready sharedFont=1 sharedColors=1 localReady=' + $(if ($Role -eq 'host') { '1 hostFirst=1' } else { '0 hostCaptureSeen=1' }) + ' nativeSDL=1'
    if ($Text -notmatch $half) { throw ('Actual shared1/2 Ready label/font/colors were not observed: ' + $Role) }
    return [ordered]@{ role = $Role; viewer = $Viewer; own_prefix = $ownQuota; half_ready = $true;
        checkpoints = @('picks', 'ready_half', 'playing', 'fused', 'combat'); packets = $baseline }
}

function Assert-NativeSunPoints {
    param([string]$HostText, [string]$ClientText)
    foreach ($amount in @(5, 25)) {
        $points = @()
        foreach ($role in @('host', 'client')) {
            $text = if ($role -eq 'host') { $HostText } else { $ClientText }
            $pattern = '(?im)SMOKE_FUSION_NATIVE_SUN_POINT role=' + $role + ' id=(\d+) amount=' + $amount +
                ' x=(\d+) y=(\d+) hitCount=1 hitIds=(\d+) margin=(\d+) grounded=1 isolated=1 samples=\d+'
            $rows = @([regex]::Matches($text, $pattern))
            if ($rows.Count -ne 1) { throw ('Missing unique native isolated sun point: ' + $role + '/' + $amount) }
            $row = $rows[0]
            $id = $row.Groups[1].Value
            if ([int]$id -le 0 -or $id -ne $row.Groups[4].Value -or [int]$row.Groups[5].Value -lt 8) {
                throw ('Native duplicate-click point includes another Coin or lacks interpolation margin: ' + $role)
            }
            if ($text -notmatch ('(?im)SMOKE_FUSION_NATIVE_SUN_STATE role=' + $role + ' id=' + $id + ' amount=' + $amount + ' collecting=1 dead=0 present=1 sun=\d+') -or
                $text -notmatch ('(?im)SMOKE_FUSION_NATIVE_SUN_STATE role=' + $role + ' id=' + $id + ' amount=' + $amount + ' collecting=1 dead=1 present=0 sun=\d+') -or
                $text -notmatch ('(?im)SMOKE_FUSION_NATIVE_COLLECT role=' + $role + ' id=' + $id + ' amount=' + $amount + '\b')) {
                throw ('Native collection did not follow the agreed real Coin id/value through collection/removal: ' + $role)
            }
            $points += [ordered]@{ role = $role; id = $id; amount = $amount; x = [int]$row.Groups[2].Value;
                y = [int]$row.Groups[3].Value; margin = [int]$row.Groups[5].Value }
        }
        if ($points[0].id -ne $points[1].id -or $points[0].x -ne $points[1].x -or $points[0].y -ne $points[1].y) {
            throw 'Host/client did not agree on the same isolated native Coin and click point.'
        }
        $point = $points[1]
        if ($ClientText -notmatch ('(?im)SMOKE_FUSION_NATIVE_SUN_DOUBLE_CLICK role=client id=' + $point.id + ' amount=' + $amount +
            ' x=' + $point.x + ' y=' + $point.y + ' clicks=2 nativeSDL=1\b')) { throw 'Missing two actual SDL clicks at the agreed native sun point.' }
        $before = if ($amount -eq 5) { 250 } else { 255 }
        $after = $before+$amount
        foreach ($text in @($HostText, $ClientText)) {
            if ($text -notmatch ('(?im)SMOKE_FUSION_NATIVE_SUN_LEDGER role=\w+ before=' + $before + ' after=' + $after + ' target=' + $point.id + '\b')) {
                throw 'Native sun ledger did not record the exact single-value transition for the agreed Coin.'
            }
        }
        [ordered]@{ amount = $amount; id = $point.id; x = $point.x; y = $point.y; hit_count = 1;
            host_margin = $points[0].margin; client_margin = $points[1].margin; real_sdl_clicks = 2; collected_id_removed = $true }
    }
}

try {
    if (-not (Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw ('Game executable missing: ' + $GameExe) }
    foreach ($resource in @('main.pak', 'dependency.pak', 'SDL3.dll', 'fusion\pilot\peasunflower\animation.json', 'fusion\pilot\portrait-peasunflower\animation.json', 'fusion\fields\classic-day-10.png')) {
        if (-not (Test-Path -LiteralPath (Join-Path $nativeGameDirectory $resource) -PathType Leaf)) { throw ('Missing native runtime resource: ' + $resource) }
    }
    foreach ($directory in @($nativeHostLog, $nativeClientLog, (Join-Path $nativeHostRoot 'appdata'), (Join-Path $nativeClientRoot 'appdata'))) { Ensure-Directory $directory }
    if ($Port -eq 0) { $Port = Get-FreeLoopbackPort }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE = '1'
    $env:PVZ_COOP_SMOKE_GAME_MODES = $null
    $env:PVZ_COOP_SMOKE_FUSION_CASE = $null
    $env:PVZ_COOP_SMOKE_FUSION_OBSERVER = $null
    foreach ($role in @('host', 'client')) {
        $root = if ($role -eq 'host') { $nativeHostRoot } else { $nativeClientRoot }
        $nickname = if ($role -eq 'host') { 'NativeHost' } else { 'NativeClient' }
        $width = if ($role -eq 'host') { $HostWindowWidth } else { $ClientWindowWidth }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        $requestedWidth = if ($role -eq 'host') { $nativeRequestedHostWidth } else { $nativeRequestedClientWidth }
        if ((Get-Content -LiteralPath (Join-Path $root 'coop.ini') -Raw) -notmatch ('(?im)^window_width=' + $requestedWidth + '\s*$')) {
            throw ('Generated native ' + $role + ' config does not preserve the requested viewport width.')
        }
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname `
            -AppDataDirectory (Join-Path $root 'appdata') -ConfigPath (Join-Path $root 'coop.ini') `
            -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed `
            -SelectedTimeoutMs ($TimeoutSeconds * 1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native10x5 Board Fusion fixture; owned host' + $HostWindowWidth + '/client' + $ClientWindowWidth + '; artifacts: ' + $nativeRunRoot)
    $nativeHostProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $nativeGameDirectory `
        -ManifestPath (Join-Path $nativeHostRoot 'smoke.ini') -Role host -RunId $nativeRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $nativeRunRoot 'pids.json') -Executable $GameExe -HostProcess $nativeHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $nativeHostLog -Pattern ('(?im)host listening port=' + $Port + '\b') `
        -Process $nativeHostProcess -Role host -TimeoutSeconds 120
    $nativeClientProcess = Start-SmokeProcess -Executable $GameExe -WorkingDirectory $nativeGameDirectory `
        -ManifestPath (Join-Path $nativeClientRoot 'smoke.ini') -Role client -RunId $nativeRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $nativeRunRoot 'pids.json') -Executable $GameExe -HostProcess $nativeHostProcess -ClientProcess $nativeClientProcess
    $results = @(Wait-ForResults -HostResultPath $nativeHostResult -ClientResultPath $nativeClientResult `
        -HostProcess $nativeHostProcess -ClientProcess $nativeClientProcess -HostLogDirectory $nativeHostLog `
        -ClientLogDirectory $nativeClientLog -TimeoutSeconds $TimeoutSeconds)
    $protocols = @((Assert-NativeResult $results[0] host), (Assert-NativeResult $results[1] client))
    if ($protocols[0] -ne $protocols[1]) { throw 'Native fixture protocols differ.' }
    $hostText = Get-CombinedLogText $nativeHostLog
    $clientText = Get-CombinedLogText $nativeClientLog
    $validatedClientText = Get-NativeValidatedDecoderText $clientText
    foreach ($text in @($hostText, $validatedClientText)) {
        if ($text -match '(?im)\bSMOKE_FAIL\b|Access Violation|unhandled exception|assertion failed|native Fusion snapshot.*rejected|reason=sequence|Fusion start host epoch=|Fusion room|Fusion native field art unavailable') {
            throw 'Native fixture contains a failure, rejected snapshot, sequence gap or standalone-runtime evidence.'
        }
        foreach ($marker in @('CHOOSER', 'INTRO', 'PLAYING', 'PLANT', 'FUSION', 'STALE', 'ZOMBIE', 'SHOVEL', 'COMPLETE')) {
            if ($text -notmatch ('(?im)\bSMOKE_FUSION_NATIVE_' + $marker + '\b')) { throw ('Missing native checkpoint: ' + $marker) }
        }
        foreach ($eventName in @('bullet8', 'sun5', 'headless', 'death')) {
            if ($text -notmatch ('(?im)SMOKE_FUSION_NATIVE_COMBAT[^\r\n]*event=' + $eventName + '\b')) { throw ('Missing real native combat event: ' + $eventName) }
        }
    }
    $nativeHostPool=Get-NativePoolEvidence $hostText host SMOKE_FUSION_NATIVE
    $nativeClientPool=Get-NativePoolEvidence $clientText client SMOKE_FUSION_NATIVE
    if ($nativeHostPool.signature -ne $nativeClientPool.signature) { throw 'Native peers disagree on the full source eligibility/target/quotas.' }
    $nativeUiProof = @((Assert-NativeOwnerUi $hostText host 0 $nativeHostPool), (Assert-NativeOwnerUi $clientText client 1 $nativeClientPool))
    $nativeSunProof = @(Assert-NativeSunPoints $hostText $clientText)
    foreach ($id in $nativeUiProof[0].packets.Keys) {
        if (-not $nativeUiProof[1].packets.ContainsKey($id) -or
            $nativeUiProof[0].packets[$id].selection -ne $nativeUiProof[1].packets[$id].selection -or
            $nativeUiProof[0].packets[$id].owner -ne $nativeUiProof[1].packets[$id].owner -or
            $nativeUiProof[0].packets[$id].source_cd -ne $nativeUiProof[1].packets[$id].source_cd) {
            throw 'Host/client viewer-relative sorting disagrees on canonical packet identities/CD.'
        }
    }
    foreach ($pattern in @(
        '(?im)SMOKE_FUSION_NATIVE_PLANT[^\r\n]*id=(\d+) kind=0 sun=400',
        '(?im)SMOKE_FUSION_NATIVE_FUSION[^\r\n]*old=(\d+) id=(\d+) kind=1000 parents=0,1 hp=300 sun=300 owner=1 price=100',
        '(?im)SMOKE_FUSION_NATIVE_STALE[^\r\n]*target=(\d+) fused=(\d+) sun=250 charge=0 recharge=0',
        '(?im)SMOKE_FUSION_NATIVE_ZOMBIE[^\r\n]*id=(\d+) row=2 nativeActor=1',
        '(?im)SMOKE_FUSION_NATIVE_COLLECT[^\r\n]*id=(\d+) amount=5 before=250 after=255 doubleClickSingleCredit=1',
        '(?im)SMOKE_FUSION_NATIVE_COLLECT[^\r\n]*id=(\d+) amount=25 before=255 after=280 doubleClickSingleCredit=1',
        '(?im)SMOKE_FUSION_NATIVE_COMPLETE[^\r\n]*epoch=(\d+) fused=(\d+) col9=(\d+) sun=30'
    )) {
        $a = [regex]::Match($hostText, $pattern); $b = [regex]::Match($clientText, $pattern)
        if (-not $a.Success -or -not $b.Success) { throw ('Missing shared native identity proof: ' + $pattern) }
        for ($i = 1; $i -lt $a.Groups.Count; ++$i) { if ($a.Groups[$i].Value -ne $b.Groups[$i].Value) { throw 'Native host/client canonical identities differ.' } }
    }
    if ($hostText -notmatch '(?im)Fusion native command slot=1 action=5 sequence=\d+ applied=1\b' -or
        $hostText -notmatch '(?im)Fusion native command slot=1 action=5 sequence=\d+ applied=0\b' -or
        $hostText -notmatch '(?im)SMOKE_FUSION_NATIVE_SHOVEL_CANCEL role=host escape=1 right=1 worldUntouched=1 nativeSDL=1\b') {
        throw 'Missing actual host-authoritative native purchase, stale rejection or shovel cancellation proof.'
    }
    $tx = @(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX)
    $applied = @(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $nativeHashes = @($applied | Where-Object { $tx -contains $_ } | Select-Object -Unique)
    if ($nativeHashes.Count -lt 8) { throw 'Fewer than eight real client-applied native snapshot hashes match host transmission.' }
    Assert-NativeFrames $nativeHostRoot host
    Assert-NativeFrames $nativeClientRoot client
    $nativeGridProof = @(Assert-NativeGrid $hostText $clientText)
    $nativeFieldProof = @((Assert-NativeFields $hostText host $nativeHostRoot $nativeGridProof[0]),
        (Assert-NativeFields $clientText client $nativeClientRoot $nativeGridProof[1]))
    $nativePassed = $true
    Write-SmokeStatus ('PASS native Board Fusion: protocol ' + $protocols[0] + ', common native hash ' + $nativeHashes[0])
}
catch {
    $nativeError = $_.Exception.Message
    if ($_.ScriptStackTrace) { $nativeError += ' | ' + $_.ScriptStackTrace }
    Write-SmokeStatus ('FAIL native Board Fusion: ' + $nativeError)
}
finally {
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE = $nativePreviousGate
    $env:PVZ_COOP_SMOKE_GAME_MODES = $nativePreviousModes
    $env:PVZ_COOP_SMOKE_FUSION_CASE = $nativePreviousCase
    $env:PVZ_COOP_SMOKE_FUSION_OBSERVER = $nativePreviousObserver
    $clientStopped = Stop-OwnedProcess -Process $nativeClientProcess -Role client
    $hostStopped = Stop-OwnedProcess -Process $nativeHostProcess -Role host
    if (-not $clientStopped -or -not $hostStopped) { $nativePassed = $false; $nativeError += ' Owned process cleanup failed.' }
    Ensure-Directory $nativeRunRoot
    $summary = [ordered]@{
        status = $(if ($nativePassed) { 'pass' } else { 'fail' })
        fixture = 'FusionNativeBoard'; variant = 'Manual'; run_id = $nativeRunId
        started_utc = $nativeStarted.ToString('o'); finished_utc = [DateTime]::UtcNow.ToString('o')
        executable = $GameExe; mode = 'fusion_validation'; level = 1; seed = $Seed
        host_window = ($HostWindowWidth.ToString()+'x600'); client_window = ($ClientWindowWidth.ToString()+'x600'); native_grid = '10x5'
        common_snapshot_hashes = $nativeHashes; ui_regressions = $nativeUiProof; isolated_sun_collection = $nativeSunProof
        tenth_column = $nativeGridProof; local_fields = $nativeFieldProof; local_decoder_negative_cases = @('column10', 'row5')
        second_world = $false; source_campaign_complete = $false
        owned_cleanup = [bool]($clientStopped -and $hostStopped); error = $nativeError; artifacts_root = $nativeRunRoot
    }
    Write-Utf8NoBom -Path (Join-Path $nativeRunRoot 'summary.json') -Text (($summary | ConvertTo-Json -Depth 8) + [Environment]::NewLine)
}
if ($nativePassed) { exit 0 } else { exit 1 }
