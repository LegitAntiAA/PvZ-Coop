#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameExe = "",
    [string]$ArtifactsRoot = "",
    [ValidateRange(60, 600)][int]$TimeoutSeconds = 180
)

$ErrorActionPreference = "Stop"
$controlsRoot = if ($ArtifactsRoot) { [IO.Path]::GetFullPath($ArtifactsRoot) } else {
    Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) "work\controls-smoke"
}
[void][IO.Directory]::CreateDirectory($controlsRoot)
$controlsPrevious = $env:PVZ_COOP_SMOKE_CONTROLS
try {
    $env:PVZ_COOP_SMOKE_CONTROLS = "1"
    foreach ($controlsScenario in @("Adventure1", "Adventure5BowlingClient")) {
        $controlsCase = Join-Path $controlsRoot $controlsScenario
        $controlsArgs = @{
            Scenario = $controlsScenario; Background = $true; ArtifactsRoot = $controlsCase
            SkipIntro = 1; MinimumSnapshots = 5; TimeoutSeconds = $TimeoutSeconds
        }
        if ($GameExe) { $controlsArgs.GameExe = $GameExe }
        & (Join-Path $PSScriptRoot "RunCoopSmoke.ps1") @controlsArgs
        if ($LASTEXITCODE -ne 0) { throw "Controls scenario failed: $controlsScenario" }
        $controlsRun = Get-ChildItem -LiteralPath $controlsCase -Directory |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if (-not $controlsRun) { throw "Controls artifacts missing: $controlsScenario" }
        foreach ($controlsRole in @("host", "client")) {
            $controlsRoleRoot = Join-Path $controlsRun.FullName $controlsRole
            $controlsLog = (Get-ChildItem -LiteralPath (Join-Path $controlsRoleRoot "logs") -File |
                ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
            if ($controlsLog -notmatch ('SMOKE_CONTROLS_COMPLETE role=' + $controlsRole + '\b[^\r\n]*clientDigit=1 hostDigit=1 shovel=1 cancel=1 chatGate=1 pauseGate=1')) {
                throw "Controls completion evidence missing: $controlsScenario/$controlsRole"
            }
            foreach ($controlsFrame in @("client_pick", "client_plant", "client_shovel", "complete")) {
                if (-not (Test-Path -LiteralPath (Join-Path $controlsRoleRoot ("controls_" + $controlsFrame + ".bmp")))) {
                    throw "Controls renderer frame missing: $controlsScenario/$controlsRole/$controlsFrame"
                }
            }
        }
    }
    Write-Host "Controls normal and conveyor workflows Passed."
}
finally {
    $env:PVZ_COOP_SMOKE_CONTROLS = $controlsPrevious
}
