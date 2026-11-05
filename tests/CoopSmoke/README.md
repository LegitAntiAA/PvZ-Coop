# Two-process co-op smoke harness

`RunCoopSmoke.ps1` launches one host and one client against loopback, starts one of the audited deterministic scenarios, performs one scenario-specific gameplay action, validates both result files and diagnostic logs, and checks that a post-action snapshot transmitted by the host was applied by the client with the same FNV-1a hash.

The runner has no external dependencies beyond Windows PowerShell 5.1. It uses the game-side `-coop-smoke-manifest=<absolute path>` hook described by `SmokeManifest.example.ini`. Each process has isolated appdata, config, logs, result, window dimensions, and profile state. The hook automatically hosts/connects/starts and writes `result_path` atomically only after the scripted action is visible on both sides and at least `minimum_snapshots` post-action snapshots were observed. The runner then closes the exact client process first and the exact host process second through their main windows.

Supported scenarios are deliberately fail-closed:

| Runner scenario | Manifest pair | Deterministic action |
|---|---|---|
| `Adventure1` | `mode=adventure`, `level=1` | Host selects the ready Peashooter packet and plants through `Board::MouseDown/Up`. |
| `Adventure5Bowling` | `mode=adventure`, `level=5` | Host takes the first ready conveyor packet and rolls it through the normal board mouse path. |
| `Adventure5BowlingClient` | `mode=adventure_bowling_client`, `level=5` | Client selects a host-identified conveyor packet through ordered network clicks; both peers must show the same moving nut. |
| `Adventure8Soak` | `mode=adventure_soak`, `level=8` | Both peers exercise the stock chooser, planting, sun collection, synchronized loss camera, brains presentation, and loss dialog. |
| `Adventure8Victory` | `mode=adventure_victory`, `level=8` | Both peers exercise the stock chooser and ready-set-plant sequence before synchronized victory fade and award UI. |
| `Adventure15WhackClient` | `mode=adventure_whack_client`, `level=15` | Client hits a host-identified ordinary zombie with the hammer; both peers must observe the stock immediate removal under the same network ID. |
| `SlotMachineClient` | `mode=slot_machine_client`, `level=1` | Client pulls the lever; both peers must observe the same roll, pull/return animations, three completed reels, 25 sun spent, and exactly one slot-machine sound. |
| `TowerDefenseClient` | `mode=tower_defense_client`, `level=1` | Client plants on a non-road cell. Both peers must report the same generated path bytes/hash, road cell count, zombie ID through off-lawn entry and a path turn, and planted ID/cell. The road alternates through exactly three separated edge visits (`top-bottom-top` or `bottom-top-bottom`), never creates an adjacent 2x2 road block, and uses the dirt road renderer. |
| `PortalCombatClient` | `mode=portal_combat_client`, `level=1` | Client plants from the stock conveyor. Both peers require exactly four live portals (two of each type) with matching IDs/cells/states and real portal resources, then observe the same stock zombie before and after a real portal teleport, a one-apply client snap, relocation advice show/clear, and a clean four-open-portal relocated layout. |
| `EndlessST` | `mode=endless_st`, `level=1` | Both peers exercise the initial 5+5 chooser, authoritative EndlessST mode, a stock day/night and weather update, stock wave progression, the ten-wave stage boundary through `CheckForGameEnd`, the synchronized repick chooser, and matching 1.12x zombie health scaling in the next stage. After repick, both peers must also prove one live non-attachment animation advances between two samples with positive rate and unpaused music; snapshot/health evidence alone cannot pass. The harness compresses only stock timers and never moves zombies. |
| `Vasebreaker` | `mode=vasebreaker`, `level=1` | Client clicks the first live vase through the strict network input path. |
| `Beghouled` | `mode=beghouled`, `level=1` | Client performs a valid drag through the strict network input path. |

Any other mode/level pair, unknown/duplicate field, relative or escaping path, invalid boolean, or pre-existing result file is rejected before normal startup.

