param(
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [Parameter(Mandatory=$true)][string]$CounterPath
)
$ErrorActionPreference = 'Stop'
$counterDirectory = [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($CounterPath))
[IO.Directory]::CreateDirectory($counterDirectory) | Out-Null
$lockPath = $CounterPath + '.lock'
$versionLock = $null
for ($attempt = 0; $attempt -lt 100 -and $null -eq $versionLock; $attempt++) {
    try { $versionLock = [IO.File]::Open($lockPath, 'OpenOrCreate', 'ReadWrite', 'None') }
    catch [IO.IOException] { Start-Sleep -Milliseconds 50 }
}
if ($null -eq $versionLock) { throw 'Another build is still generating the mod version.' }
try {
    [long]$sequence = 0
    if (Test-Path -LiteralPath $CounterPath) {
        if (-not [long]::TryParse([IO.File]::ReadAllText($CounterPath).Trim(), [ref]$sequence)) {
            throw 'Invalid build counter; refusing to reuse an ambiguous build identity.'
        }
    }
    $sequence++
    [IO.File]::WriteAllText($CounterPath, [string]$sequence, [Text.Encoding]::ASCII)
    $identity = '0.30.' + $sequence
    $header = '#pragma once' + "`r`n" + '#define NET_MOD_BUILD_STRING "' + $identity + '"' + "`r`n"
    [IO.File]::WriteAllText($OutputPath, $header, [Text.Encoding]::ASCII)
    Write-Output ('Co-op build identity: ' + $identity)
}
finally { $versionLock.Dispose() }
