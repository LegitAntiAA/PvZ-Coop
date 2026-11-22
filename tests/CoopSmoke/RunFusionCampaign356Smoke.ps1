#requires -Version 5.1
[CmdletBinding()]
param([string]$GameExe='', [string]$ArtifactsRoot='', [ValidateRange(600,1800)][int]$TimeoutSeconds=900,[int]$ExpectedProtocol=0,[ValidateSet(19,20,21,22,23,24,25,26,27,28,37)][int]$StartAtLevel=19)
$ErrorActionPreference='Stop'
$levels=@(19,20,21,22,23,24,25,26,27,28,37) | Where-Object {$_ -ge $StartAtLevel}
$completedFixture=([uint64]1 -shl ($StartAtLevel-1))-1
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$GameExe){$GameExe=Join-Path $repoPath 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
if(!$ExpectedProtocol){$header=Get-Content -LiteralPath (Join-Path $repoPath 'Lawn\Net\NetCommon.h') -Raw;$version=[regex]::Match($header,'NET_PROTOCOL_VERSION\s*=\s*(\d+)');if(!$version.Success){throw 'Cannot read expected protocol.'};$ExpectedProtocol=[int]$version.Groups[1].Value}
if(!$ArtifactsRoot){$ArtifactsRoot=Join-Path $repoPath 'work\fusion\stage356\campaign'}
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -HostWindowWidth 1066 -ClientWindowWidth 800
$GameExe=Get-NormalizedPath $GameExe
$gameDir=Split-Path -Parent $GameExe
$exeSha=(Get-FileHash -LiteralPath $GameExe).Hash
$runId=[Guid]::NewGuid().ToString('N')
$runDir=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$runId)
$hostDir=Join-Path $runDir 'host';$clientDir=Join-Path $runDir 'client'
$hostLogs=Join-Path $hostDir 'logs';$clientLogs=Join-Path $clientDir 'logs'
$hostProcess=$null;$clientProcess=$null;$failure='';$frames=@();$common=@();$environment=@{}
foreach($entry in @(Get-ChildItem Env: | Where-Object Name -like 'PVZ_COOP_SMOKE_FUSION_*')){$environment[$entry.Name]=$entry.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_SOURCE273','PVZ_COOP_SMOKE_FUSION_CAMPAIGN356','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){
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
        $width=if($role -eq 'host'){1066}else{800}
        $appDir=Join-Path $root 'appdata';$logDir=Join-Path $root 'logs'
        Ensure-Directory $appDir;Ensure-Directory $logDir
        if($cacheValid){$destination=Join-Path $appDir 'cached';Ensure-Directory $destination;Copy-Item -LiteralPath (Join-Path $soundCache 'sounds') -Destination $destination -Recurse}
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname ('C356'+$role) -WindowWidth $width -WindowHeight 600
        Add-Content -LiteralPath (Join-Path $root 'coop.ini') -Value @(('fusion_source_completed='+$completedFixture),'fusion_source_cycle=1',('fusion_source_frontier='+$StartAtLevel)) -Encoding utf8
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname ('C356'+$role) -AppDataDirectory $appDir -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory $logDir -ResultPath (Join-Path $root 'result.json') -SelectedPort $port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed 424242 -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 8 -SelectedSkipIntro 0
    }
    foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $environment['PVZ_COOP_SMOKE_FUSION_CAMPAIGN356_FIRST']=[Environment]::GetEnvironmentVariable('PVZ_COOP_SMOKE_FUSION_CAMPAIGN356_FIRST','Process');$env:PVZ_COOP_SMOKE_FUSION_CAMPAIGN356_FIRST=[string]$StartAtLevel;$env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_SOURCE273='1';$env:PVZ_COOP_SMOKE_FUSION_CAMPAIGN356='1';$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1'
    Write-SmokeStatus ('Campaign356 owned hidden pair: '+$runDir)
    $hostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDir -ManifestPath (Join-Path $hostDir 'smoke.ini') -Role host -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runDir 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $hostLogs -Pattern ('(?im)host listening port='+$port+'\b') -Process $hostProcess -Role host -TimeoutSeconds 120
    $clientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDir -ManifestPath (Join-Path $clientDir 'smoke.ini') -Role client -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runDir 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $clientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $hostDir 'result.json') -ClientResultPath (Join-Path $clientDir 'result.json') -HostProcess $hostProcess -ClientProcess $clientProcess -HostLogDirectory $hostLogs -ClientLogDirectory $clientLogs -TimeoutSeconds $TimeoutSeconds)
    if($results.Count -ne 2 -or @($results | Where-Object {$_.status -ne 'pass' -or $_.protocol -ne $ExpectedProtocol}).Count){throw 'Both Campaign peers must pass using the expected protocol.'}
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
            $root=if($role -eq 'host'){$hostDir}else{$clientDir}
            $log=if($role -eq 'host'){$hostText}else{$clientText}
            if($log -notmatch ('SMOKE_CAMPAIGN356_COMPLETE role='+$role+' cases='+$levels.Count+' firstLevel='+$StartAtLevel+'\b')){throw ('Missing Campaign356 completion on '+$role)}
            $epochs=@()
            foreach($level in $levels){
                $proof=[regex]::Matches($log,('SMOKE_CAMPAIGN356_CASE_COMPLETE role='+$role+' level='+$level+' epoch=(\d+) hash=([0-9a-f]{8})\b'))
                if($proof.Count -ne 1){throw ('Missing/duplicate local case proof '+$level+' on '+$role)}
                $epochs+=$proof[0].Groups[1].Value
                $peerLog=if($role -eq 'host'){$clientText}else{$hostText}
                if($peerLog -notmatch ('SMOKE_CAMPAIGN356_CASE_COMPLETE role=\w+ level='+$level+' epoch='+$proof[0].Groups[1].Value+' hash='+$proof[0].Groups[2].Value+'\b')){throw ('Canonical case proof differs for '+$level)}
                if($log -notmatch ('SMOKE_CAMPAIGN356_CLIENT_USE role='+$role+' level='+$level+' packet=[1-9]\d* kind=\d+ id=[1-9]\d* token=[1-9]\d*')){throw ('Missing actual client use '+$level+' on '+$role)}
                foreach($suffix in @('boot','proof')){
                    $frame=Join-Path $root ('fusion_campaign356_level'+$level+'_'+$suffix+'.bmp')
                    if(!(Test-Path -LiteralPath $frame -PathType Leaf)){throw ('Missing actual native frame '+$frame)}
                    $frames+=$frame
                }
            }
            if(@($epochs | Select-Object -Unique).Count -ne $levels.Count){throw ('Expected ten fresh source epochs on '+$role)}
        }
        $tx=@(Get-SnapshotHashes -Text $hostText -Marker SMOKE_SNAPSHOT_TX)
        $rx=@(Get-SnapshotHashes -Text $clientText -Marker SMOKE_SNAPSHOT_APPLIED)
        $common=@($rx | Where-Object {$tx -contains $_} | Select-Object -Unique)
        if($common.Count -lt (4*$levels.Count)){throw 'Insufficient common Campaign snapshots.'}
        if((Get-FileHash -LiteralPath $GameExe).Hash -ne $exeSha){throw 'Executable changed during Campaign smoke.'}
    }catch{$failure=$_.Exception.Message}
}
Ensure-Directory $runDir
[ordered]@{status=$(if($failure){'fail'}else{'pass'});error=$failure;fixture='Campaign356';sha256=$exeSha;common=$common.Count;frames=$frames;sourceLevels=$levels;fullNaturalWins=$false;profileEarned=$false;owned_cleanup=[bool]($clientStopped -and $hostStopped)} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runDir 'summary.json') -Encoding utf8
if($failure){Write-SmokeStatus ('FAIL Campaign356: '+$failure);exit 1}
Write-SmokeStatus ('PASS Campaign356, common='+$common.Count+' frames='+$frames.Count)
exit 0
