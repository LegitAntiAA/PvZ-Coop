#requires -Version 5.1
[CmdletBinding()]
param([string]$GameExe='', [string]$ArtifactsRoot='', [int]$TimeoutSeconds=240,[switch]$Victory,[switch]$DayResources,[switch]$SolarShots,[switch]$DayActors,[switch]$DayCombat,[switch]$PoleActors,[switch]$Lesson4,[switch]$Lesson4Victory,[switch]$Lesson78,[switch]$Puff280,[switch]$DayLimbs,[switch]$Adventure56,[switch]$FieldBiomes,[ValidateSet(800,1066)][int]$ClientWindowWidth=800)
if($Puff280 -and ($Lesson78 -or $Lesson4Victory -or $Lesson4 -or $FieldBiomes -or $Adventure56 -or $DayLimbs -or $PoleActors -or $DayActors -or $Victory -or $SolarShots -or $DayResources)){throw 'Puff280 is a separate native mechanism scenario.'}
if($DayCombat){$DayActors=$true}
if($Lesson78 -and ($Lesson4Victory -or $Lesson4 -or $FieldBiomes -or $Adventure56 -or $DayLimbs -or $PoleActors -or $DayActors -or $Victory -or $SolarShots -or $DayResources)){throw 'Lesson78 is a separate native source tutorial scenario.'}
if($Lesson78 -and $TimeoutSeconds -lt 540){$TimeoutSeconds=540}
if($Lesson4Victory){$Lesson4=$true;if($TimeoutSeconds -lt 1800){$TimeoutSeconds=1800}}
if($Adventure56 -and $TimeoutSeconds -lt 360){$TimeoutSeconds=360}
if($Adventure56 -and ($FieldBiomes -or $DayLimbs -or $Lesson4 -or $PoleActors -or $DayActors -or $Victory -or $SolarShots -or $DayResources)){throw 'Adventure56 is a separate source opening scenario.'}
if($FieldBiomes -and ($DayLimbs -or $Lesson4 -or $PoleActors -or $DayActors -or $Victory -or $SolarShots -or $DayResources)){throw 'FieldBiomes is a separate native presentation scenario.'}
if($DayLimbs -and ($Lesson4 -or $PoleActors -or $DayActors -or $Victory -or $SolarShots -or $DayResources)){throw 'DayLimbs is a separate native loss-event scenario.'}
if($DayLimbs -and $TimeoutSeconds -lt 720){$TimeoutSeconds=720}
if($Lesson4 -and ($PoleActors -or $DayActors -or $Victory -or $SolarShots -or $DayResources)){throw 'Lesson4 is a separate native tutorial scenario.'}
if($PoleActors -and ($DayActors -or $Victory -or $SolarShots -or $DayResources)){throw 'PoleActors is a separate native NPC scenario.'}
if($DayActors -and ($Victory -or $SolarShots -or $DayResources)){throw 'DayActors is a separate native NPC scenario.'}
if(($Victory -or $DayCombat) -and $TimeoutSeconds -lt 660){$TimeoutSeconds=660}
$sourceClientWidth273=$ClientWindowWidth
$sourceScenario273=if($Puff280){'SourcePuff280'}elseif($Lesson78){'SourceLesson78_278'}elseif($Lesson4Victory){'SourceLesson4Victory278'}elseif($FieldBiomes){'NativeField277'}elseif($Adventure56){'SourceAdventure56_277'}elseif($DayLimbs){'SourceDayLimbs276'}elseif($Lesson4){'SourceLesson4_275'}elseif($PoleActors){'SourcePole275'}elseif($DayCombat){'SourceDay275-Combat'}elseif($DayActors){'SourceDay274-NPC'}elseif($Victory){'SourceAdventure273-Victory'}elseif($SolarShots){'SourceAdventure273-Solar'}else{'SourceAdventure273-Opening'}
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
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_SOURCE273','PVZ_COOP_SMOKE_FUSION_SOURCE273_VICTORY','PVZ_COOP_SMOKE_FUSION_SOURCE273_SOLAR','PVZ_COOP_SMOKE_FUSION_DAY273_RESOURCES','PVZ_COOP_SMOKE_FUSION_DAY274','PVZ_COOP_SMOKE_FUSION_DAY274_COMBAT','PVZ_COOP_SMOKE_FUSION_POLE275','PVZ_COOP_SMOKE_FUSION_LESSON4_275','PVZ_COOP_SMOKE_FUSION_LESSON4_VICTORY278','PVZ_COOP_SMOKE_FUSION_LESSON78_278','PVZ_COOP_SMOKE_FUSION_LIMBS276','PVZ_COOP_SMOKE_FUSION_ADVENTURE56_277','PVZ_COOP_SMOKE_FUSION_FIELD277','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){
    $environment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
$environment['PVZ_COOP_SMOKE_FUSION_PUFF280']=[Environment]::GetEnvironmentVariable('PVZ_COOP_SMOKE_FUSION_PUFF280','Process')
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
        if($FieldBiomes){Add-Content -LiteralPath (Join-Path $root 'coop.ini') -Value ('fusion_classic_field='+$(if($role -eq 'host'){0}else{1})) -Encoding utf8}
        if($Adventure56){Add-Content -LiteralPath (Join-Path $root 'coop.ini') -Value @('fusion_source_completed=15','fusion_source_cycle=1','fusion_source_frontier=5') -Encoding utf8}
        if($Lesson78){Add-Content -LiteralPath (Join-Path $root 'coop.ini') -Value @('fusion_source_completed=63','fusion_source_cycle=1','fusion_source_frontier=7') -Encoding utf8}
        if($Puff280){Add-Content -LiteralPath (Join-Path $root 'coop.ini') -Value @('fusion_source_completed=511','fusion_source_cycle=1','fusion_source_frontier=10') -Encoding utf8}
        if($Lesson4){Add-Content -LiteralPath (Join-Path $root 'coop.ini') -Value @('fusion_source_completed=7','fusion_source_cycle=1','fusion_source_frontier=4') -Encoding utf8}
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname ('Source273'+$role) -AppDataDirectory $appDir -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory $logDir -ResultPath (Join-Path $root 'result.json') -SelectedPort $port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed 424242 -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    if(!$FieldBiomes){$env:PVZ_COOP_SMOKE_FUSION_SOURCE273='1'}
    if($DayActors){$env:PVZ_COOP_SMOKE_FUSION_DAY274='1'}
    if($DayCombat){$env:PVZ_COOP_SMOKE_FUSION_DAY274_COMBAT='1'}
    if($PoleActors){$env:PVZ_COOP_SMOKE_FUSION_POLE275='1'}
    if($Lesson4){$env:PVZ_COOP_SMOKE_FUSION_LESSON4_275='1'}
    if($Lesson4Victory){$env:PVZ_COOP_SMOKE_FUSION_LESSON4_VICTORY278='1'}
    if($Lesson78){$env:PVZ_COOP_SMOKE_FUSION_LESSON78_278='1'}
    if($Puff280){$env:PVZ_COOP_SMOKE_FUSION_PUFF280='1'}
    if($DayLimbs){$env:PVZ_COOP_SMOKE_FUSION_LIMBS276='1'}
    if($Adventure56){$env:PVZ_COOP_SMOKE_FUSION_ADVENTURE56_277='1'}
    if($FieldBiomes){$env:PVZ_COOP_SMOKE_FUSION_FIELD277='1'}
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
    if($results.Count -ne 2 -or @($results | Where-Object {$_.status -ne 'pass' -or $_.protocol -ne 65}).Count){throw 'Both source peers must pass using protocol65.'}
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
            $proof=if($Lesson78){'SMOKE_LESSON78_278_COMPLETE role='+$role+'\b'}elseif($Lesson4Victory){'SMOKE_LESSON4_VICTORY278_COMPLETE role='+$role+'\b'}elseif($FieldBiomes){'SMOKE_FIELD277_COMPLETE role='+$role+'\b'}elseif($Adventure56){'SMOKE_ADVENTURE56_277_COMPLETE role='+$role+'\b'}elseif($DayLimbs){'SMOKE_LIMBS276_COMPLETE role='+$role+'\b'}elseif($Lesson4){'SMOKE_LESSON4_275_COMPLETE role='+$role+'\b'}elseif($PoleActors){'SMOKE_POLE275_COMPLETE role='+$role+'\b'}elseif($DayCombat){'SMOKE_DAY275_COMPLETE role='+$role+'\b'}elseif($DayActors){'SMOKE_DAY274_COMPLETE role='+$role+'\b'}elseif($Victory){'SMOKE_SOURCE273_VICTORY_COMPLETE role='+$role}elseif($SolarShots){'SMOKE_SOURCE273_SOLAR_COMPLETE role='+$role+'\b[^\r\n]*kind=8 damage=100 sunOnHit=5 naturallyFired=1'}else{'SMOKE_SOURCE273_COMPLETE role='+$role+'\b[^\r\n]*sourceLevel=1 wave=1/10[^\r\n]*sourceFullCampaign=0'}
            if($Puff280){$proof='SMOKE_PUFF280_COMPLETE role='+$role+'\b'}
            if($log -notmatch $proof){throw ('Missing source scenario proof: '+$role)}
            if($Lesson78){
                foreach($sourceLevel in @(7,8)){
                    if([regex]::Matches($log,('SMOKE_LESSON78_278_CASE_COMPLETE role='+$role+' level='+$sourceLevel+'\b')).Count -ne 1){throw ('Missing or duplicate native lesson '+$sourceLevel+' on '+$role)}
                }
            }
            if($Adventure56){
                foreach($sourceLevel in @(5,6)){
                    if([regex]::Matches($log,('SMOKE_ADVENTURE56_277_LEVEL_COMPLETE role='+$role+' level='+$sourceLevel+'\b')).Count -ne 1){throw ('Missing or duplicate opening level '+$sourceLevel+' on '+$role)}
                }
            }
            if($FieldBiomes){
                if([regex]::Matches($log,('SMOKE_FIELD277_NODEPATHS role='+$role+' checks=34\b')).Count -ne 1){throw ('Missing actual native loader regression on '+$role)}
                foreach($biome in @('night','pool','day')){
                    if([regex]::Matches($log,('SMOKE_FIELD277_CASE role='+$role+' biome='+$biome+'\b')).Count -ne 1){throw ('Missing or duplicate field biome '+$biome+' on '+$role)}
                }
            }
            if($DayLimbs){
                foreach($cohort in 0..3){
                    if([regex]::Matches($log,('SMOKE_LIMBS276_COHORT_COMPLETE role='+$role+' cohort='+$cohort+'\b')).Count -ne 1){throw ('Missing or duplicate limb cohort '+$cohort+' on '+$role)}
                }
            }
            if($DayCombat){
                foreach($cohort in 0..2){
                    if([regex]::Matches($log,('SMOKE_DAY275_COHORT_COMPLETE role='+$role+' cohort='+$cohort+'\b')).Count -ne 1){throw ('Missing or duplicate combat cohort '+$cohort+' on '+$role)}
                }
            }
            if($DayResources -and $log -notmatch ('SMOKE_SOURCE273_DAY_RESOURCES_COMPLETE role='+$role+' bodies=8 factoryFX=4 actorAdmission=0')){throw ('Missing native Day resource proof: '+$role)}
            $frameNames=if($Lesson78){@('7_chooser','7_prepared','7_gift_held','7_gift_used','7_enemy','7_defense','7_complete','8_chooser','8_prepared','8_cards','8_dying','8_bucket_held','8_iron','8_complete')}elseif($Lesson4Victory){@('chooser','opening','prepared','defenses','held','moved','complete','battle','finalwave','trophy','trophy_clicked','next5_chooser')}elseif($FieldBiomes){@('night_chooser','night_source','pool_chooser','pool_water','pool_paused','pool_resumed','day_chooser','day_initial','day_client_fusion','day_host_native','day_complete')}elseif($Adventure56){@('level5_chooser','level5_initial','level5_purchases','level5_sun','level5_clienttool','level5_firstwaves','level5_paused','level6_chooser','level6_initial','level6_purchases','level6_sun','level6_clienttool','level6_firstwaves','level6_paused')}elseif($DayLimbs){@('case0_chooser','case0_cohort','case0_resource5033','case0_resource5035','case0_resource5037','case0_paused','case1_chooser','case1_cohort','case1_resource5063','case1_resource5064','case1_resource5065','case1_paused','case2_chooser','case2_cohort','case2_resource5059','case2_resource5011','case2_resource5010','case2_paused','case3_chooser','case3_cohort','case3_resource5059','case3_lethal','case3_paused')}elseif($Lesson4){@('chooser','opening','prepared','held','moved','complete')}elseif($PoleActors){@('chooser','cohort','jump','star','landed','paused')}elseif($DayCombat){@('combat0_chooser','combat0_start','combat0_chain21','combat0_chain22','combat0_chain23','combat0_chain2','combat1_chooser','combat1_start','combat1_bucket','combat2_chooser','combat2_start','combat2_armor107','combat2_hit11','combat2_bucket')}elseif($DayActors){@('chooser','cohort','moved','shots','paused')}elseif($Victory){@('chooser','initial','missions','defenses','gatling','battle','trophy','trophy_clicked','next2_chooser')}elseif($SolarShots){@('chooser','initial','missions','gatling','solar')}else{@('chooser','initial','missions','gatling','firstwave','paused')}
            if($Puff280){$frameNames=@('chooser','day_actor','puffs','paused')}
            foreach($suffix in $frameNames){
                $prefix=if($Lesson78){'fusion_lesson78_278_'}elseif($FieldBiomes){'fusion_field277_'}elseif($Adventure56){'fusion_adventure56_277_'}elseif($DayLimbs){'fusion_limbs276_'}elseif($Lesson4){'fusion_lesson4_275_'}elseif($PoleActors){'fusion_pole275_'}elseif($DayActors){'fusion_day274_'}else{'fusion_source273_'}
                if($Puff280){$prefix='fusion_puff280_'}
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
