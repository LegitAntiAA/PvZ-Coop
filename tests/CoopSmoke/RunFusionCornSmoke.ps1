#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(90,3600)][int]$TimeoutSeconds=360,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$cornHostWidth=$HostWindowWidth; $cornClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $cornHostWidth -ClientWindowWidth $cornClientWidth
if ($HostWindowWidth -ne $cornHostWidth -or $ClientWindowWidth -ne $cornClientWidth) { throw 'Shared helper changed requested widths.' }
$cornRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
if ([string]::IsNullOrWhiteSpace($GameExe)) { $GameExe=Join-Path $cornRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
$GameExe=Get-NormalizedPath $GameExe; $cornGame=Split-Path -Parent $GameExe
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) { $ArtifactsRoot=Join-Path $cornRepo 'work\fusion-corn' }
$cornRunId=[Guid]::NewGuid().ToString('N'); $cornRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$cornRunId)
$cornHost=Join-Path $cornRoot 'host'; $cornClient=Join-Path $cornRoot 'client'
$cornHostLog=Join-Path $cornHost 'logs'; $cornClientLog=Join-Path $cornClient 'logs'
$cornHostProcess=$null; $cornClientProcess=$null; $cornPassed=$false; $cornError=''
$cornFrames=@(); $cornCommon=@(); $cornPost=@(); $cornExeSha=''; $cornStarted=[DateTime]::UtcNow; $cornEnvironment=@{}
foreach ($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_CORN','PVZ_COOP_SMOKE_FUSION_LOBBER','PVZ_COOP_SMOKE_FUSION_CHOMPER','PVZ_COOP_SMOKE_FUSION_DEATH','PVZ_COOP_SMOKE_FUSION_FIRE','PVZ_COOP_SMOKE_FUSION_COLD','PVZ_COOP_SMOKE_FUSION_GLOVE','PVZ_COOP_SMOKE_FUSION_PUFF_PEA','PVZ_COOP_SMOKE_FUSION_PRODUCER','PVZ_COOP_SMOKE_FUSION_SPLIT','PVZ_COOP_SMOKE_FUSION_NUT',
    'PVZ_COOP_SMOKE_FUSION_DOUBLE','PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND','PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER',
    'PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION','PVZ_COOP_SMOKE_FUSION_BASE_BATCH','PVZ_COOP_SMOKE_FUSION_CHERRY',
    'PVZ_COOP_SMOKE_FUSION_POTATO','PVZ_COOP_SMOKE_FUSION_WALLNUT','PVZ_COOP_SMOKE_FUSION_ALMANAC',
    'PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY','PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY','PVZ_COOP_SMOKE_MYSTERY_FAULT',
    'PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_FUSION_CASE','PVZ_COOP_SMOKE_FUSION_OBSERVER','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $cornEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-Corn([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-CornFrames([string]$Directory,[string]$Role,[int]$Width) {
    Add-Type -AssemblyName System.Drawing
    $required=@('chooser','plant28','plant1111','plant1113','plant1121','plant1124','payload53','payload54','payload55','payload56','payload57','payload66','payload67','payload70','butter400','hidden50','sun','complete')
    foreach ($name in $required) {
        $path=Join-Path $Directory ('fusion_corn_'+$name+'.bmp')
        Assert-Corn (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try {
            Assert-Corn ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual viewport dimensions mismatch.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for ($x=16;$x -lt $bitmap.Width;$x+=37) { for ($y=16;$y -lt $bitmap.Height;$y+=31) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
            Assert-Corn ($colors.Count -ge 8) ('Blank renderer frame: '+$path)
        } finally { $bitmap.Dispose() }
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try {
    foreach ($path in @($GameExe,(Join-Path $cornGame 'main.pak'),(Join-Path $cornGame 'SDL3.dll'))) {
        Assert-Corn (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)
    }
    $cornExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach ($path in @($cornHostLog,$cornClientLog,(Join-Path $cornHost 'appdata'),(Join-Path $cornClient 'appdata'))) { Ensure-Directory $path }
    if ($Port -eq 0) { $Port=Get-FreeLoopbackPort }
    foreach ($name in $cornEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'; $env:PVZ_COOP_SMOKE_FUSION_CORN='1'; $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach ($role in @('host','client')) {
        $root=if ($role -eq 'host') {$cornHost} else {$cornClient}
        $width=if ($role -eq 'host') {$cornHostWidth} else {$cornClientWidth}
        $nickname=if ($role -eq 'host') {'CornHost'} else {'CornClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) `
            -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Native shared Corn cohort: two hidden owned windows; '+$cornRoot)
    $cornHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $cornGame -ManifestPath (Join-Path $cornHost 'smoke.ini') -Role host -RunId $cornRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $cornRoot 'pids.json') -Executable $GameExe -HostProcess $cornHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $cornHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $cornHostProcess -Role host -TimeoutSeconds 120
    $cornClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $cornGame -ManifestPath (Join-Path $cornClient 'smoke.ini') -Role client -RunId $cornRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $cornRoot 'pids.json') -Executable $GameExe -HostProcess $cornHostProcess -ClientProcess $cornClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $cornHost 'result.json') -ClientResultPath (Join-Path $cornClient 'result.json') `
        -HostProcess $cornHostProcess -ClientProcess $cornClientProcess -HostLogDirectory $cornHostLog -ClientLogDirectory $cornClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Corn ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client' -and $results[0].protocol -eq 48 -and $results[1].protocol -eq 48) 'Peer identity/protocol48 mismatch.'
    foreach ($result in $results) {
        $samples=if ($result.role -eq 'host') {[int]$result.snapshotsSent} else {[int]$result.snapshotsApplied}
        Assert-Corn ($result.status -eq 'pass' -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and
            [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Not a strict full-intro native manifest run.'
    }
    $hostText=Get-CombinedLogText $cornHostLog;$clientText=Get-CombinedLogText $cornClientLog
    Assert-Corn ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual protocol48/Fusion handshake missing.'

    $expectedKinds=@(28,26,1111,26,1113,32,1121,26,1124)
    $expectedOwners=@(1,0,1,0,1,0,1,0,0)
    $expectedCosts=@(100,100,50,100,100,300,100,100,300)
    $expectedWallet=@(2400,2300,2250,2155,2055,1755,1655,1555,1255)
    $purchasesByRole=@();$projectilesByRole=@();$finalByRole=@();$pauseByRole=@()
    foreach ($pair in @(@('host',$hostText),@('client',$clientText))) {
        $role=[string]$pair[0];$text=[string]$pair[1]
        Assert-Corn ($text -match ('SMOKE_FUSION_CORN_POOL role='+$role+' eligible=\d+ target=10 quota0=5 quota1=5 ownFirst=1')) 'Live chooser/quotas/own-first proof missing.'
        $buys=@([regex]::Matches($text,('SMOKE_FUSION_CORN_PURCHASE role='+$role+' index=(\d+) kind=(\d+) id=(\d+) token=(\d+) owner=(\d+) packet=(\d+) parents=(\d+),(\d+) cost=(\d+) sun=(\d+) nativeSDL=1')))
        Assert-Corn ($buys.Count -eq 9) 'Exactly nine real SDL purchases required.'
        for($i=0;$i -lt 9;++$i) {
            $m=$buys[$i]
            Assert-Corn ([int]$m.Groups[1].Value -eq $i -and [int]$m.Groups[2].Value -eq $expectedKinds[$i] -and [int]$m.Groups[5].Value -eq $expectedOwners[$i] -and [int]$m.Groups[9].Value -eq $expectedCosts[$i] -and [int]$m.Groups[10].Value -eq $expectedWallet[$i]) 'Source purchase order/owner/price/canonical ledger differs.'
            Assert-Corn ([uint32]$m.Groups[3].Value -gt 0 -and [uint32]$m.Groups[4].Value -gt 0 -and [int]$m.Groups[6].Value -gt 0) 'Missing stable native actor/token/packet identity.'
        }
        $purchasesByRole+=,@($buys)
        $shots=@([regex]::Matches($text,('SMOKE_FUSION_CORN_PROJECTILE role='+$role+' kind=(\d+) payload=(\d+) id=(\d+) damage=(\d+) nativeType=(\d+) motion=(\d+) limit=(-?\d+) wallet=(\d+) hostPhysics=(\d+)')))
        foreach($payload in @(53,54,55,56,57,66,67,70)) {
            $seen=@($shots|Where-Object {[int]$_.Groups[2].Value -eq $payload})
            Assert-Corn ($seen.Count -gt 0) ('No actual '+$role+' native projectile '+$payload)
            foreach($m in $seen) {Assert-Corn ([int]$m.Groups[9].Value -eq $(if($role -eq 'host'){1}else{0})) 'Client falsely claims host physics.'}
        }
        $projectilesByRole+=,@($shots)
        foreach($kind in @(28,1111,1113,1121,1124)) {Assert-Corn ($text -match ('SMOKE_FUSION_CORN_COMBAT role='+$role+' kind='+$kind+' normal=\d+ alternate=\d+ sourceCounterparts=1')) 'Both actual source counterparts/combat barrier required.'}
        Assert-Corn ($text -match ('SMOKE_FUSION_CORN_BUTTER role='+$role+' id=\d+ real=(\d+) mirror=(\d+) visible=1 overRaw255=1')) 'Exact real butter counter beyond255 missing.'
        $b=[regex]::Match($text,('SMOKE_FUSION_CORN_BUTTER role='+$role+' id=\d+ real=(\d+) mirror=(\d+) visible=1 overRaw255=1'))
        Assert-Corn ([int]$b.Groups[1].Value -gt 255 -and [int]$b.Groups[1].Value -le 400 -and [int]$b.Groups[2].Value -le 400) 'Counter was not restored within the source range.'
        $hidden=[regex]::Match($text,('SMOKE_FUSION_CORN_HIDDEN50 role='+$role+' id=\d+ real=(\d+) mirror=(\d+) visible=0 positiveTimer=1'))
        Assert-Corn ($hidden.Success -and [int]$hidden.Groups[1].Value -gt 0 -and [int]$hidden.Groups[1].Value -le 50) 'Natural67 fresh half-second hidden butter missing.'
        $pause=[regex]::Match($text,('SMOKE_FUSION_CORN_PAUSED role='+$role+' tick=(\d+) butter=(\d+) stableMs=(\d+) exactStable=1'))
        Assert-Corn ($pause.Success -and [int]$pause.Groups[2].Value -gt 255 -and [int]$pause.Groups[3].Value -ge 1200) 'Real Escape pause/positive exact timer stability missing.'
        $pauseByRole+=,$pause
        Assert-Corn ($text -match ('SMOKE_FUSION_CORN_SUN_CREDIT role='+$role+' delta=5 wallet=2255 nativeCoin=1')) 'Actual Coin5 single credit missing.'
        $final=@([regex]::Matches($text,('SMOKE_FUSION_CORN_FINAL role='+$role+' id=(\d+) row=(\d+) HP=(\d+)')))
        Assert-Corn ($final.Count -eq 9) 'Canonical final nine native actors missing.';$finalByRole+=,@($final)
        Assert-Corn ($text -match ('SMOKE_FUSION_CORN_COMPLETE role='+$role+' kinds=28,1111,1113,1121,1124 purchases=9 initialSun=2500 totalCost=1250 collected=5 finalSun=1255 payloads=53,54,55,56,57,66,67,70 nativeBoard=1 secondWorld=0 actorWritesAfterSetup=0 rngSeeds=1 setupWalletWrites=1 sourceZombieAIUnported=1')) 'Strict source cohort completion missing.'
    }
    for($i=0;$i -lt 9;++$i) {foreach($g in 1..10) {Assert-Corn ($purchasesByRole[0][$i].Groups[$g].Value -eq $purchasesByRole[1][$i].Groups[$g].Value) 'Peer purchase/provenance/price mismatch.'}}
    foreach($payload in @(53,54,55,56,57,66,67,70)) {
        $ids=@($projectilesByRole[0]|Where-Object {[int]$_.Groups[2].Value -eq $payload}|ForEach-Object {$_.Groups[3].Value})
        $both=@($projectilesByRole[1]|Where-Object {[int]$_.Groups[2].Value -eq $payload -and $ids -contains $_.Groups[3].Value})
        Assert-Corn ($both.Count -gt 0) 'No shared real projectile NetId for a source payload.'
    }
    for($i=0;$i -lt 9;++$i) {foreach($g in 1..3) {Assert-Corn ($finalByRole[0][$i].Groups[$g].Value -eq $finalByRole[1][$i].Groups[$g].Value) 'Final authoritative actor state differs.'}}
    Assert-Corn ($pauseByRole[0].Groups[2].Value -eq $pauseByRole[1].Groups[2].Value) 'Paused exact native butter differs between peers.'
    Assert-Corn ($hostText -match 'SMOKE_FUSION_CORN_FIXTURE_CONTROL initialSun=2500 originalSun=500 seed=\d+ actorWritesAfterSetup=0 setupWalletWrites=1 ambientTimersSetupOnly=1') 'Initial setup adaptation not disclosed.'
    Assert-Corn ($clientText -match 'SMOKE_FUSION_CORN_NEGATIVE role=client target=\d+ FNS12=1 crcValid=1 exact50Raw255=1 rejected=1 wholeBoardBytesUnchanged=1 localDecoderOnly=1') 'Live Board raw255 mismatch rejection missing.'
    Assert-Corn ($clientText -match 'SMOKE_FUSION_CORN_SUN_CLICK role=client id=\d+ point=-?\d+,-?\d+ singleSDL=1') 'Real client native coin click missing.'
    $hostCredit=[regex]::Match($hostText,'SMOKE_FUSION_CORN_SUN_CREDIT role=host delta=5 wallet=2255 nativeCoin=1 id=(\d+)')
    $clientCredit=[regex]::Match($clientText,'SMOKE_FUSION_CORN_SUN_CREDIT role=client delta=5 wallet=2255 nativeCoin=1 id=(\d+)')
    Assert-Corn ($hostCredit.Success -and $clientCredit.Success -and $hostCredit.Groups[1].Value -eq $clientCredit.Groups[1].Value) 'Single credited physical native Coin5 differs between peers.'
    foreach($kind in @(28,1111,1113,1121,1124)) {
        Assert-Corn ($hostText -match ('SMOKE_FUSION_CORN_PHASE kind='+$kind+' alternate=0 counter=0 last=[^\s]+ duration=[^\s]+ state=NOTREADY fullClip=1')) 'Natural shooting-counter/full-clip cleanup missing.'
        if($kind -in @(28,1113,1121)) {Assert-Corn ($hostText -match ('SMOKE_FUSION_CORN_PHASE kind='+$kind+' alternate=1 counter=0 last=[^\s]+ duration=[^\s]+ state=NOTREADY fullClip=1')) 'Natural alternate-state full-clip cleanup missing.'}
    }
    foreach($pattern in @('kind=28 delta=20,0,0 normal=1 alternate=0','kind=28 delta=40,0,0 normal=0 alternate=1','kind=1111 delta=50,50,50 normal=1 alternate=1','kind=1113 delta=50,50,50 normal=1 alternate=0','kind=1113 delta=70,70,70 normal=0 alternate=1','kind=1121 delta=100,33,33 normal=1 alternate=0','kind=1121 delta=120,40,40 normal=0 alternate=1','kind=1124 delta=133,133,66 normal=1 alternate=1')) {
        Assert-Corn ($hostText -match ('SMOKE_FUSION_CORN_DAMAGE role=host '+[regex]::Escape($pattern)+' tick=\d+ noHPWrites=1 permutationInvariant=1')) 'Actual native HP multiset differs/missing.'
    }
    $primary=@([regex]::Matches($hostText,'Fusion native Lobber primary projectile=(\d+) payload=(\d+) target=(\d+) damage=(\d+) flags=1'))
    $sunEvents=@([regex]::Matches($hostText,'Fusion native Lobber sun projectile=(\d+) coin=(\d+) value=5 ground=(\d+)'))
    foreach($p in @(53,56,57,70)) {
        $required=if($p -eq 70){2}else{3};$matched=$false
        $groups=@($primary|Where-Object {[int]$_.Groups[2].Value -eq $p}|Group-Object {$_.Groups[1].Value})
        foreach($group in $groups) {
            $targets=@($group.Group|ForEach-Object {$_.Groups[3].Value}|Select-Object -Unique)
            Assert-Corn ($targets.Count -eq $group.Count) 'Same projectile damaged a native target twice directly.'
            if($group.Count -ne $required){continue}
            if($p -eq 53) {
                $coins=@($sunEvents|Where-Object {$_.Groups[1].Value -eq $group.Name})
                if($coins.Count -ne 4 -or @($coins|Where-Object {$_.Groups[3].Value -eq '1'}).Count -ne 1){continue}
                Assert-Corn (@($coins|ForEach-Object {$_.Groups[2].Value}|Select-Object -Unique).Count -eq 4) 'Coin callbacks reused a physical native coin identity.'
            }
            $matched=$true
        }
        Assert-Corn $matched ('No complete natural direct-pierce/ground sequence for payload'+$p)
    }
    $limit53=[regex]::Match($hostText,'SMOKE_FUSION_CORN_PROJECTILE role=host kind=1111 payload=53 id=\d+ damage=50 nativeType=\d+ motion=\d+ limit=6 wallet=(\d+)')
    Assert-Corn ($limit53.Success -and [int]$limit53.Groups[1].Value -ge 1000) 'Actual53 wallet-dependent captured limit6 missing.'
    $births=@([regex]::Matches($hostText,'Fusion native Lobber fire plant=\d+ kind=1113 projectile=(\d+) payload=(56|57) damage=\d+[^\r\n]* vx=([0-9.eE+\-]+) vz=([0-9.eE+\-]+) acc=([0-9.eE+\-]+)'))
    $firstHits=@([regex]::Matches($hostText,'Fusion native Lobber impact projectile=(\d+) payload=(56|57) primary=\d+ count=0 splash=0 age=(\d+)[^\r\n]* hits=1 limit=3 vx=([0-9.eE+\-]+) vz=([0-9.eE+\-]+)'))
    foreach($payload in @(56,57)) {
        $proved=$false;$culture=[Globalization.CultureInfo]::InvariantCulture
        foreach($hit in @($firstHits|Where-Object {[int]$_.Groups[2].Value -eq $payload})) {
            $birth=@($births|Where-Object {$_.Groups[1].Value -eq $hit.Groups[1].Value}|Select-Object -First 1)
            if(-not $birth.Count){continue}
            $vx=[double]::Parse($birth[0].Groups[3].Value,$culture);$vz=[double]::Parse($birth[0].Groups[4].Value,$culture)+[double]::Parse($birth[0].Groups[5].Value,$culture)*[int]$hit.Groups[3].Value
            if($vz -le 0){continue}
            Assert-Corn ([Math]::Abs([double]::Parse($hit.Groups[4].Value,$culture)-$vx*.5) -lt .003 -and [Math]::Abs([double]::Parse($hit.Groups[5].Value,$culture)+$vz*.5) -lt .003) 'Actual descending56/57 bounce differs from source half-X/half-reversed-Y.'
            $proved=$true
        }
        Assert-Corn $proved ('No actual descending first-hit bounce for payload'+$payload)
    }
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $cornCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_FUSION_CORN_COMBAT role=host kind=1124');$ci=$clientText.IndexOf('SMOKE_FUSION_CORN_COMBAT role=client kind=1124')
    Assert-Corn ($hi -ge 0 -and $ci -ge 0) 'Final combat/source frame barrier missing.'
    $tp=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$cp=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED)
    $cornPost=@($cp|Where-Object {$tp -contains $_}|Select-Object -Unique)
    Assert-Corn ($cornCommon.Count -ge 8 -and $cornPost.Count -ge 2) 'Common/native postaction snapshots insufficient.'
    $cornFrames=@((Assert-CornFrames $cornHost host $cornHostWidth),(Assert-CornFrames $cornClient client $cornClientWidth))
    Assert-Corn ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $cornExeSha) 'Executable changed during owned run.'
    $cornPassed=$true

} catch {$cornError=$_.Exception.Message;if ($_.ScriptStackTrace) {$cornError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Corn: '+$cornError)}
finally {
    foreach($name in $cornEnvironment.Keys) {[Environment]::SetEnvironmentVariable($name,$cornEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $cornClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $cornHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped) {$cornPassed=$false;$cornError+=' Owned cleanup failed.'}
    Ensure-Directory $cornRoot
    $summary=[ordered]@{status=$(if($cornPassed){'pass'}else{'fail'});error=$cornError;fixture='FusionNativeCorn';source_kinds=@(28,1111,1113,1121,1124);run_id=$cornRunId;
        started_utc=$cornStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$cornExeSha;protocol=48;native_schema=13;requested_host_width=$cornHostWidth;requested_client_width=$cornClientWidth;
        common_snapshot_hashes=$cornCommon;postaction_common_hashes=$cornPost;frames=$cornFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$cornRoot;
        initial_sun=2500;setup_wallet_writes=1;original_initial_sun=500;purchase_total=1250;real_coin_credit=5;final_sun=1255;initial_normal_targets=9;
        native_board=$true;second_world=$false;actor_writes_after_setup=0;fixture_rng_seeds=1;
        limits='Controlled standing native NORMAL NPCs/one initial setup, source zombie AI unported. All five plants crafted via real native packets; one active plant then native shovel/drain. Natural random normal/alternate volleys, exact HP deltas, distinct direct pierce targets and56/57 descending bounce, wallet-captured53limit6, native Coin5 hit/ground and one real isolated single SDL click/single physical Coin5 credit,400tick native butter across FNS raw255 cap, real Escape pause and local malformed-metadata/rawbody rejection, natural67 hidden50, source counter0/fullclip/alternate-state cleanup. No forced random outcome, no HP/timer/velocity/phase writes after setup. Runtime does not prove alternate probability distribution, all wallet limits, collider multiplicity of other NPCs, host migration, or source campaign.';
        visual_review='Root must inspect actual five source model/projectile/butter/coin frames. Nonblank BMP alone is not visual acceptance.'}
    Write-Utf8NoBom -Path (Join-Path $cornRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if($cornPassed){exit 0}else{exit 1}
