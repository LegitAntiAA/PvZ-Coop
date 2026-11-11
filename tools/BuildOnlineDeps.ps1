[CmdletBinding()]
param(
    [ValidateSet('Win32', 'x64')][string]$Platform = 'Win32',
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$BuildTests,
    [string]$CMakePath = ''
)

$ErrorActionPreference = 'Stop'
$repoPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$vendorPath = Join-Path $repoPath 'work\vendor'
$nativePath = Join-Path $repoPath "work\online-native\build-$Platform"
$stagePath = Join-Path $repoPath "lib\online\$Platform"
New-Item -ItemType Directory -Path $vendorPath, $stagePath -Force | Out-Null

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
function Get-PinnedSource([string]$Name, [string]$Url, [string]$Tag, [string]$Commit) {
    $sourcePath = Join-Path $vendorPath $Name
    if (!(Test-Path -LiteralPath $sourcePath)) {
        Invoke-Checked 'git' @('clone', '--branch', $Tag, '--depth', '1', '--recurse-submodules', '--shallow-submodules', $Url, $sourcePath)
    }
    $actualCommit = (& git -C $sourcePath rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $Commit) {
        throw "Unexpected $Name revision in $sourcePath. Expected $Commit; preserve local changes and use the pinned checkout."
    }
    Invoke-Checked 'git' @('-C', $sourcePath, 'diff', '--quiet', 'HEAD')
    $submodules = @(& git -C $sourcePath submodule status --recursive)
    if ($LASTEXITCODE -ne 0 -or ($submodules | Where-Object { $_ -match '^[+U-]' })) {
        throw "Uninitialized or modified $Name submodule revision. Run git submodule update --init --recursive in $sourcePath."
    }
}

Get-PinnedSource 'libdatachannel' 'https://github.com/paullouisageneau/libdatachannel.git' 'v0.24.5' '443f6934d9007eb7076ab7825ba330f355fcbead'
Get-PinnedSource 'mbedtls' 'https://github.com/Mbed-TLS/mbedtls.git' 'mbedtls-3.6.7' '068ff080b369adfac81509f9b57b2afabaf82dc5'

$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'Install Visual Studio C++ Build Tools before building the native WebRTC dependency.' }
$vsMajor = (& $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion).Split('.')[0]
$generator = if ([int]$vsMajor -ge 18) { 'Visual Studio 18 2026' } else { 'Visual Studio 17 2022' }
if (!$CMakePath) {
    $bundledCMake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    $CMakePath = if (Test-Path -LiteralPath $bundledCMake) { $bundledCMake } else { (Get-Command cmake).Source }
}
$testFlag = if ($BuildTests) { 'ON' } else { 'OFF' }
Invoke-Checked $CMakePath @('-S', (Join-Path $PSScriptRoot 'online'), '-B', $nativePath, '-G', $generator,
    '-A', $Platform, "-DPVZ_VENDOR_DIR=$($vendorPath.Replace('\', '/'))", "-DPVZ_BUILD_ONLINE_TESTS=$testFlag")
$target = if ($BuildTests) { 'NetOnlineTests' } else { 'datachannel' }
Invoke-Checked $CMakePath @('--build', $nativePath, '--config', $Configuration, '--target', $target, '-j', '4')
$dllPath = Join-Path $nativePath "libdatachannel\$Configuration\datachannel.dll"
Copy-Item -LiteralPath $dllPath -Destination (Join-Path $stagePath 'datachannel.dll') -Force
Write-Output "WebRTC runtime staged: $stagePath\datachannel.dll"
Write-Output 'The game post-build step copies the matching runtime beside PlantsVsZombies.exe.'
if ($BuildTests) {
    Write-Output "Native test executable: $nativePath\$Configuration\NetOnlineTests.exe"
    Write-Output 'Run online/signaling npm run dev, then NetOnlineTests.exe http://127.0.0.1:8787.'
}
