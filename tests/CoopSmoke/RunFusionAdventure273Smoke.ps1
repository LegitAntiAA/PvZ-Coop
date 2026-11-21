#requires -Version 5.1
[CmdletBinding()]
param([string]$GameExe='', [string]$ArtifactsRoot='', [int]$TimeoutSeconds=240,[switch]$Victory,[switch]$DayResources,[switch]$SolarShots,[switch]$DayActors,[ValidateSet(800,1066)][int]$ClientWindowWidth=800)
if($DayActors -and ($Victory -or $SolarShots -or $DayResources)){throw 'DayActors is a separate native NPC scenario.'}
if($Victory -and $TimeoutSeconds -lt 660){$TimeoutSeconds=660}
$sourceClientWidth273=$ClientWindowWidth
$sourceScenario273=if($DayActors){'SourceDay274-NPC'}elseif($Victory){'SourceAdventure273-Victory'}elseif($SolarShots){'SourceAdventure273-Solar'}else{'SourceAdventure273-Opening'}
$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$GameExe){$GameExe=Join-Path $repoPath 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
if(!$ArtifactsRoot){$ArtifactsRoot=Join-Path $repoPath 'work\fusion\stage273\opening'}
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -HostWindowWidth 1066 -ClientWindowWidth $sourceClientWidth273
$GameExe=Get-NormalizedPath $GameExe
$gameDir=Split-Path -Parent $GameExe
$exeSha=(Get-FileHash -LiteralPath $GameExe).Hash
$runId=[Guid]::NewGuid().ToString('N')
$runDir=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$runId)
$hostDir=Join-Path $runDir 'host';$clientDir=Join-Path $runDir 'client'
$hostLogs=Join-Path $hostDir 'logs';$clientLogs=Join-Path $clientDir 'logs'
$hostProcess=$null;$clientProcess=$null;$failure='';$frames=@();$common=@();$environment=@{}
foreach($entry in @(Get-ChildItem Env: | Where-Object Name -like 'PVZ_COOP_SMOKE_FUSION_*')){$environment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_SOURCE273','PVZ_COOP_SMOKE_FUSION_SOURCE273_VICTORY','PVZ_COOP_SMOKE_FUSION_SOURCE273_SOLAR','PVZ_COOP_SMOKE_FUSION_DAY273_RESOURCES','PVZ_COOP_SMOKE_FUSION_DAY274','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){
    $environment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
try{
    if(@(Get-CimInstance Win32_Process -Filter "Name='PlantsVsZombies.exe'").Count){throw 'An existing game is running; it will not be touched.'}
    $port=Get-FreeLoopbackPort
    $soundCache=Join-Path $repoPath 'work\fusion\batch40-269\smoke-cached-sounds'
    $stamp=Join-Path $soundCache 'main-pak.sha256'
    $cacheValid=(Test-Path -LiteralPath $stamp) -and ([IO.File]::ReadAllText($stamp).Trim() -eq (Get-FileHash -LiteralPath (Join-Path $gameDir 'main.pak')).Hash)
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$hostDir}else{$clientDir}
        $width=if($role -eq 'host'){1066}else{$sourceClientWidth273}
        $appDir=Join-Path $root 'appdata';$logDir=Join-Path $root 'logs'
        Ensure-Directory $appDir;Ensure-Directory $logDir
        if($cacheValid){$destination=Join-Path $appDir 'cached';Ensure-Directory $destination;Copy-Item -LiteralPath (Join-Path $soundCache 'sounds') -Destination $destination -Recurse}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname ('Source273'+$role) -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname ('Source273'+$role) -AppDataDirectory $appDir -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory $logDir -ResultPath (Join-Path $root 'result.json') -SelectedPort $port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed 424242 -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_SOURCE273='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    if($DayActors){$env:PVZ_COOP_SMOKE_FUSION_DAY274='1'}
    if($Victory){$env:PVZ_COOP_SMOKE_FUSION_SOURCE273_VICTORY='1'}
    if($DayResources){$env:PVZ_COOP_SMOKE_FUSION_DAY273_RESOURCES='1'}
    if($SolarShots -and !$Victory){$env:PVZ_COOP_SMOKE_FUSION_SOURCE273_SOLAR='1'}
    Write-SmokeStatus ('SourceAdventure273 owned hidden pair: '+$runDir)
    $hostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDir -ManifestPath (Join-Path $hostDir 'smoke.ini') -Role host -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runDir 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $hostLogs -Pattern ('(?im)host listening port='+$port+'\b') -Process $hostProcess -Role host -TimeoutSeconds 120
    $clientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDir -ManifestPath (Join-Path $clientDir 'smoke.ini') -Role client -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runDir 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $clientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $hostDir 'result.json') -ClientResultPath (Join-Path $clientDir 'result.json') -HostProcess $hostProcess -ClientProcess $clientProcess -HostLogDirectory $hostLogs -ClientLogDirectory $clientLogs -TimeoutSeconds $TimeoutSeconds)
    if($results.Count -ne 2 -or @($results | Where-Object {$_.status -ne 'pass' -or $_.protocol -ne 62}).Count){throw 'Both source peers must pass using protocol62.'}
}catch{$failure=$_.Exception.Message}
finally{
    foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,$environment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $clientProcess -Role client
    $hostStopped=Stop-OwnedProcess -Process $hostProcess -Role host
    if(!$clientStopped -or !$hostStopped){$failure+=' Owned pair cleanup failed.'}
}
if(!$failure){
    try{
        $hostText=Get-CombinedLogText $hostLogs;$clientText=Get-CombinedLogText $clientLogs
        foreach($role in @('host','client')){
            $log=if($role -eq 'host'){$hostText}else{$clientText};$root=if($role -eq 'host'){$hostDir}else{$clientDir}
            if($log -match 'SMOKE_FAIL|native Fusion snapshot (encode|preflight) rejected|seq-gap|Adventure273 stopped'){throw ('Source rejection or failed input on '+$role)}
            $proof=if($DayActors){'SMOKE_DAY274_COMPLETE role='+$role+'\b'}elseif($Victory){'SMOKE_SOURCE273_VICTORY_COMPLETE role='+$role}elseif($SolarShots){'SMOKE_SOURCE273_SOLAR_COMPLETE role='+$role+'\b[^\r\n]*kind=8 damage=100 sunOnHit=5 naturallyFired=1'}else{'SMOKE_SOURCE273_COMPLETE role='+$role+'\b[^\r\n]*sourceLevel=1 wave=1/10[^\r\n]*sourceFullCampaign=0'}
            if($log -notmatch $proof){throw ('Missing source scenario proof: '+$role)}
            if($DayResources -and $log -notmatch ('SMOKE_SOURCE273_DAY_RESOURCES_COMPLETE role='+$role+' bodies=8 factoryFX=4 actorAdmission=0')){throw ('Missing native Day resource proof: '+$role)}
            $frameNames=if($DayActors){@('chooser','cohort','moved','shots','paused')}elseif($Victory){@('chooser','initial','missions','defenses','gatling','battle','trophy','trophy_clicked','next2_chooser')}elseif($SolarShots){@('chooser','initial','missions','gatling','solar')}else{@('chooser','initial','missions','gatling','firstwave','paused')}
            foreach($suffix in $frameNames){
                $prefix=if($DayActors){'fusion_day274_'}else{'fusion_source273_'}
                $frame=Join-Path $root ($prefix+$suffix+'.bmp')
                if(!(Test-Path -LiteralPath $frame -PathType Leaf)){throw ('Missing native frame '+$frame)}
                $frames+=$frame
            }
        }
        $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX)
        $rx=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
        $common=@($rx | Where-Object {$tx -contains $_} | Select-Object -Unique)
        if($common.Count -lt 16){throw 'Insufficient common source snapshots.'}
        if((Get-FileHash -LiteralPath $GameExe).Hash -ne $exeSha){throw 'Executable changed during source smoke.'}
    }catch{$failure=$_.Exception.Message}
}
Ensure-Directory $runDir
[ordered]@{status=$(if($failure){'fail'}else{'pass'});error=$failure;fixture=$sourceScenario273;sha256=$exeSha;common=$common.Count;frames=$frames;sourceFullCampaign=$false;owned_cleanup=[bool]($clientStopped -and $hostStopped)} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runDir 'summary.json') -Encoding utf8
if($failure){Write-SmokeStatus ('FAIL SourceAdventure273: '+$failure);exit 1}
Write-SmokeStatus ('PASS '+$sourceScenario273+', common='+$common.Count+' frames='+$frames.Count)
exit 0
