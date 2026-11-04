# Two-process co-op smoke harness

`RunCoopSmoke.ps1` launches one host and one client against loopback, waits for a deterministic Adventure level 1 session, validates both result files and diagnostic logs, and then checks that a snapshot transmitted by the host was applied by the client with the same hash.

The runner has no external dependencies beyond Windows PowerShell 5.1. It requires the game-side `-coop-smoke-manifest=<absolute path>` hook described by `SmokeManifest.example.ini`. The hook must isolate `appdata_dir`, `config_path`, and `log_dir`; skip the intro; automatically host/connect/start; and write `result_path` atomically after `minimum_snapshots` snapshots. The runner then closes the exact client process first and the exact host process second through their main windows.

Run from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\CoopSmoke\RunCoopSmoke.ps1
```

Useful overrides:

```powershell
.\tests\CoopSmoke\RunCoopSmoke.ps1 `
  -GameExe C:\PvZ\stabledecompile-master\build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe `
  -Seed 424242 -TimeoutSeconds 180 -MinimumSnapshots 5
```

Artifacts are retained by default under `%TEMP%\PvZ-Coop-Smoke\run_<GUID>`. Failed runs are always retained. `-CleanupOnSuccess` removes only the validated GUID run directory after both owned processes have stopped; the runner never uses `taskkill /IM` or kills unrelated PvZ processes.

Expected result schema:

```json
{"status":"pass","role":"host","protocol":16,"mode":"adventure","level":1,"seed":424242,"snapshotsSent":5}
```

The client uses the same fields with `"role":"client"` and `"snapshotsApplied":5`. Logs must include `SMOKE_SNAPSHOT_TX ... hash=<hex>` on the host and `SMOKE_SNAPSHOT_APPLIED ... hash=<hex>` on the client. The runner also requires the normal listen/authentication/WELCOME/profile/start/gameplay signatures and rejects access violations, protocol violations, queue overflow, malformed starts, rejected snapshots, and host-loss markers.

The game must run in an interactive Windows desktop session; this is process automation, not a headless renderer.
