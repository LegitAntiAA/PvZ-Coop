#requires -Version 5.1
[CmdletBinding()]
param([string]$GameExe = "", [string]$ArtifactsRoot = "", [int]$TimeoutSeconds = 150)

$ErrorActionPreference = "Stop"
& (Join-Path $PSScriptRoot "RunFusionSmoke.ps1") -Fixture GameModes -GameExe $GameExe `
    -ArtifactsRoot $ArtifactsRoot -TimeoutSeconds $TimeoutSeconds
exit $LASTEXITCODE
