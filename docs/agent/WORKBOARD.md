# Play-test delivery — 2026-11-11

Source: `REQUEST.txt` (user's complete list). Root owns integration and evidence.

| ID | Scope and acceptance | Owner | Status |
|---|---|---|---|
| BASE-00 | Preserve initial four dirty files; ReleaseGOTY Win32 and existing protocol suite pass | Root | DONE |
| SYNC-01 | Host presentation mirrored: limbs, jack explosion, intro entities/sod, reward rays, snail, tutorial, shovel/music; first entity positions authoritative | entity_sync | IN_PROGRESS |
| UI-01 | Replace/close screens, correct reward/profile/name/unlocks, store tabs, settings preview/layout | ui_progression | IN_PROGRESS |
| PICK-01 | Native top selected cards, fair total quotas with fixed cards, all-player ready and counts | ui_progression + Root | IN_PROGRESS |
| MODES-01 | Team-private compact PvP HUD/input, brains, lane/cursor, 100-kill goal; real Endless night; hard TD | modes_pvp | IN_TESTING (source complete) |
| NET-01 | Unicode draft/space/backspace, system join/leave/kick chat; host kick and fair seed reassignment; late spectators | Root | IN_PROGRESS |
| SIGNAL-01 | Middle-button ping with sound/500ms cooldown; Ctrl strokes fade after8s; Alt packet ping/name/cooldown; semantic packet-relative cursors | Root | IN_PROGRESS |
| PVP-02 | Teams chosen after launching PvP, before loading board; all participants ready | ui_progression + Root | IN_PROGRESS |
| DIFF-01 | Host lobby difficulty1..10; count/wave/type scaling, HP unchanged, default1 stock | modes_pvp + Root | IN_PROGRESS |
| AVATAR-01 | Own/client Zombatars occasionally spawn and appearances mirror | entity_sync + Root | IN_PROGRESS |
| ONLINE-01 | Native WebRTC/STUN, version-filtered rooms/name/password/capacity; free Cloudflare signaling and setup instructions | modes_pvp + Root | IN_PROGRESS |
| DOC-01 | Accurate bilingual README/roadmap, GitHub link; Lua/more games/adventure/UI only planned | Root + ui_progression | READY |
| VERIFY-01 | Integrated build, protocol regressions, background two-process game runs and game-only renderer captures; independent diff review | Root | IN_PROGRESS |

## Evidence

- Initial branch `main`, HEAD `abc6a54`; initial dirty files: NetSmokeScenario.cpp/.h, RunCoopSmoke.ps1, TestMain.cpp. Do not revert these.
- Initial ReleaseGOTY/Win32 build: Passed (exit0), `work/baseline-build.log`.
- Initial NetProtocolTests: Passed, checks552 failures0 skipped0.
- Initial Adventure1 skip_intro0: Passed, protocol29 hash963905EC; artifacts `run_d1df890f599147728d5375921bc3e865` under system-temp/PvZ-Coop-Smoke.
- New changes are NOT runtime verified until recorded here. Source completion and an author's review are not proof of success.

### New implementation evidence

- Integration4c/4d: Passed, Win32 ReleaseGOTY; current protocol tests checks650 failures0 skipped0. Source includes universal spectator message authority policy, packet-ID/semantic bank matching and the PvP client hand identity correction.

- Integration builds1/2/3/4/4b: Passed, Win32 ReleaseGOTY. Local protocol suite on4b: checks633 failures0 skipped0.
- Background Adventure1 skip_intro0: Passed, protocol30/hash4413B3C5. Its first widget-texture BMPs were black and are excluded from visual evidence.
- Background interactions3: logic Passed, hash0EF21305; pings2/3, stroke begin+3points+end, packetID7, hold/fade/clear alpha checks. Actual scene3 renders are valid; eight older helper captures are black, so visual interaction verification must repeat.
- Native WebRTC15 checks and Worker8 checks: Passed. Actual game WebRTC Adventure1: Passed, eight common post-action hashes, native DataChannel adopted on each peer, actual nonblack host/client game frames inspected. External NAT/Internet deployment not tested.
- Fresh3: Failed fixture ACK barrier (not a completed game regression pass); demo/sod/Sunflower sub-observations retained.
- Fresh4b: Passed, hashF818C08F,19 markers and17 frames per peer; complete153-byte UTF-8 advice; intro, sod, tutorial flash, reward idle/pickup rays, correct Sunflower, next-level UI cleanup, profile/name/unlocks, challenge/store lifecycle.
- Actual Fresh menu review then found the missing GitHub icon; basename fix and stable menu/store image assertions await the next build/run.
- Spectator3: Failed TD intro precondition; late join Not executed. Chooser reentry fixed;4b retest in progress.
- PvP tool/100-kill and difficulty10/semantic cursor regressions: source ready, runtime pending.
- Modes4b: PvP Failed before fixture because brains trickled during intro; fixed by PLAYING lifecycle gate. TD difficulty10 subchecks Passed (40→100waves,308→15854points, strong types and stock HP), then Failed viewer-relative-bank check; fixed ID reset/INTRO semantic matching. Both require current-binary repeat.

### Git checkpoints

- a991342: initial implementation WIP checkpoint, including preserved earlier EndlessOverflow work.
- 512ccdd: merged13 origin/main commits without conflicts and pushed main through the user's gh-auth. GitHub Build and Release / Unit Tests succeeded for this checkpoint. Later work is not implied to be in this commit.

## Constraints and contracts

- User steering: no Computer Use, no game-window activation, no desktop capture. Background smoke uses hidden SDL windows and captures only its own render target. No user keyboard/mouse injection.
- Protocol30 groups shared wire changes; build identity is separate and changes each build. No silent mixed-version fallback.
- NetBoardSync.cpp shared ownership: entity_sync owns before `bool Board::IsCoopEndless()`; modes_pvp owns custom-mode tail. Root owns NetManager/Common/Connection/Lobby, projects and integration.
- WebRTC without TURN cannot guarantee connectivity through all NAT/firewalls. No external deployment or account creation has been performed.
- Remaining acceptance tests and external setup steps stay explicit; do not mark the entire list complete on compilation alone.
