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

## Constraints and contracts

- User steering: no Computer Use, no game-window activation, no desktop capture. Background smoke uses hidden SDL windows and captures only its own render target. No user keyboard/mouse injection.
- Protocol30 groups shared wire changes; build identity is separate and changes each build. No silent mixed-version fallback.
- NetBoardSync.cpp shared ownership: entity_sync owns before `bool Board::IsCoopEndless()`; modes_pvp owns custom-mode tail. Root owns NetManager/Common/Connection/Lobby, projects and integration.
- WebRTC without TURN cannot guarantee connectivity through all NAT/firewalls. No external deployment or account creation has been performed.
- Remaining acceptance tests and external setup steps stay explicit; do not mark the entire list complete on compilation alone.
