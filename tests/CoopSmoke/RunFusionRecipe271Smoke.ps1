#requires -Version 5.1
[CmdletBinding()]
param([string]$GameExe='', [string]$ArtifactsRoot='', [int]$TimeoutSeconds=180, [switch]$CampaignWaves,
    [ValidateSet(0,21,31)][int]$VehicleLevel=0,[switch]$HealthBars)

if($HealthBars -and !$VehicleLevel){$VehicleLevel=21}

$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (!$GameExe) { $GameExe=Join-Path $repoPath 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe' }
if (!$ArtifactsRoot) { $ArtifactsRoot=Join-Path $repoPath 'work\fusion\batch40-269\recipe271' }
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background `
    -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -HostWindowWidth 1066

$GameExe=Get-NormalizedPath $GameExe
$gameDir=Split-Path -Parent $GameExe
$exeSha=(Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
$runId=[Guid]::NewGuid().ToString('N')
$runDir=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$runId)
$hostDir=Join-Path $runDir 'host'
$clientDir=Join-Path $runDir 'client'
$hostLogs=Join-Path $hostDir 'logs'
$clientLogs=Join-Path $clientDir 'logs'
$hostProcess=$null
$clientProcess=$null
$failure=''
$common=@()
$frames=@()
$environment=@{}
$caseName=if($CampaignWaves){'Waves271'}else{'Recipe271'}
if($VehicleLevel){$caseName='Vehicles272-Level'+$VehicleLevel}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_PAID_BATCH',
        'PVZ_COOP_SMOKE_FUSION_RECIPE271','PVZ_COOP_SMOKE_FUSION_CAMPAIGN270',
        'PVZ_COOP_SMOKE_FUSION_WAVES271','PVZ_COOP_SMOKE_FUSION_WAVES272','PVZ_COOP_SMOKE_FUSION_HP272','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')) {
    $environment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}

try {
    if (@(Get-CimInstance Win32_Process -Filter "Name='PlantsVsZombies.exe'").Count) {
        throw 'A game is running. The recipe smoke never touches an existing process.'
    }
    $port=Get-FreeLoopbackPort
    $soundCache=Join-Path $repoPath 'work\fusion\batch40-269\smoke-cached-sounds'
    $cacheStamp=Join-Path $soundCache 'main-pak.sha256'
    $cacheValid=(Test-Path -LiteralPath $cacheStamp) -and
        ([IO.File]::ReadAllText($cacheStamp).Trim() -eq (Get-FileHash -LiteralPath (Join-Path $gameDir 'main.pak')).Hash)
    foreach($role in @('host','client')) {
        $root=if($role -eq 'host'){$hostDir}else{$clientDir}
        $width=if($role -eq 'host'){1066}else{800}
        $appDir=Join-Path $root 'appdata'
        $logDir=Join-Path $root 'logs'
        Ensure-Directory $appDir
        Ensure-Directory $logDir
        if ($cacheValid) {
            $cacheDest=Join-Path $appDir 'cached'
            Ensure-Directory $cacheDest
            Copy-Item -LiteralPath (Join-Path $soundCache 'sounds') -Destination $cacheDest -Recurse
        }
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname ('Recipe271'+$role) -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname ('Recipe271'+$role) `
            -AppDataDirectory $appDir -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory $logDir `
            -ResultPath (Join-Path $root 'result.json') -SelectedPort $port -SelectedMode fusion_validation `
            -SelectedLevel 1 -SelectedSeed 424242 -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1'
    $env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    if($HealthBars){$env:PVZ_COOP_SMOKE_FUSION_HP272='1'}
    if ($CampaignWaves -or $VehicleLevel) {
        $env:PVZ_COOP_SMOKE_FUSION_CAMPAIGN270='1'
        $env:PVZ_COOP_SMOKE_FUSION_WAVES271='1'
        if($VehicleLevel){$env:PVZ_COOP_SMOKE_FUSION_WAVES271=$null;$env:PVZ_COOP_SMOKE_FUSION_WAVES272=[string]$VehicleLevel}
    } else {
        $env:PVZ_COOP_SMOKE_FUSION_PAID_BATCH='1'
        $env:PVZ_COOP_SMOKE_FUSION_RECIPE271='1'
    }
    Write-SmokeStatus ($caseName+' owned hidden pair: '+$runDir)
    $hostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDir -ManifestPath (Join-Path $hostDir 'smoke.ini') -Role host -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runDir 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $hostLogs -Pattern ('(?im)host listening port='+$port+'\b') -Process $hostProcess -Role host -TimeoutSeconds 120
    $clientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDir -ManifestPath (Join-Path $clientDir 'smoke.ini') -Role client -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runDir 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $clientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $hostDir 'result.json') -ClientResultPath (Join-Path $clientDir 'result.json') `
        -HostProcess $hostProcess -ClientProcess $clientProcess -HostLogDirectory $hostLogs -ClientLogDirectory $clientLogs -TimeoutSeconds $TimeoutSeconds)
    if ($results.Count -ne 2 -or @($results | Where-Object status -ne pass).Count) { throw 'Both owned peers must complete the recipe scenario.' }
} catch { $failure=$_.Exception.Message }
finally {
    foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,$environment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $clientProcess -Role client
    $hostStopped=Stop-OwnedProcess -Process $hostProcess -Role host
    if (!$clientStopped -or !$hostStopped) { $failure+=' Owned pair cleanup failed.' }
}

if (!$failure) {
    try {
        $hostText=Get-CombinedLogText $hostLogs
        $clientText=Get-CombinedLogText $clientLogs
        foreach($role in @('host','client')) {
            $roleText=if($role -eq 'host'){$hostText}else{$clientText}
            $roleDir=if($role -eq 'host'){$hostDir}else{$clientDir}
            if ($roleText -match 'SMOKE_FAIL|native Fusion snapshot (encode|preflight) rejected|seq-gap') {
                throw ('Invalid snapshot or failed input on '+$role)
            }
            if($VehicleLevel){
                $proof='SMOKE_FUSION_VEHICLES272_COMPLETE role='+$role+' level='+$VehicleLevel+' mask=0f nativeBirthPosition=1 postSetupActorWrites=0'
                $frameNames=@('fusion_campaign270_g0_vehicles.bmp')
                if($HealthBars){
                    if($roleText -notmatch ('SMOKE_HP272_COMPLETE role='+$role+' hybridMask=03 friendlyMask=03 showHealth=1 paused=1 actualHostTab=1')){throw 'Missing healthbar/Tab replication proof'}
                    if($roleText -notmatch ('SMOKE_MONEY272_COMPLETE role='+$role)){throw 'Missing unsupported Money action nonmutation proof'}
                    $frameNames+= 'fusion_campaign270_g0_healthbars.bmp'
                    if($role -eq 'client'){
                        if($roleText -notmatch 'Fusion native Money pointer rejected kind=1 reason=1 fee=0 money=3000 slot=1'){throw 'Missing actual Money rejection feedback'}
                        $frameNames+= 'fusion_campaign270_g0_moneyhint.bmp'
                    }
                }
            } elseif ($CampaignWaves) {
                $proof='SMOKE_FUSION_WAVES271_COMPLETE role='+$role+' level=11 regular15=[1-9]\d* flag104=[1-9]\d*'
                $frameNames=@('fusion_campaign270_g0_waves.bmp')
            } else {
                $proof='SMOKE_RECIPE271_COMPLETE role='+$role+' recipes=3 walker=[1-9]'
                $frameNames=@('scaredy','fume','hybrid','moving') | ForEach-Object { 'fusion_paid_2_'+$_+'.bmp' }
            }
            if ($roleText -notmatch $proof) {
                throw ('Missing advancing-zombie proof on '+$role)
            }
            foreach($frameName in $frameNames) {
                $frame=Join-Path $roleDir $frameName
                if (!(Test-Path -LiteralPath $frame -PathType Leaf)) { throw ('Missing SDL frame '+$frame) }
                $frames+=$frame
            }
        }
        $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX)
        $rx=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
        $common=@($rx | Where-Object {$tx -contains $_} | Select-Object -Unique)
        if ($common.Count -lt 16) { throw 'Insufficient common snapshots.' }
        if ((Get-FileHash -LiteralPath $GameExe).Hash -ne $exeSha) { throw 'Executable changed during recipe smoke.' }
    } catch { $failure=$_.Exception.Message }
}

Ensure-Directory $runDir
[ordered]@{status=$(if($failure){'fail'}else{'pass'});error=$failure;fixture=$caseName;sha256=$exeSha;common=$common.Count;frames=$frames;
    owned_cleanup=[bool]($clientStopped -and $hostStopped)} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runDir 'summary.json') -Encoding utf8
if ($failure) { Write-SmokeStatus ('FAIL '+$caseName+': '+$failure); exit 1 }
Write-SmokeStatus ('PASS '+$caseName+', common='+$common.Count+' frames='+$frames.Count)
exit 0
