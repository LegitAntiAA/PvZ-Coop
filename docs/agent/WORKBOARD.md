# Play-test delivery — 2026-11-11

Source: `REQUEST.txt` (user's complete list). Root owns integration and evidence.

| ID | Scope and acceptance | Owner | Status |
|---|---|---|---|
| BASE-00 | Preserve initial four dirty files; ReleaseGOTY Win32 and existing protocol suite pass | Root | DONE |
| SYNC-01 | Host presentation mirrored: limbs, jack explosion, intro entities/sod, reward rays, snail, tutorial, shovel/music; first entity positions authoritative | entity_sync | IN_PROGRESS |
| UI-01 | Replace/close screens, correct reward/profile/name/unlocks, store tabs; native PvZ browser/lobby; expanded preview; actual settings restart persistence | ui_progression + Root | IN_PROGRESS |
| PICK-01 | Native top selected cards, fair total quotas with fixed cards, all-player ready and counts | ui_progression + Root | IN_PROGRESS |
| MODES-01 | Team-private compact PvP HUD/input, brains, lane/cursor, 100-kill goal; real Endless night; hard TD | modes_pvp | IN_TESTING (source complete) |
| NET-01 | Unicode draft/space/backspace, system join/leave/kick chat; host kick and fair seed reassignment; late spectators with all communication and level-boundary promotion | Root + entity_sync | IN_PROGRESS |
| SIGNAL-01 | Middle-button ping with sound/500ms cooldown; Ctrl strokes fade after8s; Alt packet ping/name/cooldown; semantic packet-relative cursors | Root | IN_PROGRESS |
| PVP-02 | Teams chosen after launching PvP, before loading board; all participants ready | ui_progression + Root | IN_PROGRESS |
| DIFF-01 | Host lobby difficulty1..10; count/wave/type scaling, HP unchanged, default1 stock | modes_pvp + Root | IN_PROGRESS |
| AVATAR-01 | Own/client Zombatars occasionally spawn and appearances mirror | entity_sync + Root | IN_PROGRESS |
| ONLINE-01 | Native WebRTC/STUN, version-filtered rooms/name/password/capacity; free Cloudflare signaling and setup instructions | modes_pvp + Root | IN_PROGRESS |
| DOC-01 | Accurate bilingual README/roadmap and operator-only Cloudflare guide; Lua/more games/adventure remain planned; UI/UX requested now | Root + modes_pvp | IN_PROGRESS |
| VERIFY-01 | Integrated build, protocol regressions, background two-process game runs and game-only renderer captures; independent diff review | Root | IN_PROGRESS |

## Evidence

- Initial branch `main`, HEAD `abc6a54`; initial dirty files: NetSmokeScenario.cpp/.h, RunCoopSmoke.ps1, TestMain.cpp. Do not revert these.
- Initial ReleaseGOTY/Win32 build: Passed (exit0), `work/baseline-build.log`.
- Initial NetProtocolTests: Passed, checks552 failures0 skipped0.
- Initial Adventure1 skip_intro0: Passed, protocol29 hash963905EC; artifacts `run_d1df890f599147728d5375921bc3e865` under system-temp/PvZ-Coop-Smoke.
- New changes are NOT runtime verified until recorded here. Source completion and an author's review are not proof of success.

### New implementation evidence

- Published checkpoint `f0453db` pushed main through gh-auth. Its Unit Tests and Pages CI Passed; Build and Release still in progress at last check.
- NativeUI5b full Passed: zero-progress real minigames click/Back, actual SDL three-slider64/128/191 changes and Close/Reopen, full host/client process restart, compiled PUBLIC service room create/wrong-password→retry→realWebRTC, third room-full feedback/Cancel. Root viewed populated/wrong-password/settings-restart/bracket-profile frames. Visual review found native buttons ignored requested height/disabled state and lobby could cover native ZenGarden; narrow6 polish is in progress.
- PvP5b full Passed, hashBF47BFE0: initial100/passive+50, actual renderer-filtered valid opponent cursors, SDL/raw/forged shovel rejection, legitimate plant tool, held zombie/lane/cancel, real98→99(noaward/nofade)→100 decreasing fade. Root viewed actual separate private HUD and99/100 frames.
- PUBLIC Adventure1 game5b Passed, hash1F646E2F/seven common post-action hashes, both real DataChannels/no TCP. This proves public signaling plus two local game processes; cross-network NAT still not exercised. Saved scene3 frames preceded the action; no post-action image claim.
- Kick5b Passed all three peers: exact mixed bank, neutral cursor/chat/middle/Alt/Ctrl, authenticated raw mutation denial and actual pause kick/reason/system chat. Promote5b then Failed: real LEVEL_COMPLETE promoted eligible3 on host, but client ShowChallengeScreen retained its Board over the menu. Universal scene-entry teardown implemented in6; next chooser/plant was Not executed and requires repeat.
- Integration5b and final leaf: Passed, build0.30.10-03f28d4130-1794426559827;665 protocol checks Passed. Includes NativeUI dedicated runtime fixture, larger native appearance window/browser, config path/default probe, stronger snail-pose assertion, corrected observer/PvP fixtures, and renderer-observed opponent-cursor suppression. Real UI/restart/public-browser run is beginning; no result claimed yet.
- Integration5: Passed, build0.30.8-03f28d4130-1794425100752;665 protocol checks Passed. Presentation5 actualrun Passed9 stages/18 game-render frames; Root independently viewed head-loss, Jack blast, matching customized avatar and both snail frames. The visual review found an uncovered snail body-rate defect despite correct Zzz;5b adds explicit bodyRate0 plus assertion and requires rerun.
- Kick5 whole run Failed because its Alt seed ping was sent250ms after middle ping, inside the intended shared500ms cooldown. Exact late START,5+5 bank identity1255F7D9, readonly server rejection, chat/neutral cursor/middle ping/draw were observed; fixture moved Alt to900ms and needs rebuild/retest. Do not count kick completion as passed.
- PvP5 whole run Failed waiting for opponent cursor evidence: baseline never queued SDL motion. Actual initial100 brains and75→125 passive increment were observed; add real motion rather than weaken the guard. Complete tool/lane/100kill sequence still pending.
- Public native22/22 and Worker9/9 Passed in5b source including actual pending-join failure, structured code, membership cleanup, and retry to a real RTC connection. Public game/UI fixture still pending.
- Published main `03f28d4`: Build and Release, Unit Tests and Pages CI all Passed. Later changes below are uncommitted until the next checkpoint.
- Fresh4d: Passed, protocol30/hash10083F67,29 markers and24 actual game-render captures per peer. Independent visual inspection exposed browser contrast/header and preview-center issues; native redesign and larger appearance panel now in progress. Settings restart persistence was not established by this earlier test.
- TD4d: Passed, hashC1359D3E/eight common hashes. Difficulty10 changes40→100 waves and308→15854 wave points, includes Gargantuar/Football with stock HP. Semantic card19 maps hostindex5/clientindex0. Ordinary client disconnect redistributes all five owned cards: host10, orphaned0.
- Spectator4d: Failed custom-mode start descriptor mismatch (board internallevel0 versus originallevel1). Exact recorded START descriptor fix implemented; next-build retest required.
- PvP4d: Failed fixture race (client spent75 before host baseline read); initial100 was observed on client. Smoke baseline handshake fixed in source; complete shovel/lane/100-kill sequence remains pending.
- User's shared Worker `https://pvz-coop-signaling.rostov20172017.workers.dev/health` returned200/service=pvz-coop-signaling/schema1. URL and STUN are compiled in NetServiceConfig.h; player configuration keys removed. Public WebRTC game test pending.
- Config source defect: initial load preceded SexyApp's CWD change, while relative saves followed it. Default is now absolute executable-adjacent coop.ini; explicit smoke paths are resolved once. Protocol suite665/0 Passed, including a separate hidden copied test EXE launched from a directory with a decoy coop.ini, followed by another CWD change/save. Actual UI restart pending.
- Public native20 diagnostic run: Passed20/0 using the owner's Worker, including password/version/capacity errors, real ICE/DTLS/SCTP, ordered max frames and kick/leave. Earlier public attempt Failed3/1 before room creation with partial WSS disconnects; cause not confirmed and timings unchanged. Node WSS ready/8s idle/normalclose1000 also Passed. Public game pair still pending.
- Spectator source now allows neutral visible cursors, chat/pings/draw/packet annotations with PvP audience privacy; actual level-completion/new-game/retry hooks promote observers and balance retained owned cards without resetting IDs/cooldowns. Three-player communication/promotion runtime verification pending.
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
- NetBoardSync.cpp shared ownership: entity_sync owns before `bool Board::IsCoopEndless()`; modes_pvp owns custom-mode tail. Root owns NetManager/Common/Connection, projects and integration. ui_progression owns NetLobbyPanel and native dialogs during the redesign.
- WebRTC without TURN cannot guarantee connectivity through all NAT/firewalls. The owner deployed the shared Worker and supplied its URL; no account credentials were requested or accessed.
- Remaining acceptance tests and external setup steps stay explicit; do not mark the entire list complete on compilation alone.

### Baseline9b checkpoint

- Integration9b Passed ReleaseGOTY/Win32, build0.30.15-f0453db7d6-1794431284606; protocol665/0/0. Initial9 compile Failed one obsolete FIFO clear, fixed before9b.
- Promote9b Passed: real completed-level boundary, clean menus, observer promoted, all3 committed banks10 with4/3/3 ownership, own-prefix and unique IDs; semantic hash7AC708CE agreed. Actual SDL slot2 packet21 produced plant24 at2,0 on all peers. Root independently viewed committed bank and promoted plant. Artifacts work/spectator-promote-build9b/spectator_e461a0585844464b8073703f8c5fa3ba.
- NativeUI6/AvatarE2E6/Presentation7 Passed as recorded in SESSION. Previous failures remain historical, not current verification failures.

### New user scope (after baseline)

| ID | Acceptance | Owner | Status |
|---|---|---|---|
| MYSTERY-01 | Native ? toggle, locked secret placeholders through INTRO, deferred PLAYING roll/Imitator duplicate allowed, no taken marks, active mystery suppresses sunflower warning | UI + Root | IN_TESTING |
| MYSTERY-02 | Stable IDs, atomic reveal, bounded transactional protocol32/snapshot origin and recharge, latejoin and ownership persistence, packet-aware5–25% price | Entity + Modes + Root | IN_TESTING |
| DIFF-02 | D1 stock; hard D10; explicit policies for playable minigame/custom families, PvE HP invariant | Modes | IN_TESTING |
| PVP-03 | Smooth bounded losing-side help from D2, plant price/fire/sun and zombie price/speed/spawnHP, no instant heal/oscillation, cap stacked discounts | Modes + Root | IN_TESTING |
| UI-02 | Host Retry or Menu after GameOver closes the client modal and starts exactly one authoritative replacement board | Root + UI; Entity review | IN_PROGRESS |

### Protocol31/32 integration (01:11)

- Integration10c Passed, EXE0.30.18-f1ca23d74f-1794434410723, protocol31/unit1314/0/0. Mystery10c full Passed, path work/mystery-build10c/run_0ca2a1d193324c26b99bc486c9d936dd: real SDL toggle/cancel/fixed3, no sunflower warning with no public sun producers, stock Flyer confirmation, hidden5 cards throughINTRO, stable IDs/reveal, guaranteed overlap and Imitator duplicate from bounded test-only pool, price/spend75/75. Root independently viewed secret-frame; tooltip bottom clipping found and fixed in source.
- This pass also exposed hostrefresh750/client0, so protocol32 source now adds actual recharge metadata. Metadatafix standalone620/0; full game32 not built yet, positive mirrored cooldown/progress QA ready.
- Vasebreaker10b A5EAECF1, SlotMachine10b98D68DC8, Whack10b553E94D6, Adventure8Soak10b95C7970C whole Passed. Portal10b both runtimePassed but wholePSfailed missing hostcamera trace after new combined encoder; exact encoded coordinate trace restored and next repeat pending.
- Difficulty10c: D1 PvP fullPassed44commonhashes; D10 actual HP324/1320/noheal/speed1.1499, cost20/spend20 and plantsfire149→127/sun2494→2120/sky598→509 observed on host and client. WholeD10Failed chat rate-limited final ACK, fixture correction pending. Family wave budget40→100/256→3142 and native spawn HP270/1100/cadence2553→1873 observed; whole family attemptFailed Whack random cadence comparison, corrected source bound for next build. Remaining families Not executed.

### Current verification (01:40 Moscow)

- Integration12 Passed, build0.30.20-f1ca23d74f-1794436158762, protocol32, unit checks1327 failures0 skipped0. New feature sources remain uncommitted after published main f1ca23d74f.
- Mystery11 full D1 and D10 Passed: real SDL toggle/cancel/fixed3/mixed manual picks, no public taken marks, unknown cards through INTRO, stable IDs and atomic PLAYING reveal, duplicate Imitator/other-player overlap, no sunflower warning, actual price/spend48 and75. Native recharge duration/counter now agree on both peers (3000 atD1,750 atD10). Root viewed the game-rendered tooltip/secret/reveal/purchase frames. Artifacts work/mystery-build11-{1,10}.
- Difficulty11 Families:18 cases Passed across nine native families atD1/D10, including objectives, timed behavior and stock PvE HP. Root and Modes found three outdated goal HUD labels in actual BMPs; source fixed in12, dedicated image repeat pending. Native ICE remains unfinished/scripted; no unsupported mode was invented. This proves selected native paths, not complete campaigns.
- PvPOnly12 harness reported Passed, but subsequent log review found a HIGH protocol32 seed rejection after normal zombie purchase (RX66–67/applied9–10). Overall synchronization is therefore Failed, not clean Passed. D1 stock and D10 native effects/SDL spending/noheal/noncompound/pause/terminal subchecks were observed; source fix and stricter fail-on-reject runner are in progress. Artifacts work/difficulty-build12/run_fd640ebe67ae457ea12411bb4c463cc8. Controlled history is not a statistical balance study.
- GameOver12 red both Failed as expected: Retry and Menu each leave client DIALOG_GAME_OVER present. Actual stale game-only frame inspected. Root fixed KillBoard cleanup and EndLevel's duplicate construction/missing network start; next build/runtime pending. Artifact work/gameover-red-build12/run_873b60e881184b849d6c23da5728796f.
- Process ownership hardened: every new smoke launch writes owned-process.json with PID, exact executable and UTC start; close/kill verify all3 against a fresh process lookup. Four positive/negative identity checks Passed without closing any process. User's separate copy PID24464 is never stopped or captured. Total game-process limit3 includes that copy; three-owned observer suites wait until it exits.
- Still pending: GameOver green and keyboard actions; goal HUD repeat; Portal final camera trace repeat; protocol32 observer/bootstrap/kick metadata; malformed-start and RNG-recovery fault runtime. Cross-network NAT remains unverified.
- Additional blocker found01:40: protocol32 PvP snapshots reject a legal native seed state after buying zombies; entity_sync owns investigation/correction, Modes owns runner negative assertions. Do not publish a clean PvP verification claim until a repeat has no snapshot rejections.
- Families12 repeated18/18 Passed; work/difficulty-hud-build12/run_c55806911013462189f93ba486202373. Root independently viewed corrected goal frames120/120,0/1250 withtrophyprice1250,0/12. HUD fix VERIFIED.
- User steering01:44: PVP-DIGGER HIGH — tunneling miner must surface at column0 and walk back without a house score; UI-BRAINS — enlarge compact HUD brain icon. Modes owns production and native lifecycle regression, next integrated build waits for narrow patch readiness. User's PID24464 is still playing with a friend; do not close it.
