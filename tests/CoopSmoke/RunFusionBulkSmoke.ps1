#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe='', [string]$ArtifactsRoot='',
    [ValidateRange(120,3600)][int]$TimeoutSeconds=360,
    [ValidateRange(0,65535)][int]$Port=0,
    [ValidateSet(800,1066)][int]$HostWindowWidth=1066,
    [ValidateSet(800,1066)][int]$ClientWindowWidth=800,
    [uint32]$Seed=424242
)
$ErrorActionPreference='Stop'
$bulkHostWidth=$HostWindowWidth;$bulkClientWidth=$ClientWindowWidth
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot `
    -TimeoutSeconds $TimeoutSeconds -Port $Port -Seed $Seed -HostWindowWidth $bulkHostWidth -ClientWindowWidth $bulkClientWidth
if($HostWindowWidth -ne $bulkHostWidth -or $ClientWindowWidth -ne $bulkClientWidth){throw 'Shared helper changed requested widths.'}
$bulkRepo=Get-NormalizedPath (Join-Path $PSScriptRoot '..\..')
$bulkScopePath=Join-Path $bulkRepo 'work\fusion\batch40-scope.json'
$bulkScope=Get-Content -LiteralPath $bulkScopePath -Raw -Encoding UTF8|ConvertFrom-Json
$bulkKinds=@($bulkScope.groups.PSObject.Properties|ForEach-Object {$_.Value}|ForEach-Object {[int]$_})
if($bulkKinds.Count -ne 40 -or @($bulkKinds|Sort-Object -Unique).Count -ne 40){throw 'Bulk scope must contain exactly40 distinct source IDs.'}
if([string]::IsNullOrWhiteSpace($GameExe)){$GameExe=Join-Path $bulkRepo 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
$GameExe=Get-NormalizedPath $GameExe;$bulkGame=Split-Path -Parent $GameExe
if([string]::IsNullOrWhiteSpace($ArtifactsRoot)){$ArtifactsRoot=Join-Path $bulkRepo 'work\fusion-bulk'}
$bulkRunId=[Guid]::NewGuid().ToString('N');$bulkRoot=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$bulkRunId)
$bulkHost=Join-Path $bulkRoot 'host';$bulkClient=Join-Path $bulkRoot 'client'
$bulkHostLog=Join-Path $bulkHost 'logs';$bulkClientLog=Join-Path $bulkClient 'logs'
$bulkHostProcess=$null;$bulkClientProcess=$null;$bulkPassed=$false;$bulkError='';$bulkFrames=@();$bulkCommon=@();$bulkPost=@();$bulkExeSha=''
$bulkStarted=[DateTime]::UtcNow;$bulkEnvironment=@{}
foreach($entry in Get-ChildItem Env:|Where-Object {$_.Name -like 'PVZ_COOP_SMOKE_FUSION_*' -or $_.Name -in @('PVZ_COOP_SMOKE_MYSTERY_FAULT','PVZ_COOP_SMOKE_GAME_MODES','PVZ_COOP_SMOKE_STARTUP_VIDEO','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')}){$bulkEnvironment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_BULK','PVZ_COOP_SMOKE_FUSION_BULK_KINDS','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){if(-not $bulkEnvironment.ContainsKey($name)){$bulkEnvironment[$name]=$null}}
function Assert-Bulk([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Get-BulkActors([string]$Text,[string]$Role){
    $pattern='SMOKE_FUSION_BULK_ACTOR role='+$Role+' index=(\d+) kind=(\d+) id=(\d+) token=(\d+) alias=(\d+) HP=(\d+) max=(\d+) owner=(\d+) packet=(\d+) origin=(\d+) parents=(\d+),(\d+) column=(\d+) row=(\d+)'
    $rows=@([regex]::Matches($Text,$pattern));Assert-Bulk ($rows.Count -eq 40) ($Role+' requires exactly40 actual native actor records.')
    $ids=@{};$tokens=@{}
    for($i=0;$i -lt 40;++$i){
        $m=$rows[$i];$id=$m.Groups[3].Value;$token=$m.Groups[4].Value
        Assert-Bulk ([int]$m.Groups[1].Value -eq $i -and [int]$m.Groups[2].Value -eq $bulkKinds[$i] -and [uint32]$id -gt 0 -and [uint32]$token -gt 0 -and -not $ids.ContainsKey($id) -and -not $tokens.ContainsKey($token)) 'Scope/order/native actor identities differ.'
        $ids[$id]=$true;$tokens[$token]=$true
        Assert-Bulk ([int]$m.Groups[6].Value -gt 0 -and $m.Groups[6].Value -eq $m.Groups[7].Value -and [int]$m.Groups[8].Value -eq 255 -and [int]$m.Groups[9].Value -eq 0 -and [int]$m.Groups[10].Value -eq 0 -and [int]$m.Groups[13].Value -eq ($i%10) -and [int]$m.Groups[14].Value -eq [Math]::Floor($i/10)) 'Native source HP/provenance/cell mismatch.'
    }
    return $rows
}
function Assert-BulkFrames([string]$Directory,[string]$Role,[int]$Width){
    Add-Type -AssemblyName System.Drawing
    foreach($name in @('matrix_paused','matrix_live','money','complete')){
        $path=Join-Path $Directory ('fusion_bulk_'+$name+'.bmp');Assert-Bulk (Test-Path -LiteralPath $path -PathType Leaf) ('Missing actual renderer frame: '+$path)
        $bitmap=[Drawing.Bitmap]::new($path)
        try{
            Assert-Bulk ($bitmap.Width -eq $Width -and $bitmap.Height -eq 600) 'Actual native viewport dimensions differ.'
            $colors=New-Object 'System.Collections.Generic.HashSet[int]'
            for($x=16;$x -lt $bitmap.Width;$x+=37){for($y=16;$y -lt $bitmap.Height;$y+=31){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
            Assert-Bulk ($colors.Count -ge 8) 'Actual renderer frame is blank.'
        }finally{$bitmap.Dispose()}
        [ordered]@{role=$Role;frame=$name;path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
}
try{
    foreach($path in @($GameExe,(Join-Path $bulkGame 'main.pak'),(Join-Path $bulkGame 'SDL3.dll'))){Assert-Bulk (Test-Path -LiteralPath $path -PathType Leaf) ('Runtime input missing: '+$path)}
    $bulkExeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
    foreach($path in @($bulkHostLog,$bulkClientLog,(Join-Path $bulkHost 'appdata'),(Join-Path $bulkClient 'appdata'))){Ensure-Directory $path}
    if($Port -eq 0){$Port=Get-FreeLoopbackPort}
    foreach($name in $bulkEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_BULK='1';$env:PVZ_COOP_SMOKE_FUSION_BULK_KINDS=$bulkKinds -join ',';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$bulkHost}else{$bulkClient};$width=if($role -eq 'host'){$bulkHostWidth}else{$bulkClientWidth};$nickname=if($role -eq 'host'){'BulkHost'}else{'BulkClient'}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname $nickname -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname $nickname -AppDataDirectory (Join-Path $root 'appdata') `
            -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory (Join-Path $root 'logs') -ResultPath (Join-Path $root 'result.json') `
            -SelectedPort $Port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed $Seed -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    Write-SmokeStatus ('Bulk40 native factory/snapshot/render slice; two hidden owned windows; '+$bulkRoot)
    $bulkHostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $bulkGame -ManifestPath (Join-Path $bulkHost 'smoke.ini') -Role host -RunId $bulkRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $bulkRoot 'pids.json') -Executable $GameExe -HostProcess $bulkHostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $bulkHostLog -Pattern ('(?im)host listening port='+$Port+'\b') -Process $bulkHostProcess -Role host -TimeoutSeconds 120
    $bulkClientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $bulkGame -ManifestPath (Join-Path $bulkClient 'smoke.ini') -Role client -RunId $bulkRunId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $bulkRoot 'pids.json') -Executable $GameExe -HostProcess $bulkHostProcess -ClientProcess $bulkClientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $bulkHost 'result.json') -ClientResultPath (Join-Path $bulkClient 'result.json') -HostProcess $bulkHostProcess -ClientProcess $bulkClientProcess -HostLogDirectory $bulkHostLog -ClientLogDirectory $bulkClientLog -TimeoutSeconds $TimeoutSeconds)
    Assert-Bulk ($results.Count -eq 2 -and $results[0].role -eq 'host' -and $results[1].role -eq 'client') 'Actual paired results missing.'
    foreach($result in $results){
        $samples=if($result.role -eq 'host'){[int]$result.snapshotsSent}else{[int]$result.snapshotsApplied}
        Assert-Bulk ($result.status -eq 'pass' -and [int]$result.protocol -eq 48 -and $result.mode -eq 'fusion_validation' -and $result.scenario -eq 'fusion_validation' -and [int]$result.level -eq 1 -and [uint32]$result.seed -eq $Seed -and -not [bool]$result.skipIntro -and $samples -ge 8) 'Strict protocol48/full-intro native run failed.'
    }
    $hostText=Get-CombinedLogText $bulkHostLog;$clientText=Get-CombinedLogText $bulkClientLog
    Assert-Bulk ($hostText -match 'HELLO slot=1 protocol=48 expected=48' -and $clientText -match 'WELCOME assignedSlot=1[^\r\n]*contentMode=Fusion') 'Actual Fusion/protocol48 handshake missing.'
    $hostActors=@(Get-BulkActors $hostText host);$clientActors=@(Get-BulkActors $clientText client)
    for($i=0;$i -lt 40;++$i){foreach($g in 1..14){Assert-Bulk ($hostActors[$i].Groups[$g].Value -eq $clientActors[$i].Groups[$g].Value) ('Host/client actor metadata differs at index'+$i)}}
    $credits=@()
    foreach($pair in @(@('host',$hostText),@('client',$clientText))){
        $role=[string]$pair[0];$text=[string]$pair[1]
        Assert-Bulk ($text -match ('SMOKE_FUSION_BULK_MATRIX role='+$role+' actors=40 nativeBoard=1 secondWorld=0 resourcesInitialized=1 paused=1 localTick=\d+ stableMs=\d+ fires=\d+')) 'Actual paused matrix/resource proof missing.'
        Assert-Bulk ($text -match ('SMOKE_FUSION_BULK_LIVE role='+$role+' actors=40 sameGenerations=1 instantKindsStillAlive=1 fires=\d+')) 'Fresh live40 frame before natural instant removal missing.'
        foreach($probe in @(@(27,1,1184),@(27,31,1136),@(24,4,1190))) {
            Assert-Bulk ($text -match ('SMOKE_FUSION_BULK_RECIPE_REJECT role='+$role+' parent='+$probe[0]+' incoming='+$probe[1]+' result='+$probe[2]+' id=\d+ token=\d+ unchanged=1 readOnlyPolicy=1 paidCraft=0')) 'Unsupported directed recipe did not reject through the shared native placement policy.'
        }
        $credit=[regex]::Match($text,('SMOKE_FUSION_BULK_CREDIT role='+$role+' id=(\d+) delta=(\d+) money=(\d+) profileUnchanged=1 sunUnchanged=1'))
        Assert-Bulk ($credit.Success -and [uint32]$credit.Groups[1].Value -gt 0 -and [int]$credit.Groups[2].Value -gt 0) 'Actual single money coin credit missing.';$credits+=,$credit
        Assert-Bulk ($text -match ('SMOKE_FUSION_BULK_COMPLETE role='+$role+' actors=40 survivors=38 naturalInstantDeaths=2 crater=1 rowEffectsRetired=1 moneyCoin=\d+ moneyCredit=\d+ initialMoney=\d+ finalMoney=\d+ sourceZombieAIUnported=1 paidCrafts=0 postSetupActorWrites=0 all40GameplayProven=0')) 'Strict bounded bulk completion missing.'
    }
    foreach($g in 1..3){Assert-Bulk ($credits[0].Groups[$g].Value -eq $credits[1].Groups[$g].Value) 'Money physical identity/value/currency differs across peers.'}
    $creditEvent=[regex]::Match($hostText,'Fusion native Money credit coin=(\d+) value=(\d+) before=(\d+) after=(\d+)[^\r\n]*host=1 profile=0 sun=0')
    Assert-Bulk ($creditEvent.Success -and $creditEvent.Groups[1].Value -eq $credits[0].Groups[1].Value -and $creditEvent.Groups[2].Value -eq $credits[0].Groups[2].Value -and $creditEvent.Groups[4].Value -eq $credits[0].Groups[3].Value -and [int]$creditEvent.Groups[3].Value+[int]$creditEvent.Groups[2].Value -eq [int]$creditEvent.Groups[4].Value) 'Real source money callback does not support the observed credit.'
    $settledClicks=@([regex]::Matches($clientText,'SMOKE_FUSION_BULK_COIN_CLICK role=client candidate=\d+ point=-?\d+,-?\d+ singleSDL=1 settledSnapshots=(\d+) margin=8 ghost=[0-9.eE+\-,]+ target=[0-9.eE+\-,]+'))
    Assert-Bulk ($settledClicks.Count -eq 1 -and [int]$settledClicks[0].Groups[1].Value -ge 2) 'Exactly one settled/inset real client SDL money click required.'
    Assert-Bulk ($hostText -match 'SMOKE_FUSION_BULK_SETUP actors=40 initialFactoryOnly=1 paidCrafts=0 postSetupActorWrites=0 entryCallbacks=1 RNGReseeds=0') 'Initial factory control was not disclosed.'
    $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX);$ap=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
    $bulkCommon=@($ap|Where-Object {$tx -contains $_}|Select-Object -Unique)
    $hi=$hostText.IndexOf('SMOKE_FUSION_BULK_LIVE role=host');$ci=$clientText.IndexOf('SMOKE_FUSION_BULK_LIVE role=client')
    $tp=@(Get-SnapshotHashes -Text $hostText.Substring($hi) -Marker SMOKE_SNAPSHOT_TX);$cp=@(Get-SnapshotHashes -Text $clientText.Substring($ci) -Marker SMOKE_SNAPSHOT_APPLIED)
    $bulkPost=@($cp|Where-Object {$tp -contains $_}|Select-Object -Unique)
    Assert-Bulk ($bulkCommon.Count -ge 8 -and $bulkPost.Count -ge 2) 'Insufficient actual common/postmatrix native snapshot hashes.'
    $bulkFrames=@((Assert-BulkFrames $bulkHost host $bulkHostWidth),(Assert-BulkFrames $bulkClient client $bulkClientWidth))
    Assert-Bulk ((Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash -eq $bulkExeSha) 'Owned executable changed during run.'
    $bulkPassed=$true
}catch{$bulkError=$_.Exception.Message;if($_.ScriptStackTrace){$bulkError+=' | '+$_.ScriptStackTrace};Write-SmokeStatus ('FAIL Bulk: '+$bulkError)}
finally{
    foreach($name in $bulkEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$bulkEnvironment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $bulkClientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $bulkHostProcess -Role host
    if(-not $clientStopped -or -not $hostStopped){$bulkPassed=$false;$bulkError+=' Owned cleanup failed.'}
    Ensure-Directory $bulkRoot
    $summary=[ordered]@{status=$(if($bulkPassed){'pass'}else{'fail'});error=$bulkError;fixture='FusionNativeBulkFactory';source_kinds=$bulkKinds;scope_sha256=(Get-FileHash -LiteralPath $bulkScopePath -Algorithm SHA256).Hash;run_id=$bulkRunId;
        started_utc=$bulkStarted.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');executable=$GameExe;executable_sha256=$bulkExeSha;protocol=48;native_schema=13;requested_host_width=$bulkHostWidth;requested_client_width=$bulkClientWidth;
        common_snapshot_hashes=$bulkCommon;postmatrix_common_hashes=$bulkPost;frames=$bulkFrames;owned_cleanup=[bool]($clientStopped -and $hostStopped);artifacts_root=$bulkRoot;
        initial_controlled_factory_actors=40;paid_crafts=0;postsetup_actor_writes=0;all40_gameplay_proven=$false;source_zombie_AI_accepted=0;
        limits='One controlled initial40 real native AddPlant creations plus fresh-placement callbacks, real chooser/full intro, real Escape pause/resume, exact cross-peer identities/aliases/source HP/ancestry, actual live source-art frames, natural Doom/Jalapeno removal/crater/finite row FX, one physical money coin and one actual client SDL credit. No NPCs, no40 paid crafting or40 gameplay proof; recipient/equipment/layer workflows and special source dependencies remain pending.';
        visual_review='Root must inspect both actual live40 frames; nonblank BMP/resource initialization alone does not prove all80 source roles or correct art.'}
    Write-Utf8NoBom -Path (Join-Path $bulkRoot 'summary.json') -Text (($summary|ConvertTo-Json -Depth 8)+[Environment]::NewLine)
}
if($bulkPassed){exit 0}else{exit 1}
