# Active delivery — 2026-11-12

Root owns REQUEST.txt plus the user's new mystery-plant and all-mode difficulty/PvP additions. This is a continuing task; do not duplicate start reporting.

## Constraints and repository

- NO Computer Use, foreground activation or desktop screenshots. Hidden game processes and game-renderer captures only, maximum three concurrent games. Check exact owned PIDs before rebuild/stop.
- Routine Russian chat/Telegram reports and commits/pushes to main via existing gh-auth are authorized. No force-push. Root owns integration/reporting.
- Repo C:\PvZ\stabledecompile-master, latest published main f0453db7d6e7fb77fe661f3cb1738ecf4fd9f4ad. Build and Release, Unit Tests, Pages CI Passed; release job Skipped, no release published. Initial dirty work preserved in a991342; do not discard.
- Last completed build8:0.30.13-f0453db7d6-1794429631806. Build9b Passed build0.30.15-f0453db7d6-1794431284606, protocol665/0/0. Promote9b Passed all3 banks10/hash7AC708CE and slot2 actual placement. No game processes remain. MSBuild VS18 BuildTools, PlantsVsZombies.sln ReleaseGOTY/Win32.
- Shared Worker embedded: https://pvz-coop-signaling.rostov20172017.workers.dev. Player endpoint/STUN options removed, operator-only guide supplied. No cross-external-NAT proof or TURN.

## Verified evidence

- NativeUI6 Passed: actual SDL zero-progress minigames override; tint sliders64/128/191, close/reopen/full restart; public create/password retry/WebRTC/capacity; expanded/collapsed lobby and actual ZenGarden navigation. Root viewed renderer captures.
- AvatarE2E6 Passed: both personal avatars edited/saved/restarted, exact distinct13-byte configs, natural initializer host/client union on both peers. No statistical wave-frequency claim.
- PvP5b Passed: private HUD, renderer-observed opponent cursor filtering, SDL/raw/forged shovel rejection, plant tool/lane/cancel,98→99→100 boundary/fade fixture. Public game5b real DataChannel pair Passed on same PC.
- Kick5b Passed all observer communication, mutation denial and pause kick. Presentation7 Passed9stages/18frames, matching snail body0/Zzz3 plus limbs/Jack/avatars. Snail fixture is grid-only, not full ZenGarden UI.
- Fresh4d Passed29markers/24frames per peer; TD4d Passed count/type/stockHP/semantic cursor/five-card redistribution. Current interactions+Unicode and ten legacy regressions remain.

## Current baseline / next actions

1. Baseline9b built and Promote9b Passed; checkpoint now to main. No overlapping game suites.
2. Awaiting9: ready requires settled contiguous cards; exact serialized quotas; final STATE before START; NetManager drains provisional snapshots before final commit and ignores late STATE; pick/unpick uses one FIFO; final warning return rechecks other active players' ready.
3. Universal ShowChallengeScreen KillBoard fixes stale clients after level completion. Promote6/7 reached clean menu/new three-player chooser; final plant step blocked by fixture cursor check and discovered9-card commit race. Both now patched, not yet verified.
4. Checkpoint verified baseline to main before protocol31. Root runs interactions+Unicode and work/RunDeliveryRegression.ps1 ten legacy scenarios. EndlessOverflow is still Not executed.
5. New mystery/difficulty implementation follows concrete shared contracts and independent file ownership after baseline checkpoint.

## New requirements / design direction

- Rectangular ? random button/tooltip, all personal mutable slots filled; fixed Dave cards visible. Unique ordinary roll cards; Imitator MAY copy already rolled plant. Other players' cards never constrain mystery rolls.
- Nobody sees mystery before PLAYING; picker never marks them taken or blocks manual picks. Mystery locked until repeated button clears mode/returns manual. Any active mystery player suppresses native no-sunflower warning.
- Defer actual random generation until PLAYING: public placeholders owner/count/stableID/origin through chooser/INTRO. Atomic reliable START+reveal by ID before rendering. Snapshot redacts all type-derived fields while pending. CutScene already preloads all SeedTypeAvailable; sampler same eligibility.
- Intrinsic random-origin discount linear5% at1→25% at10, persists ownership change. Define raw SeedBank save compatibility; legacy padding is not valid origin state.
- Hard PvE10 and all playable minigame/custom/direct-spawn/puzzle families. No PvE HP change except native Endless. Wave caps100/50 require cadence and family policies.
- PvP bounded smooth comeback support begins2; difficulty1 adaptive assistance0. Plants price/fire/sunflower/sky sun; zombies price/speed/HP. Avoid oscillation/farming; voluntary random5% remains at1.

## Ownership and state

Root: NetManager/Common/Interactions/protocol/projects/docs/git/integration. UI: chooser/native UI/lobby; final9 source frozen, read-only mystery storage/UI design. Modes: Board/Challenge/Plant/Zombie and NetBoardSync custom tail; read-only all-family/DDA proposal. Entity: NetBoardSync before IsCoopEndless/snapshots/presentation/avatar/observerQA; Promote9 after Root signal; wire31 proposal supplied. No new production until baseline green.

Local Wrangler owned session52476 if still running; stop by owned session when done. Detailed artifacts in WORKBOARD and work/* summary.json. User-facing actual PNGs in Documents\Codex\2026-11-11\new-chat\outputs: pvz-online-browser-build6, pvz-coop-appearance-build6, pvz-host-client-zombatars-build6.

Memory used: final citation MEMORY.md58–63, rollout01a1a74a-4292-7871-92d4-fea246cbe755. Do not edit memories.

