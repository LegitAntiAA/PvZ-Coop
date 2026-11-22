#requires -Version 5.1
[CmdletBinding()]
param([string]$GameExe='', [string]$ArtifactsRoot='', [ValidateSet('day','night','pool','fog')][string]$Biome='day', [int]$TimeoutSeconds=300,[switch]$Behavior,[switch]$NextBatch)
if($Behavior -and $TimeoutSeconds -lt 1200){$TimeoutSeconds=1200}
$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$GameExe){$GameExe=Join-Path $repoPath 'build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe'}
if(!$ArtifactsRoot){$ArtifactsRoot=Join-Path $repoPath ('work\fusion\stage325\'+$Biome)}
. (Join-Path $PSScriptRoot 'RunCoopSmoke.ps1') -FunctionsOnly -Background -GameExe $GameExe -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds -HostWindowWidth 1066 -ClientWindowWidth 800
$GameExe=Get-NormalizedPath $GameExe;$gameDir=Split-Path -Parent $GameExe
$runId=[Guid]::NewGuid().ToString('N');$runDir=Join-Path (Get-NormalizedPath $ArtifactsRoot) ('run_'+$runId)
$hostDir=Join-Path $runDir 'host';$clientDir=Join-Path $runDir 'client';$hostLogs=Join-Path $hostDir 'logs';$clientLogs=Join-Path $clientDir 'logs'
$hostProcess=$null;$clientProcess=$null;$failure='';$results=@();$frames=@()
$level=@{day=1;night=11;pool=21;fog=31}[$Biome]
# The private CLI scenario transports level1; the validated biome helper boots
# the real requested level before Board creation on both peers.
$kinds=if($level -ge 21){@(16,17,19,20,27)}else{@(24,30,101,102,103,28,31,32,33,34,45,203,223,25,26,29,35,36,37,38,39,40,43,212,213)}
if($NextBatch){$kinds=if($level -ge 21){@(200,113,214,17,27)}else{@(112,216,211,217,202,204,205,206,210,218,219,220,221,31,35,114,118,119,47,48)}}
$environment=@{};foreach($item in @(Get-ChildItem Env: | Where-Object Name -like 'PVZ_COOP_SMOKE_FUSION_*')){$environment[$item.Name]=$item.Value}
foreach($name in @('PVZ_COOP_SMOKE_FUSION_NATIVE','PVZ_COOP_SMOKE_FUSION_ZOMBIE313','PVZ_COOP_SMOKE_FUSION_ZOMBIE313_BIOME','PVZ_COOP_SMOKE_FUSION_ZOMBIE313_BEHAVIOR','PVZ_COOP_SMOKE_FUSION_ZOMBIE313_NEXT','PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT')){$environment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try{
    if(@(Get-CimInstance Win32_Process -Filter "Name='PlantsVsZombies.exe'").Count){throw 'An existing game is running; it will not be touched.'}
    $port=Get-FreeLoopbackPort
    foreach($role in @('host','client')){
        $root=if($role -eq 'host'){$hostDir}else{$clientDir};$width=if($role -eq 'host'){1066}else{800}
        $appDir=Join-Path $root 'appdata';$logs=Join-Path $root 'logs';Ensure-Directory $appDir;Ensure-Directory $logs
        Write-CoopConfig -Path (Join-Path $root 'coop.ini') -Nickname ('Zombie313'+$role) -WindowWidth $width -WindowHeight 600
        Write-SmokeManifest -Path (Join-Path $root 'smoke.ini') -Role $role -Nickname ('Zombie313'+$role) -AppDataDirectory $appDir -ConfigPath (Join-Path $root 'coop.ini') -LogDirectory $logs -ResultPath (Join-Path $root 'result.json') -SelectedPort $port -SelectedMode fusion_validation -SelectedLevel 1 -SelectedSeed 424242 -SelectedTimeoutMs ($TimeoutSeconds*1000) -RequiredSnapshots 20 -SelectedSkipIntro 0
    }
    foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PVZ_COOP_SMOKE_FUSION_NATIVE='1';$env:PVZ_COOP_SMOKE_FUSION_ZOMBIE313='1';$env:PVZ_COOP_SMOKE_FUSION_ZOMBIE313_BIOME=$Biome;if($Behavior){$env:PVZ_COOP_SMOKE_FUSION_ZOMBIE313_BEHAVIOR='1'};$env:PVZ_COOP_SMOKE_HIDE_CONTROL_CHAT='1';if($NextBatch){$env:PVZ_COOP_SMOKE_FUSION_ZOMBIE313_NEXT='1'}
    Write-SmokeStatus ('Zombie313 owned hidden pair (Behavior='+$Behavior+'): '+$runDir)
    $hostProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDir -ManifestPath (Join-Path $hostDir 'smoke.ini') -Role host -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runDir 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $null
    Wait-ForLogPattern -LogDirectory $hostLogs -Pattern ('(?im)host listening port='+$port+'\b') -Process $hostProcess -Role host -TimeoutSeconds 120
    $clientProcess=Start-SmokeProcess -Executable $GameExe -WorkingDirectory $gameDir -ManifestPath (Join-Path $clientDir 'smoke.ini') -Role client -RunId $runId -EnableUnicodeChat $false
    Save-PidLedger -Path (Join-Path $runDir 'pids.json') -Executable $GameExe -HostProcess $hostProcess -ClientProcess $clientProcess
    $results=@(Wait-ForResults -HostResultPath (Join-Path $hostDir 'result.json') -ClientResultPath (Join-Path $clientDir 'result.json') -HostProcess $hostProcess -ClientProcess $clientProcess -HostLogDirectory $hostLogs -ClientLogDirectory $clientLogs -TimeoutSeconds $TimeoutSeconds)
    if($results.Count -ne 2 -or @($results | Where-Object {$_.status -ne 'pass'}).Count){throw 'Both Zombie313 resource-only peers must pass.'}
    if($results[0].protocol -ne $results[1].protocol){throw 'Zombie313 peer protocols differ.'}
}catch{$failure=$_.Exception.Message}
finally{
    foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,$environment[$name],'Process')}
    $clientStopped=Stop-OwnedProcess -Process $clientProcess -Role client;$hostStopped=Stop-OwnedProcess -Process $hostProcess -Role host
    if(!$clientStopped -or !$hostStopped){$failure+=' Owned pair cleanup failed.'}
}
if(!$failure){try{
    foreach($role in @('host','client')){
        $logs=if($role -eq 'host'){$hostLogs}else{$clientLogs};$root=if($role -eq 'host'){$hostDir}else{$clientDir};$log=Get-CombinedLogText $logs
        if($log -match 'SMOKE_FAIL|native Fusion snapshot (encode|preflight) rejected|seq-gap|SourceObstacle313 stopped'){throw ('Zombie313 failure/rejection on '+$role)}
        foreach($kind in $kinds){
            $pattern='SMOKE_ZOMBIE313_RESOURCE role='+$role+' biome='+$Biome+' index=\d+ kind='+$kind+' profile=1 carrier=\d+ model=1 root=1 closure=1 actorAllocation=0 runtimeAI=NotExecuted'
            if([regex]::Matches($log,$pattern).Count -ne 1){throw ('Missing or duplicate genuine resource proof kind='+$kind+' role='+$role)}
        }
        $complete='SMOKE_ZOMBIE313_RESOURCES_COMPLETE role='+$role+' biome='+$Biome+' sourceKinds='+$kinds.Count+'\b[^\r\n]*FNS=29\b[^\r\n]*resourceOnly=1 acceptedNPCs=0 runtimeAI=NotExecuted'
        if($Behavior){
            $groups=if($level -ge 21){1}elseif($NextBatch){4}else{5}
            $complete='SMOKE_ZOMBIE329_COMPLETE role='+$role+' biome='+$Biome+' groups='+$groups+' rootsBorn='+$kinds.Count+' resourceKinds='+$kinds.Count+'\b[^\r\n]*fullAIParity=0 fullCampaign=0 naturalPlayerCombat=NotExecuted'
            if([regex]::Matches($log,$complete).Count -ne 1){throw ('Missing bounded behavioral completion on '+$role)}
            foreach($group in 0..($groups-1)){
                $api=if($level -ge 21 -or ($NextBatch -and $group -eq 2) -or (!$NextBatch -and $group -in @(1,3))){1}else{0}
                if([regex]::Matches($log,('SMOKE_ZOMBIE329_CASE_COMPLETE role='+$role+' biome='+$Biome+' group='+$group+' roots=5 actualBirths=5 pausedCanonical=1 sourceAPIProbe='+$api)).Count -ne 1){throw ('Missing real case completion group='+$group+' role='+$role)}
                $bound='SMOKE_ZOMBIE329_BOUND role='+$role+' group='+$group+'\b'
                if([regex]::Matches($log,$bound).Count -ne 5){throw ('Expected five actual full-generation body bindings group='+$group+' role='+$role)}
                if($role -eq 'host' -and [regex]::Matches($log,('SMOKE_ZOMBIE329_BIRTH role=host group='+$group+'\b')).Count -ne 5){throw ('Expected five genuine typed births group='+$group)}
            }
            if($NextBatch){$required=if($level -ge 21){@('HeadLoss17=1')}else{@('ThrowThreshold35=1 child37=[1-9]\d*/[1-9]\d*')}}elseif($level -ge 21){$required=@('ColdFreezeCaltrop16=1','HeadLoss17=1')}else{$required=@('MC32=1','ThrowThreshold35=1 child37=[1-9]\d*/[1-9]\d*')}
            foreach($case in $required){if($log -notmatch ('SMOKE_ZOMBIE329_CASE_COMPLETE role='+$role+'\b[^\r\n]*'+$case)){throw ('Missing exact actual API case '+$case+' role='+$role)}}
        }elseif([regex]::Matches($log,$complete).Count -ne 1){throw ('Missing exact resource-only completion on '+$role)}
        $executed=if($role -eq 'host'){1}else{0}
        if($log -notmatch ('SMOKE_ZOMBIE313_CAPACITY role='+$role+' biome='+$Biome+' roots='+$kinds.Count+' hostOnly=1 executed='+$executed+' ready=1 noDeadCleanupCredit=1')){throw ('Missing actual host-capacity distinction on '+$role)}
        $suffixes=@('chooser','resources')
        if($Behavior){$suffixes=@('g0_resources');foreach($group in 0..($groups-1)){
            foreach($part in @('chooser','initial','live','moved','paused')){$suffixes+=('g'+$group+'_'+$part)}
            if($level -ge 21 -or ($NextBatch -and $group -eq 2) -or (!$NextBatch -and $group -in @(1,3))){$suffixes+=('g'+$group+'_api')}
        }}
        foreach($suffix in $suffixes){
            $name='fusion_zombie313_'+$Biome+'_'+$suffix+'.bmp';$path=Join-Path $root $name
            if(!(Test-Path -LiteralPath $path)){throw ('Missing fresh owned frame '+$path)}
            $frames+=@{role=$role;name=$name;sha256=(Get-FileHash -LiteralPath $path).Hash}
        }
    }
    if($Behavior){
        $hostText=Get-CombinedLogText $hostLogs;$clientText=Get-CombinedLogText $clientLogs
        foreach($group in 0..($groups-1)){
            $stages=@('INITIAL','MOVED','CALLBACKS');if($level -ge 21 -or ($NextBatch -and $group -eq 2) -or (!$NextBatch -and $group -in @(1,3))){$stages+=('APPLIED')}
            foreach($stage in $stages){
                $pattern='SMOKE_ZOMBIE329_CANONICAL role={0} group='+$group+' stage='+$stage+' digest=([0-9a-f]{{8}})'
                $hostHashes=@([regex]::Matches($hostText,($pattern -f 'host'))|ForEach-Object {$_.Groups[1].Value})
                $clientHashes=@([regex]::Matches($clientText,($pattern -f 'client'))|ForEach-Object {$_.Groups[1].Value})
                if(!@($hostHashes|Where-Object {$clientHashes -contains $_}).Count){throw ('Missing common ACTUAL canonical state group='+$group+' stage='+$stage)}
            }
        }
    }
}catch{$failure=$_.Exception.Message}}
$record=[ordered]@{scenario=$(if($Behavior){'Zombie313Behavior'}else{'Zombie313Resources'});behavior=$Behavior.IsPresent;biome=$Biome;level=$level;resourceKinds=$kinds;status=$(if($failure){'failed'}else{'passed'});failure=$failure;executable=$GameExe;executableSHA256=(Get-FileHash -LiteralPath $GameExe).Hash;frames=$frames;runtimeAI=$(if($Behavior -and !$failure){'Bounded cases passed; full AI parity Not executed'}else{'Not executed'});liveBirths=$(if($Behavior -and !$failure){$kinds.Count}else{0});acceptedFullAI=0;fullCampaign=$false;runDirectory=$runDir}
Write-Utf8NoBom -Path (Join-Path $runDir 'zombie313-summary.json') -Text ($record|ConvertTo-Json -Depth 6)
if($failure){throw $failure}
Write-SmokeStatus ('Zombie313 '+$Biome+' Passed within declared resource/cohort/API scope; full AI parity Not executed: '+$runDir)