Run from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\CoopSmoke\RunCoopSmoke.ps1
```

Useful overrides:

```powershell
.\tests\CoopSmoke\RunCoopSmoke.ps1 `
  -GameExe C:\PvZ\stabledecompile-master\build\ReleaseGOTY_Win32\bin\PlantsVsZombies.exe `
  -Scenario Adventure5BowlingClient -SkipIntro 0 `
  -HostWindowWidth 1366 -HostWindowHeight 768 `
  -ClientWindowWidth 1920 -ClientWindowHeight 1080 `
  -Seed 424242 -TimeoutSeconds 180 -MinimumSnapshots 5
```

Run the smoke tranche explicitly:

```powershell
foreach ($scenario in "Adventure1", "Adventure5Bowling", "Adventure5BowlingClient", "Adventure15WhackClient", "Vasebreaker", "Beghouled") {
  .\tests\CoopSmoke\RunCoopSmoke.ps1 -Scenario $scenario -SkipIntro 1
}
foreach ($scenario in "Adventure8Soak", "Adventure8Victory", "SlotMachineClient", "TowerDefenseClient", "PortalCombatClient", "EndlessST") {
  .\tests\CoopSmoke\RunCoopSmoke.ps1 -Scenario $scenario -SkipIntro 0
}
```

The Slot Machine, Tower Defense, Portal Combat, and EndlessST scenarios deliberately run the stock intro. Slot Machine waits for the seed bank and handle to finish their normal slide-on lifecycle; Tower Defense waits for path generation and the client-side plant action; EndlessST also exercises the second chooser after its ten-wave stage transition. `TowerDefenseClient`, `EndlessST`, `Adventure8Soak`, and `Adventure8Victory` reject `-SkipIntro 1` instead of silently weakening their coverage.

`-SkipIntro` accepts `0` or `1`. With `0`, the smoke controller waits for the normal intro to enter `SCENE_PLAYING`; it never injects a gameplay action during cutscene, chooser, dialog, or pause state. Host and client dimensions may differ to exercise coordinate canonicalization; both windows are always visible and placed side by side.

Artifacts are retained by default under `%TEMP%\PvZ-Coop-Smoke\run_<GUID>`. Failed runs are always retained. `-CleanupOnSuccess` removes only the validated GUID run directory after both owned processes have stopped; the runner never uses `taskkill /IM` or kills unrelated PvZ processes.

Expected result schema:

```json
{"status":"pass","role":"host","protocol":22,"scenario":"adventure1","mode":"adventure","level":1,"seed":424242,"skipIntro":true,"snapshotsSent":8,"snapshotsReceived":0,"snapshotsApplied":0,"actionApplied":true,"postActionSnapshots":5}
```

The client uses the same fields with `"role":"client"` and a non-zero `snapshotsApplied`. Logs must contain ordered `SMOKE_CHECKPOINT` markers for `PLAYING`, `BASELINE`, `ACTION_ISSUED` on the owning side, and `ACTION_APPLIED` on both sides. The runner intersects `SMOKE_POST_ACTION_TX ... hash=<hex>` with `SMOKE_POST_ACTION_APPLIED ... hash=<hex>`; a pre-action or intro snapshot cannot satisfy the test. It also requires normal listen/authentication/WELCOME/profile/start/gameplay signatures and rejects access violations, smoke failures, version/protocol violations, queue overflow, malformed starts, rejected snapshots, assertions, and host-loss markers. Fatal signatures are polled while the processes are still running for fail-fast behavior.

The Adventure 5 and Whack cursor assertions require the stock shovel/hammer draw path, player tint, darker outline, and `fallback=0`; receiving only a cursor type cannot pass them. `CURSOR_TYPE_PLANT_FROM_GLOVE` and `CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW` deliberately remain on the generic pointer fallback because their plant/potted-plant object identity is not yet part of the cursor protocol. Hammer shape/tint is covered, but its transient swing animation phase is not serialized by this harness.

Each GUID artifact tree preserves generated configs/manifests, role logs, atomic result JSON, a PID ledger, and `summary.json`. Failed runs are never cleaned automatically.

The game must run in an interactive Windows desktop session; this is process automation, not a headless renderer. This tranche proves launch/lifecycle, coordinate handling, representative host/client actions, and post-action state convergence for its audited scenarios. It is not a complete scripted playthrough or a substitute for the broader mini-game regression matrix.
